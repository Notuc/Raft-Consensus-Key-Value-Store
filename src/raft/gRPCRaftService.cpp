#include "raft/gRPCRaftService.hpp"

GrpcRaftService::GrpcRaftService(RaftNode* node) : node(node) {}

grpc::Status GrpcRaftService::RequestVote(
    grpc::ServerContext* context,
    const raft::RequestVoteRequest* request,
    raft::RequestVoteResponse* response) {
    
    // Convert protobuf to C++ struct
    RequestVoteArgs args;
    args.term = request->term();
    args.candidateId = request->candidate_id();
    args.lastLogIndex = request->last_log_index();
    args.lastLogTerm = request->last_log_term();
    
    // Call RaftNode handler
    RequestVoteReply reply = node->handleRequestVote(args);
    
    // Convert back to protobuf
    response->set_term(reply.term);
    response->set_vote_granted(reply.voteGranted);
    
    return grpc::Status::OK;
}

grpc::Status GrpcRaftService::AppendEntries(
    grpc::ServerContext* context,
    const raft::AppendEntriesRequest* request,
    raft::AppendEntriesResponse* response) {
    
    // Convert protobuf to C++ struct
    AppendEntriesArgs args;
    args.term = request->term();
    args.leaderId = request->leader_id();
    args.prevLogIndex = request->prev_log_index();
    args.prevLogTerm = request->prev_log_term();
    args.leaderCommit = request->leader_commit();
    
    // Convert entries
    for (const auto& protoEntry : request->entries()) {
        LogEntry entry;
        entry.index = protoEntry.index();
        entry.term = protoEntry.term();
        entry.command_data.assign(protoEntry.command_data().begin(),
                                  protoEntry.command_data().end());
        args.entries.push_back(entry);
    }
    
    // Call RaftNode handler
    AppendEntriesReply reply = node->handleAppendEntries(args);
    
    // Convert back to protobuf
    response->set_term(reply.term);
    response->set_success(reply.success);
    response->set_conflict_index(reply.conflictIndex);
    response->set_conflict_term(reply.conflictTerm);
    
    return grpc::Status::OK;
}

grpc::Status GrpcRaftService::InstallSnapshot(
    grpc::ServerContext* context,
    const raft::InstallSnapshotRequest* request,
    raft::InstallSnapshotResponse* response) {
    
    // Convert protobuf to C++ struct
    InstallSnapshotArgs args;
    args.term = request->term();
    args.leaderId = request->leader_id();
    args.lastIncludedIndex = request->last_included_index();
    args.lastIncludedTerm = request->last_included_term();
    args.data.assign(request->data().begin(), request->data().end());
    
    // Call RaftNode handler
    InstallSnapshotReply reply = node->handleInstallSnapshot(args);
    
    // Convert back to protobuf
    response->set_term(reply.term);
    
    return grpc::Status::OK;
}

// Server implementation
GrpcRaftServer::GrpcRaftServer(RaftNode* node, const std::string& address) {
    service = std::make_unique<GrpcRaftService>(node);
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    
    server = builder.BuildAndStart();
}

GrpcRaftServer::~GrpcRaftServer() {
    stop();
}

void GrpcRaftServer::start() {
    // Left blank for now because server is already started in constructor
}

void GrpcRaftServer::stop() {
    if (server) {
        server->Shutdown();
    }
}
