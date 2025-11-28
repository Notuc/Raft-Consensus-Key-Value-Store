#include <gtest/gtest.h>
#include "raft/RaftNode.hpp"
#include "raft/InMemoryRPC.hpp"
#include "kvstore/KVStore.hpp"
#include <thread>
#include <chrono>

class KVStoreIntegrationTest : public ::testing::Test {
protected:
    std::shared_ptr<InMemoryRPC> rpc;
    std::shared_ptr<KVStore> kvStore1;
    std::shared_ptr<KVStore> kvStore2;
    std::shared_ptr<KVStore> kvStore3;
    std::unique_ptr<RaftNode> node1;
    std::unique_ptr<RaftNode> node2;
    std::unique_ptr<RaftNode> node3;
    
    void SetUp() override {
        rpc = std::make_shared<InMemoryRPC>();
        
        // Create separate KVStores for each node
        kvStore1 = std::make_shared<KVStore>();
        kvStore2 = std::make_shared<KVStore>();
        kvStore3 = std::make_shared<KVStore>();
        
        // Create 3-node cluster
        std::vector<uint64_t> peers1 = {2, 3};
        std::vector<uint64_t> peers2 = {1, 3};
        std::vector<uint64_t> peers3 = {1, 2};
        
        node1 = std::make_unique<RaftNode>(1, peers1, kvStore1, rpc, 1000);
        node2 = std::make_unique<RaftNode>(2, peers2, kvStore2, rpc, 1000);
        node3 = std::make_unique<RaftNode>(3, peers3, kvStore3, rpc, 1000);
        
        rpc->registerNode(1, node1.get());
        rpc->registerNode(2, node2.get());
        rpc->registerNode(3, node3.get());
    };
    
   void TearDown() override {
      // Give threads time to finish before destroying nodes
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
      node1.reset();
      node2.reset();
      node3.reset();
      rpc.reset();
    };

    RaftNode* getLeader() {
        if (node1->getCurrentRole() == Role::LEADER) return node1.get();
        if (node2->getCurrentRole() == Role::LEADER) return node2.get();
        if (node3->getCurrentRole() == Role::LEADER) return node3.get();
        return nullptr;
    }
};

TEST_F(KVStoreIntegrationTest, PutCommandReplicatesToAllNodes) {
    // Wait for leader election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    RaftNode* leader = getLeader();
    ASSERT_NE(leader, nullptr);
    
    // Send PUT command
    PutCommand cmd;
    cmd.meta.clientId = 100;
    cmd.meta.sequenceNum = 1;
    cmd.key = "testkey";
    cmd.value = "testvalue";
    
    ClientRequest req;
    req.command = cmd;
    req.clientId = 100;
    req.sequenceNum = 1;
    
    ClientReply reply = leader->handleClientRequest(req);
    EXPECT_TRUE(reply.success);
    
    // Wait for replication and commit
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // All KVStores should have the value
    EXPECT_EQ(kvStore1->Get("testkey"), "testvalue");
    EXPECT_EQ(kvStore2->Get("testkey"), "testvalue");
    EXPECT_EQ(kvStore3->Get("testkey"), "testvalue");
}

TEST_F(KVStoreIntegrationTest, AppendCommandWorks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    RaftNode* leader = getLeader();
    ASSERT_NE(leader, nullptr);
    
    PutCommand putCmd;
    putCmd.meta.clientId = 100;
    putCmd.meta.sequenceNum = 1;
    putCmd.key = "key";
    putCmd.value = "hello";
    
    ClientRequest putReq;
    putReq.command = putCmd;
    putReq.clientId = 100;
    putReq.sequenceNum = 1;
    
    leader->handleClientRequest(putReq);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    AppendCommand appendCmd;
    appendCmd.meta.clientId = 100;
    appendCmd.meta.sequenceNum = 2;
    appendCmd.key = "key";
    appendCmd.value = "world";
    
    ClientRequest appendReq;
    appendReq.command = appendCmd;
    appendReq.clientId = 100;
    appendReq.sequenceNum = 2;
    
    leader->handleClientRequest(appendReq);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // All nodes should have "helloworld"
    EXPECT_EQ(kvStore1->Get("key"), "helloworld");
    EXPECT_EQ(kvStore2->Get("key"), "helloworld");
    EXPECT_EQ(kvStore3->Get("key"), "helloworld");
}

TEST_F(KVStoreIntegrationTest, DeduplicationPreventsDoubleApply) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    RaftNode* leader = getLeader();
    ASSERT_NE(leader, nullptr);
    
    // Send same command twice
    PutCommand cmd;
    cmd.meta.clientId = 100;
    cmd.meta.sequenceNum = 1;
    cmd.key = "counter";
    cmd.value = "1";
    
    ClientRequest req;
    req.command = cmd;
    req.clientId = 100;
    req.sequenceNum = 1;
    
    // First request
    leader->handleClientRequest(req);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Duplicate request
    ClientReply reply = leader->handleClientRequest(req);
    EXPECT_TRUE(reply.success);  // Should succeed (idempotent)
    
    // Value should still be "1", not applied twice
    EXPECT_EQ(kvStore1->Get("counter"), "1");
}
