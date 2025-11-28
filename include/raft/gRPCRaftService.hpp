#ifndef GRPC_RAFT_SERVICE_HPP
#define GRPC_RAFT_SERVICE_HPP

#include "RaftNode.hpp"
#include "raft.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <memory>

class GrpcRaftService final : public raft::RaftService::Service {
public:
    explicit GrpcRaftService(RaftNode* node);
    
    grpc::Status RequestVote(
        grpc::ServerContext* context,
        const raft::RequestVoteRequest* request,
        raft::RequestVoteResponse* response) override;
    
    grpc::Status AppendEntries(
        grpc::ServerContext* context,
        const raft::AppendEntriesRequest* request,
        raft::AppendEntriesResponse* response) override;
    
    grpc::Status InstallSnapshot(
        grpc::ServerContext* context,
        const raft::InstallSnapshotRequest* request,
        raft::InstallSnapshotResponse* response) override;
    
private:
    RaftNode* node;
};

// Server 
class GrpcRaftServer {
public:
    GrpcRaftServer(RaftNode* node, const std::string& address);
    ~GrpcRaftServer();
    
    void start();
    void stop();
    
private:
    std::unique_ptr<grpc::Server> server;
    std::unique_ptr<GrpcRaftService> service;
};

#endif // GRPC_RAFT_SERVICE_HPP
