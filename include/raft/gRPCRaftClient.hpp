#ifndef GRPC_RAFT_CLIENT_HPP
#define GRPC_RAFT_CLIENT_HPP

#include "RaftRPC.hpp"
#include "raft.pb.h"
#include "raft.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <map>
#include <memory>
#include <string>
#include <mutex>

class GrpcRaftClient : public RaftRPC {
public:
    GrpcRaftClient() = default;
    
    // Register peer addresses
    void addPeer(uint64_t peerId, const std::string& address);
    
    // RaftRPC Methods implementation
    RequestVoteReply sendRequestVote(
        uint64_t peerId,
        const RequestVoteArgs& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) override;
    
    AppendEntriesReply sendAppendEntries(
        uint64_t peerId,
        const AppendEntriesArgs& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) override;
    
    InstallSnapshotReply sendInstallSnapshot(
        uint64_t peerId,
        const InstallSnapshotArgs& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)
    ) override;
    
private:
    std::map<uint64_t, std::unique_ptr<raft::RaftService::Stub>> stubs;
    std::mutex stubsMutex;
    
    // Get/Create stub for peer
    raft::RaftService::Stub* getStub(uint64_t peerId);
    
    // Helpers 
    std::chrono::system_clock::time_point 
    createDeadline(std::chrono::milliseconds timeout);
};

#endif // GRPC_RAFT_CLIENT_HPP
