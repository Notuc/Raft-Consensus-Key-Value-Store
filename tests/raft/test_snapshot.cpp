#include <gtest/gtest.h>
#include "kvstore/KVStore.hpp"
#include "raft/Log.hpp"

TEST(SnapshotTest, KVStoreSnapshotRoundTrip) {
    auto kvStore = std::make_shared<KVStore>();
    
    // Add some data
    kvStore->Put("key1", "value1");
    kvStore->Put("key2", "value2");
    kvStore->Put("key3", "value3");
    
    // Take snapshot
    auto snapshot = kvStore->takeSnapshot();
    EXPECT_FALSE(snapshot.empty());
    
    // Modify store
    kvStore->Put("key4", "value4");
    kvStore->Put("key1", "modified");
    
    // Restore snapshot
    kvStore->installSnapshot(snapshot);
    
    // Verify original data is restored
    EXPECT_EQ(kvStore->Get("key1").value(), "value1");
    EXPECT_EQ(kvStore->Get("key2").value(), "value2");
    EXPECT_EQ(kvStore->Get("key3").value(), "value3");
    EXPECT_FALSE(kvStore->Get("key4").has_value());
}

TEST(SnapshotTest, LogDiscardEntries) {
    RaftLog log;
    
    // Add entries
    for (uint64_t i = 1; i <= 10; ++i) {
        LogEntry entry{i, 1, {}};
        log.append(entry);
    }
    
    EXPECT_EQ(log.getLastIndex(), 10);
    EXPECT_EQ(log.getFirstIndex(), 1);
    
    // Discard entries up to index 5
    log.discardEntriesUpTo(5, 1);
    
    EXPECT_EQ(log.getSnapshotIndex(), 5);
    EXPECT_EQ(log.getFirstIndex(), 6);
    EXPECT_EQ(log.getLastIndex(), 10);
    
    // Should still be able to get entries 6-10
    EXPECT_NO_THROW(log.getEntry(6));
    EXPECT_NO_THROW(log.getEntry(10));
    
    // Should not be able to get entries 1-5
    EXPECT_THROW(log.getEntry(5), std::out_of_range);
    EXPECT_THROW(log.getEntry(1), std::out_of_range);
}
