add_library(ShardKV_Core OBJECT
  kvstore/KVStore.cpp
  kvstore/Command.cpp
  raft/Log.cpp
  raft/RaftNode.cpp
  raft/InMemoryRPC.cpp
  raft/RaftPersistence.cpp
  raft/gRPCRaftService.cpp        
  raft/gRPCRaftClient.cpp
)

target_link_libraries(ShardKV_Core PUBLIC 
    ShardKV::Headers
    ShardKV_Generated   
)
