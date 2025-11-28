#include <gtest/gtest.h>
#include "raft/RaftNode.hpp"
#include "raft/InMemoryRPC.hpp"
#include "kvstore/KVStore.hpp"
#include <thread>
#include <chrono>

class RaftNodeTest : public ::testing::Test {
protected:
    std::shared_ptr<InMemoryRPC> rpc;
    
    void SetUp() override {
        rpc = std::make_shared<InMemoryRPC>();
    }
    
    void TearDown() override {
        rpc.reset();
    }
};

TEST_F(RaftNodeTest, InitialStateIsFollower) {
    std::vector<uint64_t> peers = {2, 3};
    auto kvStore = std::make_shared<KVStore>();  
    RaftNode node(1, peers, kvStore, rpc);  
    
    EXPECT_EQ(node.getCurrentRole(), Role::FOLLOWER);
    EXPECT_EQ(node.getCurrentTerm(), 0);
    EXPECT_EQ(node.getCommitIndex(), 0);
    EXPECT_EQ(node.getLastApplied(), 0);
}

TEST_F(RaftNodeTest, RequestVoteRejectsOldTerm) {
    std::vector<uint64_t> peers = {2};
    auto kvStore = std::make_shared<KVStore>();  
    RaftNode node(1, peers, kvStore, rpc);  
        
    RequestVoteArgs advanceArgs;
    advanceArgs.term = 2;
    advanceArgs.candidateId = 2;
    advanceArgs.lastLogIndex = 0;
    advanceArgs.lastLogTerm = 0;
    node.handleRequestVote(advanceArgs);  // Node is now at term 2
    
    // Send RequestVote with old term (term 1)
    RequestVoteArgs args;
    args.term = 1;  // Old term - less than node's current term (2)
    args.candidateId = 2;
    args.lastLogIndex = 0;
    args.lastLogTerm = 0;
    
    RequestVoteReply reply = node.handleRequestVote(args);
    
    EXPECT_FALSE(reply.voteGranted);
    EXPECT_EQ(reply.term, 2);  // Node's current term
}

TEST_F(RaftNodeTest, RequestVoteGrantsVoteToUpToDateCandidate) {
    std::vector<uint64_t> peers = {2};
    auto kvStore = std::make_shared<KVStore>();  
    RaftNode node(1, peers, kvStore, rpc); 

    RequestVoteArgs args;
    args.term = 1; // Higher term
    args.candidateId = 2;
    args.lastLogIndex = 0;
    args.lastLogTerm = 0;
    
    RequestVoteReply reply = node.handleRequestVote(args);
    
    EXPECT_TRUE(reply.voteGranted);
    EXPECT_EQ(node.getCurrentTerm(), 1);
}

TEST_F(RaftNodeTest, RequestVoteOnlyGrantsOneVotePerTerm) {
    std::vector<uint64_t> peers = {2, 3};
     auto kvStore = std::make_shared<KVStore>();  
    RaftNode node(1, peers, kvStore, rpc);  
    
    RequestVoteArgs args1;
    args1.term = 1;
    args1.candidateId = 2;
    args1.lastLogIndex = 0;
    args1.lastLogTerm = 0;
    
    RequestVoteReply reply1 = node.handleRequestVote(args1);
    EXPECT_TRUE(reply1.voteGranted);
    
    // Try to vote for different candidate in same term
    RequestVoteArgs args2;
    args2.term = 1;
    args2.candidateId = 3;
    args2.lastLogIndex = 0;
    args2.lastLogTerm = 0;
    
    RequestVoteReply reply2 = node.handleRequestVote(args2);
    EXPECT_FALSE(reply2.voteGranted);
}

TEST_F(RaftNodeTest, AppendEntriesUpdatesCommitIndex) {
    std::vector<uint64_t> peers = {2};
    auto kvStore = std::make_shared<KVStore>();  
    RaftNode node(1, peers, kvStore, rpc);  
    
    AppendEntriesArgs args;
    args.term = 1;
    args.leaderId = 2;
    args.prevLogIndex = 0;
    args.prevLogTerm = 0;
    args.leaderCommit = 0;
    args.entries = {}; // Empty heartbeat
    
    AppendEntriesReply reply = node.handleAppendEntries(args);
    
    EXPECT_TRUE(reply.success);
    EXPECT_EQ(node.getCurrentTerm(), 1);
    EXPECT_EQ(node.getCurrentRole(), Role::FOLLOWER);
}

TEST_F(RaftNodeTest, AppendEntriesRejectsOldTerm) {
    std::vector<uint64_t> peers = {2};
    auto kvStore = std::make_shared<KVStore>();  
    RaftNode node(1, peers, kvStore, rpc);  
    
    // Advance node's term
    RequestVoteArgs voteArgs;
    voteArgs.term = 5;
    voteArgs.candidateId = 2;
    voteArgs.lastLogIndex = 0;
    voteArgs.lastLogTerm = 0;
    node.handleRequestVote(voteArgs);
    
    // Try AppendEntries with old term
    AppendEntriesArgs args;
    args.term = 3; // Old term
    args.leaderId = 2;
    args.prevLogIndex = 0;
    args.prevLogTerm = 0;
    args.leaderCommit = 0;
    
    AppendEntriesReply reply = node.handleAppendEntries(args);
    
    EXPECT_FALSE(reply.success);
    EXPECT_EQ(reply.term, 5); // Node's current term
}

TEST_F(RaftNodeTest, ClientRequestRedirectsWhenNotLeader) {
    std::vector<uint64_t> peers = {2};
    auto kvStore = std::make_shared<KVStore>();   
    RaftNode node(1, peers, kvStore, rpc);     
    
    PutCommand cmd;
    cmd.meta.clientId = 100;
    cmd.meta.sequenceNum = 1;
    cmd.key = "key";
    cmd.value = "value";
    
    ClientRequest req;
    req.command = cmd;
    req.clientId = 100;
    req.sequenceNum = 1;
    
    ClientReply reply = node.handleClientRequest(req);
    
    EXPECT_FALSE(reply.success);
    EXPECT_FALSE(reply.isLeader);
    EXPECT_EQ(reply.error, "Not leader");
}
