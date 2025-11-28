#include "raft/gRPCRaftClient.hpp"
#include <stdexcept>
#include <iostream>

void GrpcRaftClient::addPeer(uint64_t peerId, const std::string& address) {
    std::lock_guard<std::mutex> lock(stubsMutex);
    
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    stubs[peerId] = raft::RaftService::NewStub(channel);
    
    std::cout << "Added gRPC peer " << peerId << " at " << address << std::endl;
}

raft::RaftService::Stub* GrpcRaftClient::getStub(uint64_t peerId) {
    std::lock_guard<std::mutex> lock(stubsMutex);
    
    auto it = stubs.find(peerId);
    if (it == stubs.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::chrono::system_clock::time_point 
GrpcRaftClient::createDeadline(std::chrono::milliseconds timeout) {
    return std::chrono::system_clock::now() + timeout;
}

RequestVoteReply GrpcRaftClient::sendRequestVote(
    uint64_t peerId,
    const RequestVoteArgs& args,
    std::chrono::milliseconds timeout) {
    
    auto stub = getStub(peerId);
    if (!stub) {
        std::cerr << "No stub for peer " << peerId << std::endl;
        return RequestVoteReply{args.term, false};
    }
    
    // Convert C++ struct to protobuf
    raft::RequestVoteRequest request;
    request.set_term(args.term);
    request.set_candidate_id(args.candidateId);
    request.set_last_log_index(args.lastLogIndex);
    request.set_last_log_term(args.lastLogTerm);
    
    raft::RequestVoteResponse response;
    grpc::ClientContext context;
    context.set_deadline(createDeadline(timeout));
    
    // Make RPC call
    grpc::Status status = stub->RequestVote(&context, request, &response);
    
    if (!status.ok()) {
        std::cerr << "RequestVote RPC to peer " << peerId << " failed: " 
                  << status.error_message() << std::endl;
        return RequestVoteReply{args.term, false};
    }
    
    // Convert protobuf back to C++ struct
    RequestVoteReply reply;
    reply.term = response.term();
    reply.voteGranted = response.vote_granted();
    
    return reply;
}

AppendEntriesReply GrpcRaftClient::sendAppendEntries(
    uint64_t peerId,
    const AppendEntriesArgs& args,
    std::chrono::milliseconds timeout) {
    
    auto stub = getStub(peerId);
    if (!stub) {
        std::cerr << "No stub for peer " << peerId << std::endl;
        return AppendEntriesReply{args.term, false, 0, 0};
    }
    
    // Convert C++ struct to protobuf
    raft::AppendEntriesRequest request;
    request.set_term(args.term);
    request.set_leader_id(args.leaderId);
    request.set_prev_log_index(args.prevLogIndex);
    request.set_prev_log_term(args.prevLogTerm);
    request.set_leader_commit(args.leaderCommit);
    
    // Convert entries
    for (const auto& entry : args.entries) {
        auto* protoEntry = request.add_entries();
        protoEntry->set_index(entry.index);
        protoEntry->set_term(entry.term);
        protoEntry->set_command_data(entry.command_data.data(), 
                                     entry.command_data.size());
    }
    
    raft::AppendEntriesResponse response;
    grpc::ClientContext context;
    context.set_deadline(createDeadline(timeout));
    
    // Make RPC call
    grpc::Status status = stub->AppendEntries(&context, request, &response);
    
    if (!status.ok()) {
        std::cerr << "AppendEntries RPC to peer " << peerId << " failed: " 
                  << status.error_message() << std::endl;
        return AppendEntriesReply{args.term, false, 0, 0};
    }
    
    // Convert protobuf back to C++ struct
    AppendEntriesReply reply;
    reply.term = response.term();
    reply.success = response.success();
    reply.conflictIndex = response.conflict_index();
    reply.conflictTerm = response.conflict_term();
    
    return reply;
}

InstallSnapshotReply GrpcRaftClient::sendInstallSnapshot(
    uint64_t peerId,
    const InstallSnapshotArgs& args,
    std::chrono::milliseconds timeout) {
    
    auto stub = getStub(peerId);
    if (!stub) {
        std::cerr << "No stub for peer " << peerId << std::endl;
        return InstallSnapshotReply{args.term};
    }
    
    // Convert C++ struct to protobuf
    raft::InstallSnapshotRequest request;
    request.set_term(args.term);
    request.set_leader_id(args.leaderId);
    request.set_last_included_index(args.lastIncludedIndex);
    request.set_last_included_term(args.lastIncludedTerm);
    request.set_data(args.data.data(), args.data.size());
    
    raft::InstallSnapshotResponse response;
    grpc::ClientContext context;
    context.set_deadline(createDeadline(timeout));
    
    // Make RPC call
    grpc::Status status = stub->InstallSnapshot(&context, request, &response);
    
    if (!status.ok()) {
        std::cerr << "InstallSnapshot RPC to peer " << peerId << " failed: " 
                  << status.error_message() << std::endl;
        return InstallSnapshotReply{args.term};
    }
    
    // Convert protobuf back to C++ struct
    InstallSnapshotReply reply;
    reply.term = response.term();
    
    return reply;
}
