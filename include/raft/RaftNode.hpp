#ifndef RAFTNODE_HPP
#define RAFTNODE_HPP
#include "RaftRPC.hpp"
#include "kvstore/KVStore.hpp"
#include "kvstore/Command.hpp"
#include "RaftPersistence.hpp"

#include "Log.hpp"
#include <cstdint>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <random>

// Enums

enum class Role : uint8_t {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

// RPC Argument Structures 

struct RequestVoteArgs {
    uint64_t term;           
    uint64_t candidateId;    
    uint64_t lastLogIndex;   
    uint64_t lastLogTerm;   
};

struct RequestVoteReply {
    uint64_t term;            
    bool voteGranted;         
};

struct AppendEntriesArgs {
    uint64_t term;           // Leader's term
    uint64_t leaderId;       // So follower can redirect clients
    uint64_t prevLogIndex;   
    uint64_t prevLogTerm;    
    std::vector<LogEntry> entries;  
    uint64_t leaderCommit;   
};

struct AppendEntriesReply {
    uint64_t term;           // Current term, for leader to update itself
    bool success;            // True if follower contained entry matching prevLogIndex and prevLogTerm
    
   // Optional: Optimization fields for faster log backtracking
   //  uint64_t conflictIndex;  
   // uint64_t conflictTerm;   
};

struct ClientRequest {
    CommandData command;     // The Put/Append/Get command
    uint64_t clientId;       // ID of the client making the request
    uint64_t sequenceNum;    // Sequence number for deduplication
};

struct ClientReply {
    bool success;            // True if command was committed
    std::string value;       // For GET requests, the retrieved value
    bool isLeader;           // True if this node is the leader
    uint64_t leaderId;       // ID of current leader 
    std::string error;       
};
struct InstallSnapshotArgs {
    uint64_t term;              // Leader's term
    uint64_t leaderId;          // So follower can redirect clients
    uint64_t lastIncludedIndex; 
    uint64_t lastIncludedTerm; 
    std::vector<uint8_t> data;  // Raw bytes of snapshot
};

struct InstallSnapshotReply {
    uint64_t term;              
};



class RaftRPC; // Forward declaration
class RaftPersistence;//Foward declaration


// RaftNode Class 

class RaftNode {
public:
    RaftNode(uint64_t nodeId, 
             const std::vector<uint64_t>& peers, 
             std::shared_ptr<KVStore> kvStore,
             std::shared_ptr<RaftRPC> rpc,
             uint64_t snapshotThreshold = 10000,
             const std::string& stateFile = "");
    
    ~RaftNode();

    InstallSnapshotReply handleInstallSnapshot(const InstallSnapshotArgs& args);
    // Snapshot trigger method
    void takeSnapshot(uint64_t index);
    // Manual snapshot trigger (for testing/debugging)
    void triggerSnapshot();
    
    // To get snapshot stats
    uint64_t getLogSize() const;
    uint64_t getSnapshotSize() const;
    
    // Handles incoming RequestVote RPC 
    // Called by candidates during elections
    RequestVoteReply handleRequestVote(const RequestVoteArgs& args);
    
    // Handles incoming AppendEntries RPC
    // Called by leader for heartbeats and log replications
    AppendEntriesReply handleAppendEntries(const AppendEntriesArgs& args);
    
    // Handles incoming client request
    // Redirects to leader if not leader, or proposes command if leader
    ClientReply handleClientRequest(const ClientRequest& request);
    
    // State Query Methods  
    uint64_t getCurrentTerm() const;
    Role getCurrentRole() const;
    uint64_t getCommitIndex() const;
    uint64_t getLastApplied() const;
    
private:
    std::mt19937 randomGenerator;
    std::shared_ptr<KVStore> kvStore;
    std::unique_ptr<RaftPersistence> persistence;
    std::shared_ptr<RaftRPC> rpcClient;
    std::unordered_map<uint64_t, size_t> peerIdToIndexMap;
    std::vector<uint8_t> snapshotData;  // Current snapshot
    uint64_t snapshotIndex;             // Index covered by snapshot
    uint64_t snapshotTerm;              // Term at snapshot index
   
    uint64_t snapshotThreshold;  // Take snapshot when log exceeds this size
    
    // Snapshot thread
    std::thread snapshotThread;
    void snapshotTimerLoop();
    
    // Helper to check if snapshot is needed
    bool shouldTakeSnapshot() const;
    
    // For tracking applied commands and deduplication
    std::unordered_map<uint64_t, uint64_t> lastAppliedSeq;  // clientId -> last applied seqNum
    
    // Timing state
    std::chrono::steady_clock::time_point lastHeartbeat{std::chrono::steady_clock::now()};
    std::chrono::milliseconds electionTimeout{std::chrono::milliseconds(200)}; // Default
    std::chrono::milliseconds heartbeatInterval{std::chrono::milliseconds(50)};    
    
    // Background threads
    std::thread electionThread;
    std::thread heartbeatThread;
    std::atomic<bool> running;


    // Persistent State 
    uint64_t currentTerm;    // Latest term server has 
    uint64_t votedFor;       // CandidateId that received vote in current term (0 if none)
    RaftLog log;             // Log entries
    
    // State (All servers)
    uint64_t commitIndex;    // Index of highest log entry known to be committed (initialized to 0)
    uint64_t lastApplied;    // Index of highest log entry applied to state machine (initialized to 0)
    Role role;               // Current role: Follower, Candidate, or Leader
    
    //  Leader State
    std::vector<uint64_t> nextIndex;  // For each server, index of next log entry to send
    std::vector<uint64_t> matchIndex; // For each server, index of highest log entry known to be replicated
    
    //  Node Configuration 
    uint64_t nodeId;                  
    std::vector<uint64_t> peerIds;    // IDs of all other nodes in cluster
    uint64_t currentLeaderId;         // ID of current leader (0 if unknown)
    
    // Concurrency     
    mutable std::mutex stateMutex;    // Protects all state variables
    
    // Helper Methods 
    
    // Convert to follower 
    void becomeFollower(uint64_t newTerm);
    
    // Convert to candidate and start election
    void becomeCandidate();
    
    // Convert to leader after winning election
    void becomeLeader();
    
    // Check if candidate's log is at least as up-to-date as receiver's log
    bool isLogUpToDate(uint64_t candidateLastLogIndex, uint64_t candidateLastLogTerm) const;
    
    // Update commitIndex based on matchIndex array  
    void updateCommitIndex();
    
    // Apply committed entries to state machine
    void applyCommittedEntries();
    
    // Persist state to disk 
    void persistState();
    
    // Restore state from disk
    void restoreState();

    // Methods
    void electionTimerLoop();      // Monitors for leader timeout
    void heartbeatTimerLoop();     // Leader sends periodic sendHeartbeats
    void replicateToFollower(uint64_t peerId);  // Send AppendEntries to one peerId
    void sendHeartbeats();         // Send to all peerIds
    void startElection();   // Broadcast RequestVote RPCs
    void resetElectionTimer();
};

#endif // RAFTNODE_HPP
