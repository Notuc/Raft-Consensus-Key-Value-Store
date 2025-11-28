#include "raft/gRPCRaftClient.hpp"
#include "kvstore/Command.hpp"
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] 
                  << " <command> <key> [value] <node1_address> [node2_address] ..." 
                  << std::endl;
        std::cerr << "Commands: put, get, append" << std::endl;
        std::cerr << "Example: " << argv[0] 
                  << " put mykey myvalue localhost:5001 localhost:5002 localhost:5003" 
                  << std::endl;
        return 1;
    }
    
    std::string command = argv[1];
    std::string key = argv[2];
    std::string value;
    
    int addressStart = 3;
    if (command == "put" || command == "append") {
        if (argc < 4) {
            std::cerr << "Error: " << command << " requires a value" << std::endl;
            return 1;
        }
        value = argv[3];
        addressStart = 4;
    }
    
    std::vector<std::string> nodeAddresses;
    for (int i = addressStart; i < argc; i++) {
        nodeAddresses.push_back(argv[i]);
    }
    
    if (nodeAddresses.empty()) {
        std::cerr << "Error: No node addresses provided" << std::endl;
        return 1;
    }
    
    // RPC client
    auto rpcClient = std::make_shared<GrpcRaftClient>();
    for (size_t i = 0; i < nodeAddresses.size(); i++) {
        rpcClient->addPeer(i + 1, nodeAddresses[i]);
    }
    
    // Command
    CommandData cmdData;
    ClientRequestMeta meta{100, 1};      
    if (command == "put") {
        PutCommand cmd{meta, key, value};
        cmdData = cmd;
    } else if (command == "append") {
        AppendCommand cmd{meta, key, value};
        cmdData = cmd;
    } else if (command == "get") {
        GetCommand cmd{meta, key};
        cmdData = cmd;
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }
    
    // Looping and try each node until we find the leader
    for (size_t i = 0; i < nodeAddresses.size(); i++) {
        std::cout << "Trying node " << (i + 1) << " at " << nodeAddresses[i] << "..." << std::endl;
        if (!value.empty()) {
            std::cout << " = " << value;
        }
        std::cout << std::endl;
        std::cout << "Note: Client RPC not yet implemented in this example" << std::endl;
        break;
    }
    
    return 0;
}
