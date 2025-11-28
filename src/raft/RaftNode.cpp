#include "raft/RaftNode.hpp"
#include "raft/RaftPersistence.hpp"
#include <algorithm>
#include <stdexcept>
#include <random>
#include <chrono>
#include <iostream>


RaftNode::RaftNode(uint64_t nodeId, 
                   const std::vector<uint64_t>& peers, 
                   std::shared_ptr<KVStore> kvStore, 
                   std::shared_ptr<RaftRPC> rpc,
                   uint64_t snapshotThreshold,
                   const std::string& stateDir)
    : currentTerm(0),
      votedFor(0),
      log(),
      commitIndex(0),
      lastApplied(0),
      role(Role::FOLLOWER),
      nodeId(nodeId),
      peerIds(peers),
      currentLeaderId(0),
      running(false),
      heartbeatInterval( std::chrono::milliseconds(50)),
      rpcClient(rpc),
      kvStore(kvStore),
      randomGenerator(std::random_device{}()),
      lastHeartbeat(std::chrono::steady_clock::now()),
      snapshotThreshold(snapshotThreshold) {

    // Initialize persistence if state file provided
    if (!stateDir.empty()) {
        persistence = std::make_unique<RaftPersistence>(stateDir);
        restoreState();
    }

    
    // Initialize leader state arrays 
    nextIndex.resize(peerIds.size(), 1);
    matchIndex.resize(peerIds.size(), 0);

    for (size_t i = 0; i < peerIds.size(); ++i) {
      peerIdToIndexMap[peerIds[i]] = i;
     }

    // Reset the timer to start the initial election timeout period
    resetElectionTimer();

    // Start worker threads 
    running = true;
    electionThread = std::thread(&RaftNode::electionTimerLoop, this);
    heartbeatThread = std::thread(&RaftNode::heartbeatTimerLoop, this);
    snapshotThread = std::thread(&RaftNode::snapshotTimerLoop, this);
   
    // If any, attempt to restore persisted state 
    restoreState();
}

RaftNode::~RaftNode() {
    running = false; 

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    if (electionThread.joinable()) {
        electionThread.join();
    }
    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
    if (snapshotThread.joinable()) {  
        snapshotThread.join();
    }
    
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        persistState();
    }
}

// RPC Handlers
RequestVoteReply RaftNode::handleRequestVote(const RequestVoteArgs& args) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    RequestVoteReply reply;
    reply.term = currentTerm;
    reply.voteGranted = false;
    
    // Reply false if term < currentTerm 
    if (args.term < currentTerm) {
        return reply;
    }
    // If RPC request contains term T > currentTerm, set currentTerm = T, convert to follower 
    if (args.term > currentTerm) {
        becomeFollower(args.term);
        reply.term = currentTerm;
    }
    
    // Check if we can grant vote
    bool canVote = (votedFor == 0 || votedFor == args.candidateId);
    
    // Check if candidate's log is at least as up-to-date as receiver's log 
    bool logIsUpToDate = isLogUpToDate(args.lastLogIndex, args.lastLogTerm);
    
    // Grant vote if both conditions are met
    if (canVote && logIsUpToDate) {
        votedFor = args.candidateId;
        reply.voteGranted = true;
        persistState();
        
        // Reset the timer after granting a vote
        resetElectionTimer();
    }
    
    return reply;
}

AppendEntriesReply RaftNode::handleAppendEntries(const AppendEntriesArgs& args) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    AppendEntriesReply reply;
    reply.term = currentTerm;
    reply.success = false;
    reply.conflictIndex = 0;
    reply.conflictTerm = 0;
    
    // Reply false if term < currentTerm 
    if (args.term < currentTerm) {
        return reply;
    }
    // If RPC request contains term T ≥ currentTerm, set currentTerm = T, convert to follower 
    if (args.term > currentTerm) {
        becomeFollower(args.term);
        reply.term = currentTerm;
    } else if (role != Role::FOLLOWER) {
        // Same term but we're not a follower - step down
        becomeFollower(args.term);
    }
    // Update current leader
    currentLeaderId = args.leaderId;
    
    resetElectionTimer();

    // Reply false if log doesn't contain an entry at prevLogIndex whose term matches prevLogTerm
    uint64_t lastLogIndex = log.getLastIndex();
    
    // Check if we have the previous entry
    if (args.prevLogIndex > lastLogIndex) {
        // We're missing entries
        reply.conflictIndex = lastLogIndex + 1;
        reply.conflictTerm = 0;
        return reply;
    }
    
    // Check if the term matches
    if (args.prevLogIndex > 0) {
        uint64_t prevTerm = log.getTerm(args.prevLogIndex);
        if (prevTerm != args.prevLogTerm) {
            // Term mismatch - find the first entry of the conflicting term
            reply.conflictTerm = prevTerm;
            reply.conflictIndex = args.prevLogIndex;
            
            // find the first index of the conflicting term
            for (uint64_t i = args.prevLogIndex; i > 0; --i) {
                if (log.getTerm(i) != prevTerm) {
                    reply.conflictIndex = i + 1;
                    break;
                }
                if (i == 1) {
                    reply.conflictIndex = 1;
                }
            }
            return reply;
        }
    }
    // If an existing entry conflicts with a new one delete the existing entry and all that follow it 
        if (!args.entries.empty()) {
        uint64_t newEntryIndex = args.prevLogIndex + 1;
        
        for (size_t i = 0; i < args.entries.size(); ++i) {
            uint64_t entryIndex = newEntryIndex + i;
            
            if (entryIndex <= lastLogIndex) {
                // Entry exists at this index - check for conflict
                uint64_t existingTerm = log.getTerm(entryIndex);
                if (existingTerm != args.entries[i].term) {
                    // Conflict detected - truncate from here
                    log.truncate(entryIndex);
                    lastLogIndex = log.getLastIndex();
                }
            }
            // Append any new entries not already in the log
            if (entryIndex > lastLogIndex) {
                log.append(args.entries[i]);
                lastLogIndex = log.getLastIndex();
            }
        }
        
        persistState();
    }
    
    if (args.leaderCommit > commitIndex) {
        commitIndex = std::min(args.leaderCommit, log.getLastIndex());
        applyCommittedEntries();
    }
    
    reply.success = true;
    return reply;
}

ClientReply RaftNode::handleClientRequest(const ClientRequest& request) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    ClientReply reply;
    reply.success = false;
    reply.isLeader = (role == Role::LEADER);
    reply.leaderId = currentLeaderId;
    
    if (role != Role::LEADER) {
        reply.error = "Not leader";
        return reply;
    }
    
    // Check for duplicate request (already applied)
    auto it = lastAppliedSeq.find(request.clientId);
    if (it != lastAppliedSeq.end() && it->second >= request.sequenceNum) {
        // This request was already applied
        reply.success = true;
        reply.isLeader = true;
        
        // For GET commands, retrieve the value
       if (std::holds_alternative<GetCommand>(request.command)) {
         const GetCommand& getCmd = std::get<GetCommand>(request.command);
         auto result = kvStore->Get(getCmd.key);
         if (result.has_value()) {
           reply.value = result.value();  
         } else {
             reply.value = "";  // Key not found, return empty string
         }
       }        
        return reply;
    }
    
    // Leader proposes the command by appending to log
    try {
        std::vector<uint8_t> commandData = serialize(request.command);
        
        LogEntry entry;
        entry.index = log.getLastIndex() + 1;
        entry.term = currentTerm;
        entry.command_data = std::move(commandData);
        
        log.append(entry);
        persistState();
        
        // For now, return success
        reply.success = true;
        reply.isLeader = true;
        
    } catch (const std::exception& e) {
        reply.error = std::string("Failed to append entry: ") + e.what();
    }
    
    return reply;
}

InstallSnapshotReply RaftNode::handleInstallSnapshot(const InstallSnapshotArgs& args) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    InstallSnapshotReply reply;
    reply.term = currentTerm;
    
    // Reply false if term < currentTerm
    if (args.term < currentTerm) {
        return reply;
    }
    
    if (args.term > currentTerm) {
        becomeFollower(args.term);
        reply.term = currentTerm;
    }
    
    resetElectionTimer();
    
    // Save snapshot
    snapshotData = args.data;
    snapshotIndex = args.lastIncludedIndex;
    snapshotTerm = args.lastIncludedTerm;
    
    // Discard entire log or entries up to snapshot
    log.discardEntriesUpTo(args.lastIncludedIndex, args.lastIncludedTerm);
    
    // Install snapshot to state machine
    kvStore->installSnapshot(args.data);
    
    // Update lastApplied
    if (args.lastIncludedIndex > lastApplied) {
        lastApplied = args.lastIncludedIndex;
    }
    
    if (args.lastIncludedIndex > commitIndex) {
        commitIndex = args.lastIncludedIndex;
    }
    
    persistState();
    
    return reply;
}

// State Query Methods 

uint64_t RaftNode::getCurrentTerm() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentTerm;
}

Role RaftNode::getCurrentRole() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return role;
}

uint64_t RaftNode::getCommitIndex() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return commitIndex;
}

uint64_t RaftNode::getLastApplied() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return lastApplied;
}

//  Helper Methods 

void RaftNode::becomeFollower(uint64_t newTerm) {
    // Must be called with stateMutex held
    currentTerm = newTerm;
    role = Role::FOLLOWER;
    votedFor = 0;
    currentLeaderId = 0;
    persistState();
}

void RaftNode::becomeCandidate() {
    // Must be called with stateMutex held
    role = Role::CANDIDATE;
    currentTerm++;
    votedFor = nodeId; // Vote for self
    currentLeaderId = 0;
    persistState();
    
}

void RaftNode::becomeLeader() {
    role = Role::LEADER;
    currentLeaderId = nodeId;
    uint64_t lastLogIndex = log.getLastIndex();
    for (size_t i = 0; i < peerIds.size(); ++i) {
        nextIndex[i] = lastLogIndex + 1;
        matchIndex[i] = 0;
    }
}

bool RaftNode::isLogUpToDate(uint64_t candidateLastLogIndex, uint64_t candidateLastLogTerm) const {
    // Raft determines which of two logs is more up to date by comparing the
    // index and term of the last entries in the logs.
    
    uint64_t myLastLogIndex = log.getLastIndex();
    uint64_t myLastLogTerm = log.getLastTerm();
    
    // If the logs have last entries with different terms, then the log with the
    // later term is more up-to-date.
    if (candidateLastLogTerm != myLastLogTerm) {
        return candidateLastLogTerm > myLastLogTerm;
    }
    // If the logs end with the same term, then whichever log is longer is more up to date.
    return candidateLastLogIndex >= myLastLogIndex;
}

void RaftNode::updateCommitIndex() {
    if (role != Role::LEADER) {
        return;
    }
    
    uint64_t lastLogIndex = log.getLastIndex();
    
    // Try each index from commitIndex+1 to lastLogIndex
    for (uint64_t n = commitIndex + 1; n <= lastLogIndex; ++n) {
        // Only commit entries from current term (safety requirement 
        if (log.getTerm(n) != currentTerm) {
            continue;
        }
        // Count how many servers have replicated this entry
        size_t replicationCount = 1; // Count self
        for (size_t i = 0; i < peerIds.size(); ++i) {
            if (matchIndex[i] >= n) {
                replicationCount++;
            }
        }
        // Check if we have a majority
        size_t majority = (peerIds.size() + 1) / 2 + 1; 
        if (replicationCount >= majority) {
            commitIndex = n;
            applyCommittedEntries();
        }
    }
}

void RaftNode::applyCommittedEntries() {
    
   while (lastApplied < commitIndex) {
        lastApplied++;
        const LogEntry& entry = log.getEntry(lastApplied);
        try {
            CommandData command = deserialize(entry.command_data);
            
            // Apply command to KVStore 
            std::visit([this](auto&& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                // Check for duplicate 
                auto it = lastAppliedSeq.find(cmd.meta.clientId);
                if (it != lastAppliedSeq.end() && it->second >= cmd.meta.sequenceNum) {
                    // Already applied this command, skip
                    return;
                }
                // Apply command based on type
                if constexpr (std::is_same_v<T, PutCommand>) {
                    kvStore->Put(cmd.key, cmd.value);
                } else if constexpr (std::is_same_v<T, AppendCommand>) {
                    kvStore->Append(cmd.key, cmd.value);
                } else if constexpr (std::is_same_v<T, GetCommand>) {
                // GET doesn't modify state, but we still track it,and the result would be returned to client via different mechanism
                }
                // Update last applied sequence number for this client
                lastAppliedSeq[cmd.meta.clientId] = cmd.meta.sequenceNum;
            }, command);
            
        } catch (const std::exception& e) {
        }
    }}

void RaftNode::persistState() {
    if (persistence) {
        persistence->saveState(currentTerm, votedFor, log, 
                              snapshotData, log.getSnapshotIndex(), log.getSnapshotTerm());
    }
}

void RaftNode::restoreState() {
    if (persistence) {
        uint64_t savedTerm, savedVotedFor, snapIndex, snapTerm;
        std::vector<uint8_t> savedSnapshot;
        
        if (persistence->loadState(savedTerm, savedVotedFor, log, 
                                   savedSnapshot, snapIndex, snapTerm)) {
            currentTerm = savedTerm;
            votedFor = savedVotedFor;
            snapshotData = savedSnapshot;
            
            // Restore snapshot to KVStore
            if (!savedSnapshot.empty()) {
                kvStore->installSnapshot(savedSnapshot);
                lastApplied = snapIndex;
                commitIndex = snapIndex;
            }
            
            std::cout << "Node " << nodeId << " restored state: term=" << currentTerm
                      << ", snapshotIndex=" << snapIndex << std::endl;
        }
    }
}

void RaftNode::electionTimerLoop() {
    std::cout << "Node " << nodeId << " election timer loop started" << std::endl;
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!running) break;
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!running) break;
        
        // Only followers/candidates check election timeout
        if (role == Role::LEADER) continue;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastHeartbeat
        );
        
        // Debug output every second
        static auto lastDebug = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDebug).count() > 1000) {
            std::cout << "Node " << nodeId << " election check: elapsed=" 
                      << elapsed.count() << "ms, timeout=" << electionTimeout.count() 
                      << "ms, role=" << (role == Role::FOLLOWER ? "FOLLOWER" : "CANDIDATE")
                      << std::endl;
            lastDebug = now;
        }
        
        if (elapsed > electionTimeout) {
            std::cout << "Node " << nodeId << " ELECTION TIMEOUT! Starting election..." << std::endl;
            startElection();
        }
    }
    
    std::cout << "Node " << nodeId << " election timer loop ended" << std::endl;
}

void RaftNode::heartbeatTimerLoop() {
    while (running) {
        // Sleep in smaller chunks to be more responsive
        for (int i = 0; i < 5 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (!running) break;
        
        std::lock_guard<std::mutex> lock(stateMutex);
        
        if (!running) break;
        
        if (role == Role::LEADER) {
            sendHeartbeats();
        }
    }
}

void RaftNode::sendHeartbeats() {
    // Send empty AppendEntries to all peers
    for (uint64_t peerId : peerIds) {
        replicateToFollower(peerId);
    }
}

void RaftNode::startElection() {
    std::cout << "Node " << nodeId << " starting election in term " << (currentTerm + 1) << std::endl;
    
    becomeCandidate();
    resetElectionTimer();
    
    RequestVoteArgs args;
    args.term = currentTerm;
    args.candidateId = nodeId;
    args.lastLogIndex = log.getLastIndex();
    args.lastLogTerm = log.getLastTerm();
    
    size_t votesReceived = 1; // Vote for self
    size_t votesNeeded = (peerIds.size() + 1) / 2 + 1;
    
    std::cout << "Node " << nodeId << " needs " << votesNeeded << " votes" << std::endl;
    
    // Send RequestVote to all peers
    for (uint64_t peerId : peerIds) {
        try {
            RequestVoteReply reply = rpcClient->sendRequestVote(peerId, args);
            
            std::cout << "Node " << nodeId << " got vote response from " << peerId 
                      << ": granted=" << reply.voteGranted << ", term=" << reply.term << std::endl;
            
            if (reply.voteGranted) {
                votesReceived++;
            }
            
            if (reply.term > currentTerm) {
                std::cout << "Node " << nodeId << " stepping down due to higher term" << std::endl;
                becomeFollower(reply.term);
                return;
            }
        } catch (const std::exception& e) {
            std::cout << "Node " << nodeId << " RPC to " << peerId << " failed: " << e.what() << std::endl;
            continue;
        }
    }
    std::cout << "Node " << nodeId << " received " << votesReceived << " votes" << std::endl;

    if (votesReceived >= votesNeeded) {
        std::cout << "Node " << nodeId << " WON ELECTION! Becoming leader" << std::endl;
        becomeLeader();
        sendHeartbeats();
    } else {
        std::cout << "Node " << nodeId << " LOST election, remaining candidate" << std::endl;
    }
}

void RaftNode::replicateToFollower(uint64_t peerId) {
    size_t peerIndex;
    try {
        peerIndex = peerIdToIndexMap.at(peerId);
    } catch (const std::out_of_range& e) {
        return;
    }

    uint64_t next = nextIndex[peerIndex];
    uint64_t prevIndex = next - 1;
    uint64_t prevTerm = log.getTerm(prevIndex);
    
    if (next <= log.getSnapshotIndex()) {
        InstallSnapshotArgs args;
        args.term = currentTerm;
        args.leaderId = nodeId;
        args.lastIncludedIndex = snapshotIndex;
        args.lastIncludedTerm = snapshotTerm;
        args.data = snapshotData;
        
        InstallSnapshotReply reply = rpcClient->sendInstallSnapshot(peerId, args);
        
        if (reply.term > currentTerm) {
            becomeFollower(reply.term);
            return;
        }
        
        // Update nextIndex
        nextIndex[peerIndex] = snapshotIndex + 1;
        matchIndex[peerIndex] = snapshotIndex;
        return;
    }

    AppendEntriesArgs args;
    args.term = currentTerm;
    args.leaderId = nodeId;
    args.prevLogIndex = prevIndex;
    args.prevLogTerm = prevTerm;
    args.leaderCommit = commitIndex;
    
    for (uint64_t i = next; i <= log.getLastIndex(); ++i) {
        args.entries.push_back(log.getEntry(i));
    }
    
    // Send RPC
    AppendEntriesReply reply = rpcClient->sendAppendEntries(
        peerId, 
        args,
        std::chrono::milliseconds(100)
    );
    
    // Handle response
    if (reply.term > currentTerm) {
        becomeFollower(reply.term);
        return;
    }
    
    if (reply.success) {
        if (!args.entries.empty()) {
            matchIndex[peerIndex] = args.entries.back().index;
            nextIndex[peerIndex] = matchIndex[peerIndex] + 1;
        }
        updateCommitIndex();
    } else {
        // Backtrack on failure
        nextIndex[peerIndex] = std::max(static_cast<uint64_t>(1), reply.conflictIndex);
    }
}
// Timer Helper Methods 
void RaftNode::resetElectionTimer() {
   // Generate random timeout between 150ms and 300ms 
    std::uniform_int_distribution<> distrib(150, 300); 
    electionTimeout = std::chrono::milliseconds(distrib(randomGenerator));
    lastHeartbeat = std::chrono::steady_clock::now();

}

void RaftNode::takeSnapshot(uint64_t upToIndex) {
    //std::lock_guard<std::mutex> lock(stateMutex);
    
    if (upToIndex <= log.getSnapshotIndex()) {
        return; 
    }
    
    if (upToIndex > lastApplied) {
      throw std::runtime_error(
        "Cannot snapshot unapplied entries. upToIndex=" + 
        std::to_string(upToIndex) + 
        ", lastApplied=" + std::to_string(lastApplied)
      );
    }
    
    // Take snapshot of state machine
    snapshotData = kvStore->takeSnapshot();
    snapshotIndex = upToIndex;
    snapshotTerm = log.getTerm(upToIndex);
    // Discard log entries
    log.discardEntriesUpTo(upToIndex, snapshotTerm);
    // Persist snapshot to disk
    persistState();
}
void RaftNode::snapshotTimerLoop() {
    // Check for snapshot need every 5 seconds
    while (running) {
        // Sleep in chunks to be responsive to shutdown
        for (int i = 0; i < 50 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!running) break;
        // Check if snapshot is needed
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            
            if (!running) break;
            
            if (shouldTakeSnapshot()) {
                try {
                    // Take snapshot at lastApplied
                    uint64_t snapshotIndex = lastApplied;
                    
                    if (snapshotIndex > log.getSnapshotIndex()) {
                        std::cout << "Node " << nodeId 
                                  << " taking automatic snapshot at index " 
                                  << snapshotIndex << std::endl;
                        
                        takeSnapshot(snapshotIndex);
                        
                        std::cout << "Node " << nodeId 
                                  << " snapshot complete. Log size: " 
                                  << (log.getLastIndex() - log.getSnapshotIndex())
                                  << " entries" << std::endl;
                    }
                } catch (const std::exception& e) {
                    // Log error but continue
                    std::cerr << "Node " << nodeId 
                              << " snapshot failed: " << e.what() << std::endl;
                }
            }
        }
    }
}

bool RaftNode::shouldTakeSnapshot() const {
    uint64_t logSize = log.getLastIndex() - log.getSnapshotIndex();
    // Take snapshot if log exceeds threshold
    return logSize >= snapshotThreshold;
}

void RaftNode::triggerSnapshot() {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    if (lastApplied > log.getSnapshotIndex()) {
        takeSnapshot(lastApplied);
    }
}

uint64_t RaftNode::getLogSize() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return log.getLastIndex() - log.getSnapshotIndex();
}

uint64_t RaftNode::getSnapshotSize() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return snapshotData.size();
}
