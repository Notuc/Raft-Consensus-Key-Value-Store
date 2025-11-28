#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <variant>


enum class CommandType: uint8_t {
  PUT = 1,
  APPEND = 2,
  GET = 3,
};

struct ClientRequestMeta {
  uint64_t clientId;
  uint64_t sequenceNum;
};

struct PutCommand {
  ClientRequestMeta meta;
  std::string key;
  std::string value;
};

struct AppendCommand {
  ClientRequestMeta meta;
  std::string key;
  std::string value;
};

struct GetCommand{
  ClientRequestMeta meta;
  std::string key;
};

using CommandData = std::variant<PutCommand, AppendCommand, GetCommand>;

// Serialization Methods 
std::vector<uint8_t> serialize(const CommandData& command);
CommandData deserialize(const std::vector<uint8_t>& bytes);



#endif // COMMAND_HPP
