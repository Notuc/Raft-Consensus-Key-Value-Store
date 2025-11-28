#include "raft/InMemoryRPC.hpp"
#include "raft/RaftNode.hpp"
#include <stdexcept>

void InMemoryRPC::registerNode(uint64_t nodeId, RaftNode* node) {
    std::lock_guard<std::mutex> lock(rpcMutex);
    nodes[nodeId] = node;
}

void InMemoryRPC::unregisterNode(uint64_t nodeId) {
    std::lock_guard<std::mutex> lock(rpcMutex);
    nodes.erase(nodeId);
}

bool InMemoryRPC::isReachable(uint64_t fromNodeId, uint64_t toNodeId) const {
    std::lock_guard<std::mutex> lock(rpcMutex);
    
    auto key1 = std::make_pair(fromNodeId, toNodeId);
    auto key2 = std::make_pair(toNodeId, fromNodeId);
    
    auto it1 = partitions.find(key1);
    if (it1 != partitions.end() && it1->second) {
        return false;     }
    
    auto it2 = partitions.find(key2);
    if (it2 != partitions.end() && it2->second) {
        return false; 
    }
    
    return true; 
}

void InMemoryRPC::setPartition(uint64_t node1, uint64_t node2, bool partitioned) {
    std::lock_guard<std::mutex> lock(rpcMutex);
    partitions[std::make_pair(node1, node2)] = partitioned;
    partitions[std::make_pair(node2, node1)] = partitioned;
}

RequestVoteReply InMemoryRPC::sendRequestVote(
    uint64_t peerId,
    const RequestVoteArgs& args,
    std::chrono::milliseconds timeout
) {
    std::lock_guard<std::mutex> lock(rpcMutex);
    
    // Check if peer exists
    auto it = nodes.find(peerId);
    if (it == nodes.end()) {
        // Peer not found - return failure
        RequestVoteReply reply;
        reply.term = 0;
        reply.voteGranted = false;
        return reply;
    }
    
    // Call the handler directly (in-memory)
    return it->second->handleRequestVote(args);
}

AppendEntriesReply InMemoryRPC::sendAppendEntries(
    uint64_t peerId,
    const AppendEntriesArgs& args,
    std::chrono::milliseconds timeout
) {
    std::lock_guard<std::mutex> lock(rpcMutex);
    
    // Check if peer exists
    auto it = nodes.find(peerId);
    if (it == nodes.end()) {
        // Peer not found - return failure
        AppendEntriesReply reply;
        reply.term = 0;
        reply.success = false;
        reply.conflictIndex = 0;
        reply.conflictTerm = 0;
        return reply;
    }
    
    // Call the handler directly (in-memory)
    return it->second->handleAppendEntries(args);
}

InstallSnapshotReply InMemoryRPC::sendInstallSnapshot(
    uint64_t peerId,
    const InstallSnapshotArgs& args,
    std::chrono::milliseconds timeout
) {
    std::lock_guard<std::mutex> lock(rpcMutex);
    
    auto it = nodes.find(peerId);
    if (it == nodes.end()) {
        InstallSnapshotReply reply;
        reply.term = 0;
        return reply;
    }
    
    return it->second->handleInstallSnapshot(args);
}
