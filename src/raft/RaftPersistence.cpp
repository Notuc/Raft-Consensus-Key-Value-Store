#include "raft/RaftPersistence.hpp"
#include "raft/Log.hpp"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <cstring>

namespace fs = std::filesystem;

RaftPersistence::RaftPersistence(const std::string& stateDir)
    : stateDir(stateDir) {
    if (!stateDir.empty()) {
        fs::create_directories(stateDir);
    }
}

std::string RaftPersistence::getStatePath() const {
    return stateDir + "/raft_state.dat";
}

std::string RaftPersistence::getSnapshotPath() const {
    return stateDir + "/raft_snapshot.dat";
}

void RaftPersistence::saveState(uint64_t currentTerm,
                                 uint64_t votedFor,
                                 const RaftLog& log,
                                 const std::vector<uint8_t>& snapshot,
                                 uint64_t snapshotIndex,
                                 uint64_t snapshotTerm) {
    if (stateDir.empty()) {
        return; 
    }
    
    // Save state file (term, votedFor, log)
    {
        std::ofstream file(getStatePath(), std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Failed to open state file for writing");
        }
        
        // Write currentTerm
        file.write(reinterpret_cast<const char*>(&currentTerm), sizeof(currentTerm));
        
        // Write votedFor
        file.write(reinterpret_cast<const char*>(&votedFor), sizeof(votedFor));
        
        // Write snapshot metadata
        file.write(reinterpret_cast<const char*>(&snapshotIndex), sizeof(snapshotIndex));
        file.write(reinterpret_cast<const char*>(&snapshotTerm), sizeof(snapshotTerm));
        
        // Serialize and write log
        std::vector<uint8_t> logData = serializeLog(log);
        uint64_t logSize = logData.size();
        file.write(reinterpret_cast<const char*>(&logSize), sizeof(logSize));
        file.write(reinterpret_cast<const char*>(logData.data()), logSize);
        
        file.flush();
    }
    
    // Save snapshot file separately
    if (!snapshot.empty()) {
        std::ofstream snapFile(getSnapshotPath(), std::ios::binary | std::ios::trunc);
        if (!snapFile) {
            throw std::runtime_error("Failed to open snapshot file for writing");
        }
        
        snapFile.write(reinterpret_cast<const char*>(snapshot.data()), snapshot.size());
        snapFile.flush();
    }
}

bool RaftPersistence::loadState(uint64_t& currentTerm,
                                 uint64_t& votedFor,
                                 RaftLog& log,
                                 std::vector<uint8_t>& snapshot,
                                 uint64_t& snapshotIndex,
                                 uint64_t& snapshotTerm) {
    if (stateDir.empty() || !exists()) {
        return false;
    }
    // Load state file
    {
        std::ifstream file(getStatePath(), std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Read currentTerm
        file.read(reinterpret_cast<char*>(&currentTerm), sizeof(currentTerm));
        if (file.gcount() != sizeof(currentTerm)) {
            throw std::runtime_error("Corrupted state file: incomplete currentTerm");
        }
        
        // Read votedFor
        file.read(reinterpret_cast<char*>(&votedFor), sizeof(votedFor));
        if (file.gcount() != sizeof(votedFor)) {
            throw std::runtime_error("Corrupted state file: incomplete votedFor");
        }
        
        // Read snapshot metadata
        file.read(reinterpret_cast<char*>(&snapshotIndex), sizeof(snapshotIndex));
        if (file.gcount() != sizeof(snapshotIndex)) {
            throw std::runtime_error("Corrupted state file: incomplete snapshotIndex");
        }
        
        file.read(reinterpret_cast<char*>(&snapshotTerm), sizeof(snapshotTerm));
        if (file.gcount() != sizeof(snapshotTerm)) {
            throw std::runtime_error("Corrupted state file: incomplete snapshotTerm");
        }
        
        // Read log size
        uint64_t logSize;
        file.read(reinterpret_cast<char*>(&logSize), sizeof(logSize));
        if (file.gcount() != sizeof(logSize)) {
            throw std::runtime_error("Corrupted state file: incomplete log size");
        }
        
        // Read log data
        std::vector<uint8_t> logData(logSize);
        file.read(reinterpret_cast<char*>(logData.data()), logSize);
        if (static_cast<uint64_t>(file.gcount()) != logSize) {
            throw std::runtime_error("Corrupted state file: incomplete log data");
        }
        
        // Deserialize log
        deserializeLog(logData, log);
    }
    
    // Load snapshot file if exists
    if (fs::exists(getSnapshotPath())) {
        std::ifstream snapFile(getSnapshotPath(), std::ios::binary);
        if (snapFile) {
            snapFile.seekg(0, std::ios::end);
            size_t snapSize = snapFile.tellg();
            snapFile.seekg(0, std::ios::beg);
            
            snapshot.resize(snapSize);
            snapFile.read(reinterpret_cast<char*>(snapshot.data()), snapSize);
        }
    }
    
    return true;
}

bool RaftPersistence::exists() const {
    return fs::exists(getStatePath());
}

void RaftPersistence::clear() {
    if (!stateDir.empty()) {
        fs::remove(getStatePath());
        fs::remove(getSnapshotPath());
    }
}

std::vector<uint8_t> RaftPersistence::serializeLog(const RaftLog& log) {
    std::vector<uint8_t> data;
    
    uint64_t firstIndex = log.getSnapshotIndex() + 1;
    uint64_t lastIndex = log.getLastIndex();
    
    // Write number of entries 
    uint64_t numEntries = (lastIndex >= firstIndex) ? (lastIndex - firstIndex + 1) : 0;
    const uint8_t* numPtr = reinterpret_cast<const uint8_t*>(&numEntries);
    data.insert(data.end(), numPtr, numPtr + sizeof(numEntries));
    
    // Write each entry
    for (uint64_t i = firstIndex; i <= lastIndex; i++) {
        try {
            const LogEntry& entry = log.getEntry(i);
            
            // Write index
            const uint8_t* idxPtr = reinterpret_cast<const uint8_t*>(&entry.index);
            data.insert(data.end(), idxPtr, idxPtr + sizeof(entry.index));
            
            // Write term
            const uint8_t* termPtr = reinterpret_cast<const uint8_t*>(&entry.term);
            data.insert(data.end(), termPtr, termPtr + sizeof(entry.term));
            
            // Write command_data size
            uint64_t cmdSize = entry.command_data.size();
            const uint8_t* sizePtr = reinterpret_cast<const uint8_t*>(&cmdSize);
            data.insert(data.end(), sizePtr, sizePtr + sizeof(cmdSize));
            
            // Write command_data
            data.insert(data.end(), entry.command_data.begin(), entry.command_data.end());
        } catch (const std::out_of_range&) {
            break;
        }
    }
    
    return data;
}

void RaftPersistence::deserializeLog(const std::vector<uint8_t>& data, RaftLog& log) {
    if (data.empty()) {
        return;
    }
    
    size_t offset = 0;
    
    // Read number of entries
    if (offset + sizeof(uint64_t) > data.size()) {
        throw std::runtime_error("Corrupted log data: missing entry count");
    }
    uint64_t numEntries;
    std::memcpy(&numEntries, data.data() + offset, sizeof(numEntries));
    offset += sizeof(numEntries);
    
    // Read each entry
    for (uint64_t i = 0; i < numEntries; i++) {
        LogEntry entry;
        
        // Read index
        if (offset + sizeof(entry.index) > data.size()) {
            throw std::runtime_error("Corrupted log data: incomplete index");
        }
        std::memcpy(&entry.index, data.data() + offset, sizeof(entry.index));
        offset += sizeof(entry.index);
        
        // Read term
        if (offset + sizeof(entry.term) > data.size()) {
            throw std::runtime_error("Corrupted log data: incomplete term");
        }
        std::memcpy(&entry.term, data.data() + offset, sizeof(entry.term));
        offset += sizeof(entry.term);
        
        // Read command_data size
        uint64_t cmdSize;
        if (offset + sizeof(cmdSize) > data.size()) {
            throw std::runtime_error("Corrupted log data: incomplete command size");
        }
        std::memcpy(&cmdSize, data.data() + offset, sizeof(cmdSize));
        offset += sizeof(cmdSize);
        
        // Read command_data
        if (offset + cmdSize > data.size()) {
            throw std::runtime_error("Corrupted log data: incomplete command data");
        }
        entry.command_data.resize(cmdSize);
        std::memcpy(entry.command_data.data(), data.data() + offset, cmdSize);
        offset += cmdSize;
        
        // Append to log
        log.append(entry);
    }
}
