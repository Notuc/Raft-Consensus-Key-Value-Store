#include <gtest/gtest.h>
#include "raft/RaftNode.hpp"
#include "raft/InMemoryRPC.hpp"
#include "kvstore/KVStore.hpp"
#include <thread>
#include <chrono>
#include <memory>

class RaftClusterTest : public ::testing::Test {
protected:
    std::shared_ptr<InMemoryRPC> rpc;
    std::unique_ptr<RaftNode> node1;
    std::unique_ptr<RaftNode> node2;
    std::unique_ptr<RaftNode> node3;
    
    void SetUp() override {
        rpc = std::make_shared<InMemoryRPC>();
        // Create KVStores
        auto kvStore1 = std::make_shared<KVStore>();
        auto kvStore2 = std::make_shared<KVStore>();
        auto kvStore3 = std::make_shared<KVStore>();

        // Create 3-node cluster
        std::vector<uint64_t> peers1 = {2, 3};
        std::vector<uint64_t> peers2 = {1, 3};
        std::vector<uint64_t> peers3 = {1, 2};
        
        node1 = std::make_unique<RaftNode>(1, peers1, kvStore1, rpc, 1000) ;
        node2 = std::make_unique<RaftNode>(2, peers2, kvStore2, rpc, 1000);
        node3 = std::make_unique<RaftNode>(3, peers3, kvStore3, rpc, 1000);
        
        rpc->registerNode(1, node1.get());
        rpc->registerNode(2, node2.get());
        rpc->registerNode(3, node3.get());
    }
    
    void TearDown() override {
      // Give threads time to finish before destroying nodes
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
      node1.reset();
      node2.reset();
      node3.reset();
      rpc.reset();
    }    

    RaftNode* getLeader() {
        if (node1->getCurrentRole() == Role::LEADER) return node1.get();
        if (node2->getCurrentRole() == Role::LEADER) return node2.get();
        if (node3->getCurrentRole() == Role::LEADER) return node3.get();
        return nullptr;
    }
    
    int countLeaders() {
        int count = 0;
        if (node1->getCurrentRole() == Role::LEADER) count++;
        if (node2->getCurrentRole() == Role::LEADER) count++;
        if (node3->getCurrentRole() == Role::LEADER) count++;
        return count;
    }
};

TEST_F(RaftClusterTest, LeaderElectionSucceeds) {
    // Wait for election to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Exactly one leader should be elected
    EXPECT_EQ(countLeaders(), 1);
    
    // All nodes should be in the same term
    uint64_t term1 = node1->getCurrentTerm();
    uint64_t term2 = node2->getCurrentTerm();
    uint64_t term3 = node3->getCurrentTerm();
    
    EXPECT_EQ(term1, term2);
    EXPECT_EQ(term2, term3);
    EXPECT_GT(term1, 0); // Term should have advanced
}

TEST_F(RaftClusterTest, AllNodesStartAsFollowers) {
    EXPECT_EQ(node1->getCurrentRole(), Role::FOLLOWER);
    EXPECT_EQ(node2->getCurrentRole(), Role::FOLLOWER);
    EXPECT_EQ(node3->getCurrentRole(), Role::FOLLOWER);
}

TEST_F(RaftClusterTest, LeaderReceivesClientRequests) {
    // Wait for leader election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    RaftNode* leader = getLeader();
    ASSERT_NE(leader, nullptr);
    
    // Send client request
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
    EXPECT_TRUE(reply.isLeader);
}

TEST_F(RaftClusterTest, FollowersRedirectClientRequests) {
    // Wait for leader election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Find a follower
    RaftNode* follower = nullptr;
    if (node1->getCurrentRole() == Role::FOLLOWER) follower = node1.get();
    else if (node2->getCurrentRole() == Role::FOLLOWER) follower = node2.get();
    else if (node3->getCurrentRole() == Role::FOLLOWER) follower = node3.get();
    
    ASSERT_NE(follower, nullptr);
    
    // Send client request to follower
    PutCommand cmd;
    cmd.meta.clientId = 100;
    cmd.meta.sequenceNum = 1;
    cmd.key = "key";
    cmd.value = "value";
    
    ClientRequest req;
    req.command = cmd;
    req.clientId = 100;
    req.sequenceNum = 1;
    
    ClientReply reply = follower->handleClientRequest(req);
    
    EXPECT_FALSE(reply.success);
    EXPECT_FALSE(reply.isLeader);
}

TEST_F(RaftClusterTest, LogReplicationToAllNodes) {
    // Wait for leader election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    RaftNode* leader = getLeader();
    ASSERT_NE(leader, nullptr);
    
    // Send client request
    PutCommand cmd;
    cmd.meta.clientId = 100;
    cmd.meta.sequenceNum = 1;
    cmd.key = "replicatedkey";
    cmd.value = "replicatedvalue";
    
    ClientRequest req;
    req.command = cmd;
    req.clientId = 100;
    req.sequenceNum = 1;
    
    leader->handleClientRequest(req);
    
    // Wait for replication
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // All nodes should have committed the entry
    uint64_t commit1 = node1->getCommitIndex();
    uint64_t commit2 = node2->getCommitIndex();
    uint64_t commit3 = node3->getCommitIndex();
    
    EXPECT_GT(commit1, 0);
    EXPECT_EQ(commit1, commit2);
    EXPECT_EQ(commit2, commit3);
}

TEST_F(RaftClusterTest, NewLeaderElectedAfterFailure) {
    // Wait for initial leader election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    RaftNode* originalLeader = getLeader();
    ASSERT_NE(originalLeader, nullptr);
    
    uint64_t originalLeaderId = 0;
    if (originalLeader == node1.get()) originalLeaderId = 1;
    else if (originalLeader == node2.get()) originalLeaderId = 2;
    else if (originalLeader == node3.get()) originalLeaderId = 3;
    
    std::cout << "Original leader: Node " << originalLeaderId << std::endl;
    
    
// Simulate leader failure: stop leader node and unregister it
if (originalLeaderId == 1) {
    rpc->unregisterNode(1);
    node1.reset();   // destructor stops election/heartbeat threads
} else if (originalLeaderId == 2) {
    rpc->unregisterNode(2);
    node2.reset();
} else if (originalLeaderId == 3) {
    rpc->unregisterNode(3);
    node3.reset();
}
    
    // Wait for new election - try multiple times
    // Election timeout is 150-300ms, so elections can happen multiple times
    bool newLeaderFound = false;
    for (int attempt = 0; attempt < 5 && !newLeaderFound; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        
        int leaderCount = 0;
        if (originalLeaderId != 1 && node1->getCurrentRole() == Role::LEADER) leaderCount++;
        if (originalLeaderId != 2 && node2->getCurrentRole() == Role::LEADER) leaderCount++;
        if (originalLeaderId != 3 && node3->getCurrentRole() == Role::LEADER) leaderCount++;
        
        if (leaderCount == 1) {
            newLeaderFound = true;
            std::cout << "New leader found on attempt " << (attempt + 1) << std::endl;
        } else {
            std::cout << "Attempt " << (attempt + 1) << ": leaderCount=" << leaderCount << std::endl;
        }
    }
    
    // Final check
    int leaderCount = 0;
    if (originalLeaderId != 1 && node1->getCurrentRole() == Role::LEADER) {
        leaderCount++;
        std::cout << "Node 1 is leader (term " << node1->getCurrentTerm() << ")" << std::endl;
    }
    if (originalLeaderId != 2 && node2->getCurrentRole() == Role::LEADER) {
        leaderCount++;
        std::cout << "Node 2 is leader (term " << node2->getCurrentTerm() << ")" << std::endl;
    }
    if (originalLeaderId != 3 && node3->getCurrentRole() == Role::LEADER) {
        leaderCount++;
        std::cout << "Node 3 is leader (term " << node3->getCurrentTerm() << ")" << std::endl;
    }
    
    // Print final state for debugging
    if (originalLeaderId != 1) {
        std::cout << "Node 1: term=" << node1->getCurrentTerm() 
                  << ", role=" << (node1->getCurrentRole() == Role::LEADER ? "LEADER" :
                                   node1->getCurrentRole() == Role::CANDIDATE ? "CANDIDATE" : "FOLLOWER")
                  << std::endl;
    }
    if (originalLeaderId != 2) {
        std::cout << "Node 2: term=" << node2->getCurrentTerm()
                  << ", role=" << (node2->getCurrentRole() == Role::LEADER ? "LEADER" :
                                   node2->getCurrentRole() == Role::CANDIDATE ? "CANDIDATE" : "FOLLOWER")
                  << std::endl;
    }
    if (originalLeaderId != 3) {
        std::cout << "Node 3: term=" << node3->getCurrentTerm()
                  << ", role=" << (node3->getCurrentRole() == Role::LEADER ? "LEADER" :
                                   node3->getCurrentRole() == Role::CANDIDATE ? "CANDIDATE" : "FOLLOWER")
                  << std::endl;
    }
    
    EXPECT_EQ(leaderCount, 1) << "Expected exactly 1 new leader after original leader failed";
}
