#include "kvstore/KVStore.hpp"

void KVStore::Put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mapMutex_); 
    kvMap_[key] = value;
}

std::optional<std::string> KVStore::Get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mapMutex_); 

    auto it = kvMap_.find(key);

    if (it != kvMap_.end()) {
        return it->second; 
    } else {
        return std::nullopt; 
    }
}

bool KVStore::Append(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mapMutex_); 

    auto it = kvMap_.find(key);

    if (it != kvMap_.end()) {
        it->second += value; 
        return true;
    } else {
        kvMap_[key] = value;
        return true;
    }
}

std::vector<uint8_t> KVStore::takeSnapshot() const {
    std::lock_guard<std::mutex> lock(mapMutex_);
    
    std::vector<uint8_t> snapshot;
    
    // Write number of entries
    uint64_t numEntries = kvMap_.size();
    snapshot.insert(snapshot.end(),
                   reinterpret_cast<const uint8_t*>(&numEntries),
                   reinterpret_cast<const uint8_t*>(&numEntries) + sizeof(numEntries));
    
    // Write each key-value pair
    for (const auto& [key, value] : kvMap_) {
        uint32_t keyLen = key.size();
        snapshot.insert(snapshot.end(),
                       reinterpret_cast<const uint8_t*>(&keyLen),
                       reinterpret_cast<const uint8_t*>(&keyLen) + sizeof(keyLen));
        snapshot.insert(snapshot.end(), key.begin(), key.end());
        
        uint32_t valueLen = value.size();
        snapshot.insert(snapshot.end(),
                       reinterpret_cast<const uint8_t*>(&valueLen),
                       reinterpret_cast<const uint8_t*>(&valueLen) + sizeof(valueLen));
        snapshot.insert(snapshot.end(), value.begin(), value.end());
    }
    
    return snapshot;
}

void KVStore::installSnapshot(const std::vector<uint8_t>& snapshot) {
    std::lock_guard<std::mutex> lock(mapMutex_);
    
    kvMap_.clear();
    
    size_t offset = 0;
    
    // Read number of entries
    uint64_t numEntries;
    std::memcpy(&numEntries, snapshot.data() + offset, sizeof(numEntries));
    offset += sizeof(numEntries);
    
    // Read each key-value pair
    for (uint64_t i = 0; i < numEntries; ++i) {
        // Read key
        uint32_t keyLen;
        std::memcpy(&keyLen, snapshot.data() + offset, sizeof(keyLen));
        offset += sizeof(keyLen);
        
        std::string key(reinterpret_cast<const char*>(snapshot.data() + offset), keyLen);
        offset += keyLen;
        
        // Read value
        uint32_t valueLen;
        std::memcpy(&valueLen, snapshot.data() + offset, sizeof(valueLen));
        offset += sizeof(valueLen);
        
        std::string value(reinterpret_cast<const char*>(snapshot.data() + offset), valueLen);
        offset += valueLen;
        
        kvMap_[key] = value;
    }
}



