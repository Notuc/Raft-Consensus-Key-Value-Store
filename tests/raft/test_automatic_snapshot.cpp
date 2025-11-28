#include <gtest/gtest.h>
#include "raft/RaftNode.hpp"
#include "raft/InMemoryRPC.hpp"
#include "kvstore/KVStore.hpp"
#include <thread>
#include <chrono>

class AutomaticSnapshotTest : public ::testing::Test {
protected:
    std::shared_ptr<InMemoryRPC> rpc;
    std::shared_ptr<KVStore> kvStore;
    std::unique_ptr<RaftNode> node;
    
    void SetUp() override {
        rpc = std::make_shared<InMemoryRPC>();
        kvStore = std::make_shared<KVStore>();
    }
    
    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        node.reset();
        rpc.reset();
    }
};

TEST_F(AutomaticSnapshotTest, SnapshotTriggeredAtThreshold) {
    // Create node with small threshold for testing
    std::vector<uint64_t> peers = {2, 3};
    uint64_t snapshotThreshold = 10;  
    
    node = std::make_unique<RaftNode>(1, peers, kvStore, rpc, snapshotThreshold);
    rpc->registerNode(1, node.get());
    
    // Make node become leader 
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(node->getLogSize(), 0);
    
    // Add entries to log by sending client requests
    for (int i = 0; i < 15; i++) {
        PutCommand cmd;
        cmd.meta.clientId = 100;
        cmd.meta.sequenceNum = i;
        cmd.key = "key" + std::to_string(i);
        cmd.value = "value" + std::to_string(i);
        
        ClientRequest req;
        req.command = cmd;
        req.clientId = 100;
        req.sequenceNum = i;
        
        node->handleClientRequest(req);
    }
    
    uint64_t logSizeBefore = node->getLogSize();
    std::cout << "Log size before snapshot: " << logSizeBefore << std::endl;
    
    // Wait for automatic snapshot 
    // For testing, a manual trigger it
    node->triggerSnapshot();
    
    uint64_t logSizeAfter = node->getLogSize();
    std::cout << "Log size after snapshot: " << logSizeAfter << std::endl;
    
    // Snapshot should have been taken
    EXPECT_LT(logSizeAfter, logSizeBefore);
    EXPECT_GT(node->getSnapshotSize(), 0);
}

TEST_F(AutomaticSnapshotTest, ManualSnapshotTrigger) {
    std::vector<uint64_t> peers = {};
    node = std::make_unique<RaftNode>(1, peers, kvStore, rpc, 1000);
    rpc->registerNode(1, node.get());
    
    // Add some data to KVStore
    kvStore->Put("test1", "value1");
    kvStore->Put("test2", "value2");
    
    // Initially no snapshot
    EXPECT_EQ(node->getSnapshotSize(), 0);
    
    // Manually trigger snapshot
    node->triggerSnapshot();
    
    // Should have snapshot now
    EXPECT_GT(node->getSnapshotSize(), 0);
}

TEST_F(AutomaticSnapshotTest, SnapshotPreservesData) {
    std::vector<uint64_t> peers = {};
    node = std::make_unique<RaftNode>(1, peers, kvStore, rpc, 100);
    rpc->registerNode(1, node.get());
    
    kvStore->Put("key1", "value1");
    kvStore->Put("key2", "value2");
    
    // Take snapshot
    node->triggerSnapshot();
    
    // Data should still be accessible
    EXPECT_EQ(kvStore->Get("key1").value(), "value1");
    EXPECT_EQ(kvStore->Get("key2").value(), "value2");
}
