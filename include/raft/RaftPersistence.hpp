#ifndef RAFT_PERSISTENCE_HPP
#define RAFT_PERSISTENCE_HPP

#include <string>
#include <vector>
#include <cstdint>

class RaftLog;

class RaftPersistence {
public:
    explicit RaftPersistence(const std::string& stateDir);
    
    // Save complete Raft state
    void saveState(uint64_t currentTerm, 
                   uint64_t votedFor, 
                   const RaftLog& log,
                   const std::vector<uint8_t>& snapshot,
                   uint64_t snapshotIndex,
                   uint64_t snapshotTerm);
    
    // Load complete Raft state
    bool loadState(uint64_t& currentTerm,
                   uint64_t& votedFor,
                   RaftLog& log,
                   std::vector<uint8_t>& snapshot,
                   uint64_t& snapshotIndex,
                   uint64_t& snapshotTerm);
    
    // Check if state exists
    bool exists() const;
    
    // Clear all persisted state
    void clear();
    
private:
    std::string stateDir;
    std::string getStatePath() const;
    std::string getSnapshotPath() const;
    
    // Serialize/deserialize log
    std::vector<uint8_t> serializeLog(const RaftLog& log);
    void deserializeLog(const std::vector<uint8_t>& data, RaftLog& log);
};

#endif // RAFT_PERSISTENCE_HPP
