#ifndef IN_MEMORY_RPC_HPP
#define IN_MEMORY_RPC_HPP

#include "RaftRPC.hpp"
#include "RaftNode.hpp"
#include <map>
#include <mutex>


class RaftNode; 

// In-memory RPC implementation for testing 
// Nodes run in the same process
class InMemoryRPC : public RaftRPC {
public:
    InMemoryRPC() = default;
    
    // Register a node so RPCs can be routed to it
    void registerNode(uint64_t nodeId, RaftNode* node);
    
    // Unregister (For Testing)
    void unregisterNode(uint64_t nodeId);
    
    // Check if a node is reachable 
    bool isReachable(uint64_t fromNodeId, uint64_t toNodeId) const;
    
    // Simulate network partition
    void setPartition(uint64_t node1, uint64_t node2, bool partitioned);
    
    // RaftRPC Method implementation
    RequestVoteReply sendRequestVote(
        uint64_t peerId,
        const RequestVoteArgs& args,
        std::chrono::milliseconds timeout
    ) override;
    
    AppendEntriesReply sendAppendEntries(
        uint64_t peerId,
        const AppendEntriesArgs& args,
        std::chrono::milliseconds timeout
    ) override;

    // Snapshot
    InstallSnapshotReply sendInstallSnapshot(
        uint64_t peerId,
        const InstallSnapshotArgs& args,
        std::chrono::milliseconds timeout
    ) override;
    
private:
    std::map<uint64_t, RaftNode*> nodes;
    std::map<std::pair<uint64_t, uint64_t>, bool> partitions;
    mutable std::mutex rpcMutex;
};

#endif // IN_MEMORY_RPC_HPP
