#include "raft/Log.hpp"
#include <stdexcept>


RaftLog::RaftLog()
: snapshotIndex(0), snapshotTerm(0) {
    // Initialize with a dummy entry at index 0
    // This simplifies index math and boundary checks throughout Raft
    LogEntry dummyEntry;
    dummyEntry.index = 0;
    dummyEntry.term = 0;
    dummyEntry.command_data = {}; // Empty command data
    
    Entries.push_back(dummyEntry);
}

// Methods 

void RaftLog::append(const LogEntry& entry) {
    // Checking that the new index is sequential relative to the last entry.
    uint64_t expectedIndex = Entries.back().index + 1;

    if (entry.index != expectedIndex) {
        throw std::logic_error(
            "Invalid log entry index: expected " + 
            std::to_string(expectedIndex) + 
            ", got " + std::to_string(entry.index)
        );
    }
    Entries.push_back(entry);
}

void RaftLog::truncate(uint64_t index){
    // Truncates all entries from the given index 
    // index 0 is the dummy val, so its entry must not be truncated.
    if (index == 0) {
        throw std::invalid_argument("Cannot truncate at index 0 (sentinel)");
    }
    size_t position = static_cast<size_t>(index);
    if (position >= Entries.size()) {
        return; 
    }
    // discard elements from the given position to the end.
    Entries.resize(position);
}

uint64_t RaftLog::getFirstIndex() const {
    return snapshotIndex + 1;
}

const LogEntry& RaftLog::getEntry(uint64_t index) const {
    if (index <= snapshotIndex) {
        throw std::out_of_range("Entry is in snapshot");
    }
    
    size_t vectorIndex = index - snapshotIndex;
    if (vectorIndex >= Entries.size()) {
        throw std::out_of_range("Index out of range");
    }
    
    return Entries[vectorIndex];
}

void RaftLog::discardEntriesUpTo(uint64_t index, uint64_t term) {
    if (index <= snapshotIndex) {
        return; // Already discarded
    }
    size_t vectorIndex = index - snapshotIndex;
    
    if (vectorIndex >= Entries.size()) {
        // Discard everything
        Entries.clear();
    } else {
        // Keep entries after index
        std::vector<LogEntry> newEntries(
            Entries.begin() + vectorIndex,
            Entries.end()
        );
        Entries = std::move(newEntries);
    }
    
    snapshotIndex = index;
    snapshotTerm = term;
    
    // Add dummy entry at new base
    if (Entries.empty() || Entries[0].index != snapshotIndex) {
        LogEntry dummy{snapshotIndex, snapshotTerm, {}};
        Entries.insert(Entries.begin(), dummy);
    }
}




uint64_t RaftLog::getTerm(uint64_t index) const {
    // Returns 0 if index is out of bounds (consistent with Raft's safety checks).
    if (index >= Entries.size()) {
        return 0; 
    }
    return Entries[index].term;
}

uint64_t RaftLog::getLastIndex() const {
    // Log is guaranteed to have at least the index 0 entry.
    return Entries.back().index;
}

uint64_t RaftLog::getLastTerm() const {
    // Log is guaranteed to have at least the index 0 entry.
    return Entries.back().term;
}


