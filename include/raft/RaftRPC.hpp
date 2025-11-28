#ifndef RAFT_RPC_HPP
#define RAFT_RPC_HPP

#include <memory>
#include <chrono>

// Forward declarations 
class RaftNode;

struct RequestVoteArgs;
struct RequestVoteReply;
struct AppendEntriesArgs;
struct AppendEntriesReply;
struct InstallSnapshotArgs;      
struct InstallSnapshotReply;     

// Abstract RPC interface
class RaftRPC {
public:
    virtual ~RaftRPC() = default;
    
    virtual RequestVoteReply sendRequestVote(
        uint64_t peerId,
        const RequestVoteArgs& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) = 0;
    
    virtual AppendEntriesReply sendAppendEntries(
        uint64_t peerId,
        const AppendEntriesArgs& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(100)
    ) = 0;

   // Snapshot RPC
    virtual InstallSnapshotReply sendInstallSnapshot(
        uint64_t peerId,
        const InstallSnapshotArgs& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)
    ) = 0;
};

#endif // RAFT_RPC_HPP
