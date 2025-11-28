#ifndef LOG_HPP
#define LOG_HPP 

#include <cstdint>
#include <vector>

struct LogEntry {
  // Metadata for Raft Consensus
  uint64_t index;      
  uint64_t term;        // The term of the Leader that created the entry

  // The Command Data (Serialized)
  std::vector<uint8_t> command_data; // The raw serialized bytes  
};

class RaftLog {
public:
    RaftLog();
    
    void append(const LogEntry& entry);
    void truncate(uint64_t index);
    const LogEntry& getEntry(uint64_t index) const;
    uint64_t getLastIndex() const;
    uint64_t getLastTerm() const;
    uint64_t getTerm(uint64_t index) const;
    
    // Snapshot 
    void discardEntriesUpTo(uint64_t index, uint64_t term);
    uint64_t getFirstIndex() const;
    uint64_t getSnapshotIndex() const { return snapshotIndex; }
    uint64_t getSnapshotTerm() const { return snapshotTerm; }
    
private:
    std::vector<LogEntry> Entries;
    uint64_t snapshotIndex;  // Last index included in snapshot
    uint64_t snapshotTerm;   // Term of snapshotIndex
};
#endif // !LOG_HPP
