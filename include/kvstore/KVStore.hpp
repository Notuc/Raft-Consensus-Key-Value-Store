#ifndef KVSTORE_HPP
#define KVSTORE_HPP

#include <unordered_map>
#include <string>
#include <mutex>
#include <optional>
#include <vector>

class KVStore {
  public:
  
    void Put(const std::string& key, const std::string& value);
    std::optional<std::string> Get(const std::string& key) const;
    bool Append(const std::string& key, const std::string& value); 
  

  // Snapshots
    std::vector<uint8_t> takeSnapshot() const;
    void installSnapshot(const std::vector<uint8_t>& snapshot);

  private:
  
  //containers kvMap_, mapMutex_ underscore for best pratices? May remove.
    std::unordered_map<std::string, std::string> kvMap_;
    mutable std::mutex mapMutex_; 
};


#endif // !KVSTORE_HPP
