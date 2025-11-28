#include <gtest/gtest.h>
#include "raft/Log.hpp"

class LogTest : public ::testing::Test {
protected:
    RaftLog log;
    
    void SetUp() override {
    }
};

TEST_F(LogTest, InitialState) {
    EXPECT_EQ(log.getLastIndex(), 0);
    EXPECT_EQ(log.getLastTerm(), 0);
}

TEST_F(LogTest, AppendSingleEntry) {
    LogEntry entry;
    entry.index = 1;
    entry.term = 1;
    entry.command_data = {1, 2, 3};
    
    log.append(entry);
    
    EXPECT_EQ(log.getLastIndex(), 1);
    EXPECT_EQ(log.getLastTerm(), 1);
}

TEST_F(LogTest, AppendMultipleEntries) {
    for (uint64_t i = 1; i <= 5; i++) {
        LogEntry entry;
        entry.index = i;
        entry.term = 1;
        entry.command_data = {static_cast<uint8_t>(i)};
        log.append(entry);
    }
    
    EXPECT_EQ(log.getLastIndex(), 5);
    EXPECT_EQ(log.getLastTerm(), 1);
}

TEST_F(LogTest, GetEntry) {
    LogEntry entry;
    entry.index = 1;
    entry.term = 2;
    entry.command_data = {10, 20, 30};
    log.append(entry);
    
    const LogEntry& retrieved = log.getEntry(1);
    EXPECT_EQ(retrieved.index, 1);
    EXPECT_EQ(retrieved.term, 2);
    EXPECT_EQ(retrieved.command_data.size(), 3);
}

TEST_F(LogTest, GetTerm) {
    LogEntry e1{1, 1, {}};
    LogEntry e2{2, 2, {}};
    LogEntry e3{3, 2, {}};
    
    log.append(e1);
    log.append(e2);
    log.append(e3);
    
    EXPECT_EQ(log.getTerm(0), 0);
    EXPECT_EQ(log.getTerm(1), 1);
    EXPECT_EQ(log.getTerm(2), 2);
    EXPECT_EQ(log.getTerm(3), 2);
    EXPECT_EQ(log.getTerm(4), 0); // Out of bounds
}

TEST_F(LogTest, TruncateFromMiddle) {
    for (uint64_t i = 1; i <= 5; i++) {
        LogEntry entry{i, 1, {}};
        log.append(entry);
    }
    
    log.truncate(3); // Remove entries 3, 4, 5
    
    EXPECT_EQ(log.getLastIndex(), 2);
    EXPECT_EQ(log.getTerm(3), 0); // Entry 3 no longer exists
}

TEST_F(LogTest, TruncateAll) {
    LogEntry e1{1, 1, {}};
    log.append(e1);
    
    log.truncate(1);
    
    EXPECT_EQ(log.getLastIndex(), 0); // Back to dummy entry only
}

TEST_F(LogTest, InvalidIndexThrows) {
    EXPECT_THROW(log.getEntry(1), std::out_of_range);
}

TEST_F(LogTest, NonSequentialIndexThrows) {
    LogEntry e1{1, 1, {}};
    log.append(e1);
    
    LogEntry e3{3, 1, {}}; // Skipping index 2
    EXPECT_THROW(log.append(e3), std::logic_error);
}
