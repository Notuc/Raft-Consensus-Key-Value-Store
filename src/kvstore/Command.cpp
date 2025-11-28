#include "kvstore/Command.hpp"
#include <stdexcept>
#include <type_traits>
#include <cstring>

namespace {

inline void write_u8(std::vector<uint8_t>& out, uint8_t val) {
    out.push_back(val);
}

inline void write_u64_le(std::vector<uint8_t>& out, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

inline void write_u32_le(std::vector<uint8_t>& out, uint32_t val) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

void write_string(std::vector<uint8_t>& out, const std::string& str) {
    if (str.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("String too large to serialize");
    }
    write_u32_le(out, static_cast<uint32_t>(str.size()));
    out.insert(out.end(), str.begin(), str.end());
}

void write_meta(std::vector<uint8_t>& out, const ClientRequestMeta& meta) {
    write_u64_le(out, meta.clientId);
    write_u64_le(out, meta.sequenceNum);
}

// Read Helpers 

inline uint8_t read_u8(const std::vector<uint8_t>& in, size_t& offset) {
    if (offset >= in.size()) {
        throw std::runtime_error("Buffer underrun reading u8");
    }
    return in[offset++];
}

inline uint64_t read_u64_le(const std::vector<uint8_t>& in, size_t& offset) {
    if (offset + 8 > in.size()) {
        throw std::runtime_error("Buffer underrun reading u64");
    }
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (static_cast<uint64_t>(in[offset++]) << (i * 8));
    }
    return val;
}

inline uint32_t read_u32_le(const std::vector<uint8_t>& in, size_t& offset) {
    if (offset + 4 > in.size()) {
        throw std::runtime_error("Buffer underrun reading u32");
    }
    uint32_t val = 0;
    for (int i = 0; i < 4; ++i) {
        val |= (static_cast<uint32_t>(in[offset++]) << (i * 8));
    }
    return val;
}

std::string read_string(const std::vector<uint8_t>& in, size_t& offset) {
    uint32_t len = read_u32_le(in, offset);
    if (offset + len > in.size()) {
        throw std::runtime_error("Buffer underrun reading string");
    }
    std::string result(reinterpret_cast<const char*>(in.data() + offset), len);
    offset += len;
    return result;
}

ClientRequestMeta read_meta(const std::vector<uint8_t>& in, size_t& offset) {
    ClientRequestMeta meta;
    meta.clientId = read_u64_le(in, offset);
    meta.sequenceNum = read_u64_le(in, offset);
    return meta;
}

} 

// Seraialization Logic

std::vector<uint8_t> serialize(const CommandData& command) {
    std::vector<uint8_t> buffer;
    buffer.reserve(128); // Reserve reasonable space upfront

    std::visit([&buffer](const auto& cmd) {
        using CmdType = std::decay_t<decltype(cmd)>;

        if constexpr (std::is_same_v<CmdType, PutCommand>) {
            write_u8(buffer, static_cast<uint8_t>(CommandType::PUT));
            write_meta(buffer, cmd.meta);
            write_string(buffer, cmd.key);
            write_string(buffer, cmd.value);
        }
        else if constexpr (std::is_same_v<CmdType, AppendCommand>) {
            write_u8(buffer, static_cast<uint8_t>(CommandType::APPEND));
            write_meta(buffer, cmd.meta);
            write_string(buffer, cmd.key);
            write_string(buffer, cmd.value);
        }
        else if constexpr (std::is_same_v<CmdType, GetCommand>) {
            write_u8(buffer, static_cast<uint8_t>(CommandType::GET));
            write_meta(buffer, cmd.meta);
            write_string(buffer, cmd.key);
        }
        else {
            static_assert(sizeof(CmdType) == 0, "Unhandled command type in serialize");
        }
    }, command);

    return buffer;
}
// Deseraialization Logic

CommandData deserialize(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        throw std::runtime_error("Cannot deserialize empty buffer");
    }

    size_t offset = 0;
    uint8_t type_byte = read_u8(bytes, offset);
    CommandType type = static_cast<CommandType>(type_byte);

    switch (type) {
        case CommandType::PUT: {
            PutCommand cmd;
            cmd.meta = read_meta(bytes, offset);
            cmd.key = read_string(bytes, offset);
            cmd.value = read_string(bytes, offset);
            return cmd;
        }

        case CommandType::APPEND: {
            AppendCommand cmd;
            cmd.meta = read_meta(bytes, offset);
            cmd.key = read_string(bytes, offset);
            cmd.value = read_string(bytes, offset);
            return cmd;
        }

        case CommandType::GET: {
            GetCommand cmd;
            cmd.meta = read_meta(bytes, offset);
            cmd.key = read_string(bytes, offset);
            return cmd;
        }

        default:
            throw std::runtime_error("Unknown command type: " + 
                                   std::to_string(static_cast<int>(type_byte)));
    }
}
