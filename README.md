# Raft Consensus Key-Value Store

A distributed key-value store implementing the **Raft consensus algorithm** from scratch in C++. This project demonstrates a complete implementation of Raft with leader election, log replication, snapshotting, persistence, and gRPC-based network communication.

[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg)](https://cmake.org/)
[![gRPC](https://img.shields.io/badge/gRPC-1.76-00ADD8.svg)](https://grpc.io/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

## Table of Contents

- [Features](#features)
- [Architecture](#architecture)
- [Building the Project](#building-the-project)
- [Running a Cluster](#running-a-cluster)
- [Project Structure](#project-structure)
- [Implementation Details](#implementation-details)
- [Testing](#testing)
- [Performance](#performance)
- [Contributing](#contributing)

---

## Features

### Core Raft Algorithm
- **Leader Election** - Automatic leader election with randomized timeouts
- **Log Replication** - Consistent log replication across all nodes
- **Safety** - Guarantees consistency even under network failures
- **Log Compaction** - Automatic snapshots to prevent unbounded log growth
- **Crash Recovery** - Persistent state survives node restarts

### Production-Ready Features
- **gRPC Communication** - Real network communication between nodes
- **Disk Persistence** - All state saved to disk for crash recovery
- **Automatic Snapshots** - Configurable snapshot thresholds
- **Background Threads** - Separate threads for election, heartbeat, and snapshot management
- **Comprehensive Testing** - Unit, integration, and cluster tests
- **Status Monitoring** - Real-time cluster state reporting

### Key-Value Store Operations
- **PUT** - Store a key-value pair
- **GET** - Retrieve a value by key
- **APPEND** - Append to existing value

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Raft Cluster (3+ nodes)                 │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────┐      ┌──────────┐      ┌──────────┐          │
│  │  Node 1  │      │  Node 2  │      │  Node 3  │          │
│  │ (Leader) │◄────►│(Follower)│◄────►│(Follower)│          │
│  └──────────┘      └──────────┘      └──────────┘          │
│       │                  │                  │                │
│       │                  │                  │                │
│  ┌────▼──────────────────▼──────────────────▼────┐          │
│  │           gRPC Network Layer                   │          │
│  └────────────────────────────────────────────────┘          │
│                                                               │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
                   ┌──────────────┐
                   │   Clients    │
                   │  (Read/Write)│
                   └──────────────┘
```

### Component Breakdown

**RaftNode** - Core consensus engine
- Implements leader election
- Manages log replication
- Handles state machine application

**RaftLog** - Persistent replicated log
- Stores command entries
- Supports log compaction via snapshots
- Efficient index-based access

**KVStore** - State machine
- In-memory key-value storage
- Snapshot/restore capabilities
- Thread-safe operations

**gRPC Layer** - Network communication
- `GrpcRaftService` - Server-side RPC handlers
- `GrpcRaftClient` - Client-side RPC sender
- Protocol Buffer message serialization

**Persistence** - Crash recovery
- Saves Raft state to disk
- Stores snapshots separately
- Atomic write operations

---

## Building the Project

### Prerequisites

- **C++20 compatible compiler** (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake 3.20+**
- **Git** (for fetching dependencies)

### Dependencies (Auto-fetched)

The following dependencies are automatically downloaded via CMake FetchContent:
- gRPC (v1.76.0) - Includes Protocol Buffers
- GoogleTest (v1.15.0)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/raft-kvstore.git
cd raft-kvstore

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build (use -j for parallel compilation)
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Build Outputs

After building, you'll have:
- `raft_server` - Raft node executable
- `raft_client` - Client CLI tool
- Test executables (`log_test`, `command_test`, `raft_node_test`, etc.)

---

## Running a Cluster

### Starting a 3-Node Cluster

**Terminal 1 - Node 1:**
```bash
./raft_server 1 localhost:5001 localhost:5002 localhost:5003
```

**Terminal 2 - Node 2:**
```bash
./raft_server 2 localhost:5002 localhost:5001 localhost:5003
```

**Terminal 3 - Node 3:**
```bash
./raft_server 3 localhost:5003 localhost:5001 localhost:5002
```

### Expected Output

**Initial Startup (Node 1):**
```
=== Starting Raft Node ===
Node ID: 1
Listen Address: localhost:5001
Peers: 
  - Node 2 at localhost:5002
  - Node 3 at localhost:5003
Added gRPC peer 2 at localhost:5002
Added gRPC peer 3 at localhost:5003
Node 1 election timer loop started
Node 1 is running on localhost:5001
Press Ctrl+C to stop
```

**Leader Election:**
```
Node 1 ELECTION TIMEOUT! Starting election...
Node 1 starting election in term 1
Node 1 needs 2 votes
Node 1 got vote response from 2: granted=1, term=1
Node 1 got vote response from 3: granted=1, term=1
Node 1 received 3 votes
Node 1 WON ELECTION! Becoming leader
Node 1 | Role: LEADER | Term: 1 | Commit: 0 | Log Size: 0
```

**Steady State (Periodic Status):**
```
Node 1 | Role: LEADER | Term: 1 | Commit: 5 | Log Size: 5
Node 2 | Role: FOLLOWER | Term: 1 | Commit: 5 | Log Size: 5
Node 3 | Role: FOLLOWER | Term: 1 | Commit: 5 | Log Size: 5
```

**Automatic Snapshot:**
```
Node 1 taking automatic snapshot at index 10000
Node 1 snapshot complete. Log size: 0 entries
```

### Using the Client

**PUT Operation:**
```bash
./raft_client put mykey myvalue localhost:5001 localhost:5002 localhost:5003
```

**GET Operation:**
```bash
./raft_client get mykey localhost:5001 localhost:5002 localhost:5003
```

**APPEND Operation:**
```bash
./raft_client append mykey moredata localhost:5001 localhost:5002 localhost:5003
```

**Client Output:**
```
Trying node 1 at localhost:5001...
Command: put mykey = myvalue
Note: Client RPC not yet implemented in this example
```

---

## Project Structure

```
raft-kvstore/
├── CMakeLists.txt                 # Root build configuration
├── README.md                      # This file
├── proto/
│   └── raft.proto                # gRPC service definitions
├── include/
│   ├── kvstore/
│   │   ├── KVStore.hpp           # Key-value store interface
│   │   └── Command.hpp           # Command serialization
│   └── raft/
│       ├── RaftNode.hpp          # Core Raft implementation
│       ├── RaftRPC.hpp           # RPC interface
│       ├── Log.hpp               # Replicated log
│       ├── InMemoryRPC.hpp       # In-memory RPC (testing)
│       ├── GrpcRaftService.hpp   # gRPC server
│       ├── GrpcRaftClient.hpp    # gRPC client
│       └── RaftPersistence.hpp   # Disk persistence
├── src/
│   ├── CMakeLists.txt
│   ├── kvstore/
│   │   ├── KVStore.cpp
│   │   └── Command.cpp
│   ├── raft/
│   │   ├── RaftNode.cpp
│   │   ├── Log.cpp
│   │   ├── InMemoryRPC.cpp
│   │   ├── GrpcRaftService.cpp
│   │   ├── GrpcRaftClient.cpp
│   │   └── RaftPersistence.cpp
│   ├── raft_main.cpp             # Server executable
│   └── raft_client.cpp           # Client executable
└── tests/
    ├── CMakeLists.txt
    ├── raft/
    │   ├── test_log.cpp
    │   ├── test_command.cpp
    │   ├── test_raft_node.cpp
    │   ├── test_raft_cluster.cpp
    │   ├── test_snapshot.cpp
    │   └── test_automatic_snapshot.cpp
    └── kvstore/
        └── kvStore_test.cpp
```

---

## Implementation Details

### Raft Consensus Algorithm by Diego Ongaro and John Ousterhout.

**Key Methods:**
- **Election Safety** - At most one leader per term
- **Leader Append-Only** - Leaders never overwrite log entries
- **Log Matching** - If two logs contain an entry with same index and term, all preceding entries match
- **Leader Completeness** - If a log entry is committed, it will be present in all future leader logs
- **State Machine Safety** - If a server applies a log entry at a given index, no other server applies a different entry at that index

### State Machine

```cpp
// Commands are serialized and stored in the log
CommandData = variant<PutCommand, AppendCommand, GetCommand>

// Each command has metadata for deduplication
struct ClientRequestMeta {
    uint64_t clientId;
    uint64_t sequenceNum;
};
```


### Thread Architecture

Each `RaftNode` runs three background threads:

1. **Election Timer Thread** - Monitors leader heartbeats, triggers elections on timeout
2. **Heartbeat Thread** - Leader sends periodic heartbeats to followers
3. **Snapshot Thread** - Checks log size and takes snapshots when threshold exceeded

All threads share state protected by a mutex .

---

## Testing

### Test Suite

The project includes comprehensive tests:

**Unit Tests:**
- `test_log.cpp` - Log operations (append, truncate, snapshot)
- `test_command.cpp` - Command serialization/deserialization
- `test_snapshot.cpp` - Snapshot creation and restoration

**Integration Tests:**
- `test_raft_node.cpp` - Single node behavior
- `test_raft_cluster.cpp` - Multi-node consensus
- `test_automatic_snapshot.cpp` - Automatic snapshot triggering
- `test_kvstore_integration.cpp` - End-to-end KV operations

### Running Tests

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific test suite
./log_test
./raft_cluster_test

# Run with verbose output
./raft_cluster_test --gtest_print_time=1

# Run specific test case
./raft_cluster_test --gtest_filter="*LeaderElection*"
```

### Test Coverage

- Leader election with multiple nodes
- Log replication and consistency
- Follower crash and recovery
- Leader failure and re-election
- Split brain prevention
- Snapshot creation and installation
- Persistent state recovery
- Command deduplication

---

## Future Enhancements

- Membership changes (adding/removing nodes)
- Read-only queries without log entries
- Prometheus metrics export
- Client retry logic and leader discovery
- Linearizable reads
- Batch request processing
- TLS/SSL for gRPC connections
- Disk-based log storage (vs in-memory)

---

## References

- [Raft Paper](https://raft.github.io/raft.pdf) - In Search of an Understandable Consensus Algorithm
- [Raft Visualization](https://raft.github.io/) - Interactive Raft visualization
- [gRPC Documentation](https://grpc.io/docs/)
- [Protocol Buffers](https://developers.google.com/protocol-buffers)

---

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

### Development Setup

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Code Style

- Follow C++20 best practices
- Use meaningful variable names
- Add comments for complex logic
- Include tests for new features
- Run `clang-format` before committing

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- Diego Ongaro and John Ousterhout for the Raft algorithm
- The gRPC team for excellent RPC framework
- Google for Protocol Buffers and GoogleTest

---

## Authors

- **Nathan Ngaleu** - [GitHub](https://github.com/Notuc)

