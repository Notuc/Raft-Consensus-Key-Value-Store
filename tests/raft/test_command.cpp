#include <gtest/gtest.h>
#include "kvstore/Command.hpp"

TEST(CommandSerializationTest, SerializePutCommand) {
    PutCommand cmd;
    cmd.meta.clientId = 123;
    cmd.meta.sequenceNum = 456;
    cmd.key = "mykey";
    cmd.value = "myvalue";
    
    CommandData cmdData = cmd;
    std::vector<uint8_t> serialized = serialize(cmdData);
    
    EXPECT_FALSE(serialized.empty());
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(CommandType::PUT));
}

TEST(CommandSerializationTest, SerializeAppendCommand) {
    AppendCommand cmd;
    cmd.meta.clientId = 789;
    cmd.meta.sequenceNum = 101;
    cmd.key = "key2";
    cmd.value = "value2";
    
    CommandData cmdData = cmd;
    std::vector<uint8_t> serialized = serialize(cmdData);
    
    EXPECT_FALSE(serialized.empty());
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(CommandType::APPEND));
}

TEST(CommandSerializationTest, SerializeGetCommand) {
    GetCommand cmd;
    cmd.meta.clientId = 999;
    cmd.meta.sequenceNum = 111;
    cmd.key = "getkey";
    
    CommandData cmdData = cmd;
    std::vector<uint8_t> serialized = serialize(cmdData);
    
    EXPECT_FALSE(serialized.empty());
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(CommandType::GET));
}

TEST(CommandSerializationTest, RoundTripPutCommand) {
    PutCommand original;
    original.meta.clientId = 123;
    original.meta.sequenceNum = 456;
    original.key = "testkey";
    original.value = "testvalue";
    
    CommandData cmdData = original;
    std::vector<uint8_t> serialized = serialize(cmdData);
    CommandData deserialized = deserialize(serialized);
    
    ASSERT_TRUE(std::holds_alternative<PutCommand>(deserialized));
    PutCommand& restored = std::get<PutCommand>(deserialized);
    
    EXPECT_EQ(restored.meta.clientId, original.meta.clientId);
    EXPECT_EQ(restored.meta.sequenceNum, original.meta.sequenceNum);
    EXPECT_EQ(restored.key, original.key);
    EXPECT_EQ(restored.value, original.value);
}

TEST(CommandSerializationTest, RoundTripAppendCommand) {
    AppendCommand original;
    original.meta.clientId = 789;
    original.meta.sequenceNum = 101;
    original.key = "appendkey";
    original.value = "appendval";
    
    CommandData cmdData = original;
    std::vector<uint8_t> serialized = serialize(cmdData);
    CommandData deserialized = deserialize(serialized);
    
    ASSERT_TRUE(std::holds_alternative<AppendCommand>(deserialized));
    AppendCommand& restored = std::get<AppendCommand>(deserialized);
    
    EXPECT_EQ(restored.meta.clientId, original.meta.clientId);
    EXPECT_EQ(restored.meta.sequenceNum, original.meta.sequenceNum);
    EXPECT_EQ(restored.key, original.key);
    EXPECT_EQ(restored.value, original.value);
}

TEST(CommandSerializationTest, RoundTripGetCommand) {
    GetCommand original;
    original.meta.clientId = 555;
    original.meta.sequenceNum = 222;
    original.key = "getkey";
    
    CommandData cmdData = original;
    std::vector<uint8_t> serialized = serialize(cmdData);
    CommandData deserialized = deserialize(serialized);
    
    ASSERT_TRUE(std::holds_alternative<GetCommand>(deserialized));
    GetCommand& restored = std::get<GetCommand>(deserialized);
    
    EXPECT_EQ(restored.meta.clientId, original.meta.clientId);
    EXPECT_EQ(restored.meta.sequenceNum, original.meta.sequenceNum);
    EXPECT_EQ(restored.key, original.key);
}

TEST(CommandSerializationTest, EmptyBufferThrows) {
    std::vector<uint8_t> empty;
    EXPECT_THROW(deserialize(empty), std::runtime_error);
}

TEST(CommandSerializationTest, InvalidTypeThrows) {
    std::vector<uint8_t> invalid = {99}; // Invalid command type
    EXPECT_THROW(deserialize(invalid), std::runtime_error);
}
