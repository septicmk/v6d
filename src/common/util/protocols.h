/** Copyright 2020-2021 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef SRC_COMMON_UTIL_PROTOCOLS_H_
#define SRC_COMMON_UTIL_PROTOCOLS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/memory/payload.h"
#include "common/util/boost.h"
#include "common/util/json.h"
#include "common/util/status.h"
#include "common/util/uuid.h"

namespace vineyard {

enum class CommandType {
  DebugCommand = -1,
  NullCommand = 0,
  ExitRequest = 1,
  ExitReply = 2,
  RegisterRequest = 3,
  RegisterReply = 4,
  GetDataRequest = 5,
  GetDataReply = 6,
  PersistRequest = 8,
  ExistsRequest = 9,
  DelDataRequest = 10,
  ClusterMetaRequest = 11,
  ListDataRequest = 12,
  CreateBufferRequest = 13,
  GetBuffersRequest = 14,
  CreateDataRequest = 15,
  PutNameRequest = 16,
  GetNameRequest = 17,
  DropNameRequest = 18,
  CreateStreamRequest = 19,
  GetNextStreamChunkRequest = 20,
  PullNextStreamChunkRequest = 21,
  StopStreamRequest = 22,
  IfPersistRequest = 25,
  InstanceStatusRequest = 26,
  ShallowCopyRequest = 27,
  OpenStreamRequest = 28,
  MigrateObjectRequest = 29,
  CreateRemoteBufferRequest = 30,
  GetRemoteBuffersRequest = 31,
  DropBufferRequest = 32,
  MakeArenaRequest = 33,
  FinalizeArenaRequest = 34,
  DeepCopyRequest = 35,
  ClearRequest = 36,
  PushNextStreamChunkRequest = 37,
  GetBuffersByExternalRequest = 38,
  ModifyReferenceCountRequest = 39,
  ModifyReferenceCountReply = 40,
};

CommandType ParseCommandType(const std::string& str_type);

void WriteErrorReply(Status const& status, std::string& msg);

void WriteRegisterRequest(std::string& msg);

Status ReadRegisterRequest(const json& msg, std::string& version);

void WriteRegisterReply(const std::string& ipc_socket,
                        const std::string& rpc_endpoint,
                        const InstanceID instance_id, std::string& msg);

Status ReadRegisterReply(const json& msg, std::string& ipc_socket,
                         std::string& rpc_endpoint, InstanceID& instance_id,
                         std::string& version);

void WriteExitRequest(std::string& msg);

void WriteGetDataRequest(const ObjectID id, const bool sync_remote,
                         const bool wait, std::string& msg);

void WriteGetDataRequest(const std::vector<ObjectID>& ids,
                         const bool sync_remote, const bool wait,
                         std::string& msg);

Status ReadGetDataRequest(const json& root, std::vector<ObjectID>& ids,
                          bool& sync_remote, bool& wait);

void WriteGetDataReply(const json& content, std::string& msg);

Status ReadGetDataReply(const json& root, json& content);

Status ReadGetDataReply(const json& root,
                        std::unordered_map<ObjectID, json>& content);

void WriteListDataRequest(std::string const& pattern, bool const regex,
                          size_t const limit, std::string& msg);

Status ReadListDataRequest(const json& root, std::string& pattern, bool& regex,
                           size_t& limit);

void WriteCreateDataRequest(const json& content, std::string& msg);

Status ReadCreateDataRequest(const json& root, json& content);

void WriteCreateDataReply(const ObjectID& id, const Signature& sigature,
                          const InstanceID& instance_id, std::string& msg);

Status ReadCreateDataReply(const json& root, ObjectID& id, Signature& sigature,
                           InstanceID& instance_id);

void WritePersistRequest(const ObjectID id, std::string& msg);

Status ReadPersistRequest(const json& root, ObjectID& id);

void WritePersistReply(std::string& msg);

Status ReadPersistReply(const json& root);

void WriteIfPersistRequest(const ObjectID id, std::string& msg);

Status ReadIfPersistRequest(const json& root, ObjectID& id);

void WriteIfPersistReply(bool exists, std::string& msg);

Status ReadIfPersistReply(const json& root, bool& persist);

void WriteExistsRequest(const ObjectID id, std::string& msg);

Status ReadExistsRequest(const json& root, ObjectID& id);

void WriteExistsReply(bool exists, std::string& msg);

Status ReadExistsReply(const json& root, bool& exists);

void WriteDelDataRequest(const ObjectID id, const bool force, const bool deep,
                         const bool fastpath, std::string& msg);

void WriteDelDataRequest(const std::vector<ObjectID>& id, const bool force,
                         const bool deep, const bool fastpath,
                         std::string& msg);

Status ReadDelDataRequest(const json& root, std::vector<ObjectID>& id,
                          bool& force, bool& deep, bool& fastpath);

void WriteDelDataReply(std::string& msg);

Status ReadDelDataReply(const json& root);

void WriteClusterMetaRequest(std::string& msg);

Status ReadClusterMetaRequest(const json& root);

void WriteClusterMetaReply(const json& content, std::string& msg);

Status ReadClusterMetaReply(const json& root, json& content);

void WriteInstanceStatusRequest(std::string& msg);

Status ReadInstanceStatusRequest(const json& root);

void WriteInstanceStatusReply(const json& content, std::string& msg);

Status ReadInstanceStatusReply(const json& root, json& content);

void WriteCreateBufferRequest(const size_t size, const ExternalID external_id,
                              const size_t external_size, std::string& msg);

Status ReadCreateBufferRequest(const json& root, size_t& size,
                               ExternalID& external_id, size_t& external_size);

void WriteCreateBufferReply(const ObjectID id,
                            const std::shared_ptr<Payload>& object,
                            std::string& msg);

Status ReadCreateBufferReply(const json& root, ObjectID& id, Payload& object);

void WriteCreateRemoteBufferRequest(const size_t size, std::string& msg);

Status ReadCreateRemoteBufferRequest(const json& root, size_t& size);

void WriteGetBuffersRequest(const std::set<ObjectID>& ids, std::string& msg);

Status ReadGetBuffersRequest(const json& root, std::vector<ObjectID>& ids);

void WriteGetBuffersReply(const std::vector<std::shared_ptr<Payload>>& objects,
                          std::string& msg);

Status ReadGetBuffersReply(const json& root, std::vector<Payload>& objects);

void WriteGetBuffersByExternalRequest(const std::set<ExternalID>& eids,
                                      std::string& msg);

Status ReadGetBuffersByExternalRequest(const json& root,
                                       std::vector<ExternalID>& ids);

void WriteGetRemoteBuffersRequest(const std::unordered_set<ObjectID>& ids,
                                  std::string& msg);

Status ReadGetRemoteBuffersRequest(const json& root,
                                   std::vector<ObjectID>& ids);

void WriteDropBufferRequest(const ObjectID id, std::string& msg);

Status ReadDropBufferRequest(const json& root, ObjectID& id);

void WriteDropBufferReply(std::string& msg);

Status ReadDropBufferReply(const json& root);

void WritePutNameRequest(const ObjectID object_id, const std::string& name,
                         std::string& msg);

Status ReadPutNameRequest(const json& root, ObjectID& object_id,
                          std::string& name);

void WritePutNameReply(std::string& msg);

Status ReadPutNameReply(const json& root);

void WriteGetNameRequest(const std::string& name, const bool wait,
                         std::string& msg);

Status ReadGetNameRequest(const json& root, std::string& name, bool& wait);

void WriteGetNameReply(const ObjectID& object_id, std::string& msg);

Status ReadGetNameReply(const json& root, ObjectID& object_id);

void WriteDropNameRequest(const std::string& name, std::string& msg);

Status ReadDropNameRequest(const json& root, std::string& name);

void WriteDropNameReply(std::string& msg);

Status ReadDropNameReply(const json& root);

void WriteMigrateObjectRequest(const ObjectID object_id, const bool local,
                               const bool is_stream, std::string const& peer,
                               std::string const& peer_rpc_endpoint,
                               std::string& msg);

Status ReadMigrateObjectRequest(const json& root, ObjectID& object_id,
                                bool& local, bool& is_stream, std::string& peer,
                                std::string& peer_rpc_endpoint);

void WriteMigrateObjectReply(const ObjectID& object_id, std::string& msg);

Status ReadMigrateObjectReply(const json& root, ObjectID& object_id);

void WriteCreateStreamRequest(const ObjectID& object_id, std::string& msg);

Status ReadCreateStreamRequest(const json& root, ObjectID& object_id);

void WriteCreateStreamReply(std::string& msg);

Status ReadCreateStreamReply(const json& root);

void WriteOpenStreamRequest(const ObjectID& object_id, const int64_t& mode,
                            std::string& msg);

Status ReadOpenStreamRequest(const json& root, ObjectID& object_id,
                             int64_t& mode);

void WriteOpenStreamReply(std::string& msg);

Status ReadOpenStreamReply(const json& root);

void WriteGetNextStreamChunkRequest(const ObjectID stream_id, const size_t size,
                                    std::string& msg);

Status ReadGetNextStreamChunkRequest(const json& root, ObjectID& stream_id,
                                     size_t& size);

void WriteGetNextStreamChunkReply(std::shared_ptr<Payload>& object,
                                  std::string& msg);

Status ReadGetNextStreamChunkReply(const json& root, Payload& object);

void WritePushNextStreamChunkRequest(const ObjectID stream_id,
                                     const ObjectID chunk, std::string& msg);

Status ReadPushNextStreamChunkRequest(const json& root, ObjectID& stream_id,
                                      ObjectID& chunk);

void WritePushNextStreamChunkReply(std::string& msg);

Status ReadPushNextStreamChunkReply(const json& root);

void WritePullNextStreamChunkRequest(const ObjectID stream_id,
                                     std::string& msg);

Status ReadPullNextStreamChunkRequest(const json& root, ObjectID& stream_id);

void WritePullNextStreamChunkReply(ObjectID const chunk, std::string& msg);

Status ReadPullNextStreamChunkReply(const json& root, ObjectID& chunk);

void WriteStopStreamRequest(const ObjectID stream_id, const bool failed,
                            std::string& msg);

Status ReadStopStreamRequest(const json& root, ObjectID& stream_id,
                             bool& failed);

void WriteStopStreamReply(std::string& msg);

Status ReadStopStreamReply(const json& root);

void WriteShallowCopyRequest(const ObjectID id, std::string& msg);

void WriteShallowCopyRequest(const ObjectID id, json const& extra_metadata,
                             std::string& msg);

Status ReadShallowCopyRequest(const json& root, ObjectID& id,
                              json& extra_metadata);

void WriteShallowCopyReply(const ObjectID target_id, std::string& msg);

Status ReadShallowCopyReply(const json& root, ObjectID& target_id);

void WriteDeepCopyRequest(const ObjectID object_id, std::string const& peer,
                          std::string const& peer_rpc_endpoint,
                          std::string& msg);

Status ReadDeepCopyRequest(const json& root, ObjectID& object_id,
                           std::string& peer, std::string& peer_rpc_endpoint);

void WriteDeepCopyReply(const ObjectID& object_id, std::string& msg);

Status ReadDeepCopyReply(const json& root, ObjectID& object_id);

void WriteMakeArenaRequest(const size_t size, std::string& msg);

Status ReadMakeArenaRequest(const json& root, size_t& size);

void WriteMakeArenaReply(const int fd, const size_t size, const uintptr_t base,
                         std::string& msg);

Status ReadMakeArenaReply(const json& root, int& fd, size_t& size,
                          uintptr_t& base);

void WriteFinalizeArenaRequest(const int fd, std::vector<size_t> const& offsets,
                               std::vector<size_t> const& sizes,
                               std::string& msg);

Status ReadFinalizeArenaRequest(const json& root, int& fd,
                                std::vector<size_t>& offsets,
                                std::vector<size_t>& sizes);

void WriteFinalizeArenaReply(std::string& msg);

Status ReadFinalizeArenaReply(const json& root);

void WriteClearRequest(std::string& msg);

Status ReadClearRequest(const json& root);

void WriteClearReply(std::string& msg);

Status ReadClearReply(const json& root);

void WriteDebugRequest(const json& debug, std::string& msg);

Status ReadDebugRequest(const json& root, json& debug);

void WriteDebugReply(const json& result, std::string& msg);

Status ReadDebugReply(const json& root, json& result);

void WriteModifyReferenceCountRequest(ExternalID eid, int changes, std::string& msg);

Status ReadModifyReferenceCountRequest(const json& result, ExternalID& eid, int& changes);

void WriteModifyReferenceCountReply(std::string& msg);

Status ReadModifyReferenceCountReply(const json& result);
}  // namespace vineyard

#endif  // SRC_COMMON_UTIL_PROTOCOLS_H_
