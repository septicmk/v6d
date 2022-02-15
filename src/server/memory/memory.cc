/**
 * NOLINT(legal/copyright)
 *
 * The file src/server/memory/memory.cc is partially referred and derived
 * from project apache-arrow,
 *
 *    https://github.com/apache/arrow/blob/master/cpp/src/plasma/memory.cc
 *
 * which has the following license:
 *
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
 */

#include "server/memory/memory.h"

#include <sys/mman.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "common/util/logging.h"
#include "server/memory/allocator.h"
#include "server/memory/malloc.h"

namespace vineyard {

using memory::GetMallocMapinfo;
using memory::kBlockSize;

namespace memory {

static inline size_t system_page_size() {
  return (size_t) sysconf(_SC_PAGESIZE);
}

static inline uintptr_t align_up(const uintptr_t address,
                                 const size_t alignment) {
  return (address + alignment - 1) & ~(alignment - 1);
}

static inline uintptr_t align_down(const uintptr_t address,
                                   const size_t alignment) {
  return address & ~(alignment - 1);
}

static inline void recycle_resident_memory(const uintptr_t aligned_left,
                                           const uintptr_t aligned_right) {
  if (aligned_left != aligned_right) {
    /**
     * Notes [Recycle Pages with madvise]:
     *
     * 1. madvise(.., MADV_FREE) cannot be used for shared memory, thus we use
     * `MADV_DONTNEED`.
     * 2. madvise(...) requires alignment to PAGE size.
     *
     * See also: https://man7.org/linux/man-pages/man2/madvise.2.html
     */
    if (madvise(reinterpret_cast<void*>(aligned_left),
                aligned_right - aligned_left, MADV_DONTNEED)) {
      LOG(ERROR) << "madvise: " << errno << " -> " << strerror(errno);
    }
  }
}

static inline void recycle_resident_memory(const uintptr_t base, size_t left,
                                           size_t right) {
  static size_t page_size = system_page_size();
  uintptr_t aligned_left = align_up(base + left, page_size),
            aligned_right = align_down(base + right, page_size);
  DVLOG(10) << "recycle memory: " << reinterpret_cast<void*>(base + left) << "("
            << reinterpret_cast<void*>(aligned_left) << ") to "
            << reinterpret_cast<void*>(base + right) << "("
            << reinterpret_cast<void*>(aligned_right) << ")";
  recycle_resident_memory(aligned_left, aligned_right);
}

/**
 * @brief Find non-covered intervals, and release the memory back to OS kernel.
 *
 * n.b.: the intervals may overlap.
 */
static void recycle_arena(const uintptr_t base, const size_t size,
                          std::vector<size_t> const& offsets,
                          std::vector<size_t> const& sizes) {
  std::map<size_t, int32_t> points;
  points[0] = 0;
  points[size] = 0;
  for (size_t idx = 0; idx < offsets.size(); ++idx) {
    points[offsets[idx]] += 1;
    points[offsets[idx] + sizes[idx]] -= 1;
  }
  auto head = points.begin();
  int markersum = 0;
  while (true) {
    markersum += head->second;
    auto next = std::next(head);
    if (next == points.end()) {
      break;
    }
    if (markersum == 0) {
      // release memory in the untouched interval.
      recycle_resident_memory(base, head->first, next->first);
    }
    head = next;
  }
}
}  // namespace memory

std::set<ObjectID> BulkStore::Arena::spans{};

BulkStore::~BulkStore() {
  std::vector<ObjectID> object_ids;
  object_ids.reserve(objects_.size());
  for (auto iter = objects_.begin(); iter != objects_.end(); iter++) {
    object_ids.emplace_back(iter->first);
  }
  for (auto const& item : object_ids) {
    VINEYARD_DISCARD(Delete(item));
  }
}

Status BulkStore::PreAllocate(const size_t size) {
  BulkAllocator::SetFootprintLimit(size);
  void* pointer = BulkAllocator::Init(size);

  if (pointer == nullptr) {
    return Status::NotEnoughMemory("mmap failed, size = " +
                                   std::to_string(size));
  }

  // insert a special marker for obtaining the whole shared memory range
  ObjectID object_id = GenerateBlobID(
      reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max()));
  int fd = -1;
  int64_t map_size = 0;
  ptrdiff_t offset = 0;
  GetMallocMapinfo(pointer, &fd, &map_size, &offset);
  objects_.emplace(
      object_id,
      std::make_shared<Payload>(object_id, size, static_cast<uint8_t*>(pointer),
                                fd, map_size, offset));
  return Status::OK();
}

// Allocate memory
uint8_t* BulkStore::AllocateMemory(size_t size, int* fd, int64_t* map_size,
                                   ptrdiff_t* offset) {
  // Try to evict objects until there is enough space.
  uint8_t* pointer = nullptr;
  pointer =
      reinterpret_cast<uint8_t*>(BulkAllocator::Memalign(size, kBlockSize));
  if (pointer) {
    GetMallocMapinfo(pointer, fd, map_size, offset);
  }
  return pointer;
}

Status BulkStore::Create(const size_t data_size, ObjectID& object_id,
                         std::shared_ptr<Payload>& object) {
  return Create(data_size, {}, 0, object_id, object);
}

Status BulkStore::Create(const size_t data_size, const ExternalID external_id,
                         const size_t external_size, ObjectID& object_id,
                         std::shared_ptr<Payload>& object) {
  if (data_size == 0) {
    object_id = EmptyBlobID();
    object = Payload::MakeEmpty();
    return Status::OK();
  }
  int fd = -1;
  int64_t map_size = 0;
  ptrdiff_t offset = 0;
  uint8_t* pointer = nullptr;
  pointer = AllocateMemory(data_size, &fd, &map_size, &offset);
  if (pointer == nullptr) {
    return Status::NotEnoughMemory("size = " + std::to_string(data_size));
  }
  object_id = GenerateBlobID(pointer);
  object = std::make_shared<Payload>(object_id, external_id, data_size, pointer,
                                     fd, map_size, offset, external_size);
  objects_.emplace(object_id, object);
  externals_.emplace(external_id, object);
  DVLOG(10) << "after allocate: " << ObjectIDToString(object_id) << ": "
            << Footprint() << "(" << FootprintLimit() << ")";
  return Status::OK();
}

Status BulkStore::Get(const ObjectID id, std::shared_ptr<Payload>& object) {
  if (id == EmptyBlobID()) {
    object = Payload::MakeEmpty();
    return Status::OK();
  } else {
    object_map_t::const_accessor accessor;
    if (objects_.find(accessor, id)) {
      object = accessor->second;
      return Status::OK();
    } else {
      return Status::ObjectNotExists("get: id = " + ObjectIDToString(id));
    }
  }
}

Status BulkStore::Get(const std::vector<ObjectID>& ids,
                      std::vector<std::shared_ptr<Payload>>& objects) {
  for (auto object_id : ids) {
    if (object_id == EmptyBlobID()) {
      objects.push_back(Payload::MakeEmpty());
    } else {
      object_map_t::const_accessor accessor;
      if (objects_.find(accessor, object_id)) {
        objects.push_back(accessor->second);
      }
    }
  }
  return Status::OK();
}

Status BulkStore::Get(const std::vector<ExternalID>& eids,
                      std::vector<std::shared_ptr<Payload>>& objects) {
  for (auto object_id : eids) {
    external_map_t::const_accessor accessor;
    if (externals_.find(accessor, object_id)) {
      objects.push_back(accessor->second);
    }
  }
  return Status::OK();
}

Status BulkStore::Delete(const ExternalID& external_id) {
  external_map_t::const_accessor external_accessor;
  if (externals_.find(external_accessor, external_id)) {
    auto& object = external_accessor->second;
    auto object_id = object->object_id;
    return Delete(object_id);
  }
  return Status::OK();
}

Status BulkStore::Delete(const ObjectID& object_id) {
  // see also: BulkStore::PreAllocate().
  if (object_id == EmptyBlobID() ||
      object_id == GenerateBlobID(reinterpret_cast<void*>(
                       std::numeric_limits<uintptr_t>::max()))) {
    return Status::OK();
  }
  object_map_t::const_accessor accessor;
  if (!objects_.find(accessor, object_id)) {
    return Status::ObjectNotExists("delete: id = " +
                                   ObjectIDToString(object_id));
  }
  auto& object = accessor->second;
  auto external_id = object->external_id;
  if (object->arena_fd == -1) {
    auto buff_size = object->data_size;
    BulkAllocator::Free(object->pointer, buff_size);
    DVLOG(10) << "after free: " << ObjectIDToString(object_id) << ": "
              << Footprint() << "(" << FootprintLimit() << ")";
  } else {
    static size_t page_size = memory::system_page_size();
    uintptr_t pointer = reinterpret_cast<uintptr_t>(object->pointer);
    uintptr_t lower = memory::align_down(pointer, page_size),
              upper = memory::align_up(pointer, page_size);
    uintptr_t lower_bound = lower, upper_bound = upper;
    {
      auto iter = Arena::spans.find(object_id);
      if (iter != Arena::spans.begin()) {
        auto iter_prev = std::prev(iter);
        object_map_t::const_accessor accessor;
        if (!objects_.find(accessor, *iter_prev)) {
          return Status::Invalid(
              "Internal state error: previous blob not found");
        }
        auto& object_prev = accessor->second;
        lower_bound =
            memory::align_up(reinterpret_cast<uintptr_t>(object_prev->pointer) +
                                 object_prev->data_size,
                             page_size);
      }
      auto iter_next = std::next(iter);
      if (iter_next != Arena::spans.end()) {
        object_map_t::const_accessor accessor;
        if (!objects_.find(accessor, *iter_next)) {
          return Status::Invalid("Internal state error: next blob not found");
        }
        auto& object_next = accessor->second;
        upper_bound = memory::align_down(
            reinterpret_cast<uintptr_t>(object_next->pointer), page_size);
      }
    }
    if (std::max(lower, lower_bound) < std::min(upper, upper_bound)) {
      DVLOG(10) << "after free: " << Footprint() << "(" << FootprintLimit()
                << "), recycle: (" << std::max(lower, lower_bound) << ", "
                << std::min(upper, upper_bound) << ")";
      memory::recycle_resident_memory(std::max(lower, lower_bound),
                                      std::min(upper, upper_bound));
    }
  }
  external_map_t::const_accessor external_accessor;
  if (externals_.find(external_accessor, external_id)) {
    externals_.erase(external_accessor);
  }
  objects_.erase(accessor);
  return Status::OK();
}

bool BulkStore::Exists(const ObjectID& object_id) {
  object_map_t::const_accessor accessor;
  return objects_.find(accessor, object_id);
}

bool BulkStore::Exists(const ExternalID& object_id) {
  external_map_t::const_accessor accessor;
  return externals_.find(accessor, object_id);
}

size_t BulkStore::Footprint() const { return BulkAllocator::Allocated(); }

size_t BulkStore::FootprintLimit() const {
  return BulkAllocator::GetFootprintLimit();
}

Status BulkStore::MakeArena(size_t const size, int& fd, uintptr_t& base) {
  fd = memory::create_buffer(size);
  if (fd == -1) {
    return Status::NotEnoughMemory("Failed to allocate a new arena");
  }
  void* space = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  base = reinterpret_cast<uintptr_t>(space);
  arenas_.emplace(fd, Arena{.fd = fd,
                            .size = size,
                            .base = reinterpret_cast<uintptr_t>(space)});
  return Status::OK();
}

Status BulkStore::FinalizeArena(const int fd,
                                std::vector<size_t> const& offsets,
                                std::vector<size_t> const& sizes) {
  VLOG(2) << "finalizing arena (fd) " << fd << "...";
  auto arena = arenas_.find(fd);
  if (arena == arenas_.end()) {
    return Status::ObjectNotExists("arena for fd " + std::to_string(fd) +
                                   " cannot be found");
  }
  if (offsets.size() != sizes.size()) {
    return Status::UserInputError(
        "The offsets and sizes of sealed blobs are not match");
  }
  size_t mmap_size = arena->second.size;
  uintptr_t mmap_base = arena->second.base;
  for (size_t idx = 0; idx < offsets.size(); ++idx) {
    VLOG(2) << "blob in use: in " << fd << ", at " << offsets[idx]
            << " of size " << sizes[idx];
    // make them available for blob pool
    uintptr_t pointer = mmap_base + offsets[idx];
    ObjectID object_id = GenerateBlobID(pointer);
    objects_.emplace(object_id, std::make_shared<Payload>(
                                    object_id, sizes[idx],
                                    reinterpret_cast<uint8_t*>(pointer), fd,
                                    mmap_size, offsets[idx]));
    // record the span, will be used to release memory back to OS when deleting
    // blobs
    Arena::spans.emplace(object_id);
  }
  // recycle memory
  { memory::recycle_arena(mmap_base, mmap_size, offsets, sizes); }
  // make it available for mmap record
  {
    memory::MmapRecord& record =
        memory::mmap_records[reinterpret_cast<void*>(mmap_base)];
    record.fd = fd;
    record.size = mmap_size;
    arenas_.erase(arena);
  }
  return Status::OK();
}

}  // namespace vineyard
