#include "raft/RaftNode.hpp"
#include "raft/gRPCRaftClient.hpp"
#include "raft/gRPCRaftService.hpp"
#include "kvstore/KVStore.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <csignal>

std::unique_ptr<GrpcRaftServer> server;

void signalHandler(int signal) {
    std::cout << "\nShutting down..." << std::endl;
    if (server) {
        server->stop();
    }
    exit(0);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] 
                  << " <node_id> <listen_address> <peer1_address> [peer2_address] ..." 
                  << std::endl;
        std::cerr << "Example: " << argv[0] 
                  << " 1 localhost:5001 localhost:5002 localhost:5003" 
                  << std::endl;
        return 1;
    }
    
    // Parse arguments
    uint64_t nodeId = std::stoull(argv[1]);
    std::string listenAddress = argv[2];
    
    std::vector<uint64_t> peerIds;
    std::vector<std::string> peerAddresses;
    
    for (int i = 3; i < argc; i++) {
        peerIds.push_back(i - 2); 
        peerAddresses.push_back(argv[i]);
    }
    
    std::cout << "=== Starting Raft Node ===" << std::endl;
    std::cout << "Node ID: " << nodeId << std::endl;
    std::cout << "Listen Address: " << listenAddress << std::endl;
    std::cout << "Peers: " << std::endl;
    for (size_t i = 0; i < peerIds.size(); i++) {
        std::cout << "  - Node " << peerIds[i] << " at " << peerAddresses[i] << std::endl;
    }
    
    // Components
    auto kvStore = std::make_shared<KVStore>();
    auto rpcClient = std::make_shared<GrpcRaftClient>();
    
    // Register peers
    for (size_t i = 0; i < peerIds.size(); i++) {
        rpcClient->addPeer(peerIds[i], peerAddresses[i]);
    }
    
    // Creating a Raft node with persistence
    std::string stateDir = "raft_data_node_" + std::to_string(nodeId);
    auto node = std::make_unique<RaftNode>(
        nodeId, 
        peerIds, 
        rpcClient, 
        kvStore,
        10000,      // Snapshot threshold
        stateDir    // Persistence directory
    );
    
    // gRPC server
    server = std::make_unique<GrpcRaftServer>(node.get(), listenAddress);
    server->start();
    
    std::cout << "Node " << nodeId << " is running on " << listenAddress << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print status every 5 seconds
        static int counter = 0;
        if (++counter % 5 == 0) {
            Role role = node->getCurrentRole();
            std::string roleStr = (role == Role::LEADER) ? "LEADER" :
                                 (role == Role::CANDIDATE) ? "CANDIDATE" : "FOLLOWER";
            
            std::cout << "Node " << nodeId 
                      << " | Role: " << roleStr
                      << " | Term: " << node->getCurrentTerm()
                      << " | Commit: " << node->getCommitIndex()
                      << " | Log Size: " << node->getLogSize()
                      << std::endl;
        }
    }
    
    return 0;
}
