add_executable(kvstore_test kvstore/kvStore_test.cpp)
add_executable(log_test raft/test_log.cpp)
add_executable(command_test raft/test_command.cpp)
add_executable(raft_node_test raft/test_raft_node.cpp)
add_executable(raft_cluster_test raft/test_raft_cluster.cpp)
add_executable(main_run_all_test raft/test_main.cpp)
add_executable(kvstore_integration_test raft/test_kvstore_integration.cpp)
add_executable(snapshot_test raft/test_snapshot.cpp)
add_executable(automatic_snapshot_test raft/test_automatic_snapshot.cpp)


target_link_libraries(kvstore_test 
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)

target_link_libraries(log_test 
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)

target_link_libraries(command_test 
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)

target_link_libraries(raft_node_test 
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)
target_link_libraries(raft_cluster_test
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)


target_link_libraries(main_run_all_test 
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)
target_link_libraries(kvstore_integration_test
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)

target_link_libraries(snapshot_test 
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)

target_link_libraries(automatic_snapshot_test
  PRIVATE 
  gtest_main
  ShardKV::Headers
  ShardKV_Core
  gtest
)


enable_testing()
include(GoogleTest)
gtest_discover_tests(kvstore_test)
gtest_discover_tests(log_test)
gtest_discover_tests(command_test)
gtest_discover_tests(raft_node_test)
gtest_discover_tests(raft_cluster_test)
gtest_discover_tests(main_run_all_test)
gtest_discover_tests(kvstore_integration_test)
gtest_discover_tests(snapshot_test)
gtest_discover_tests(automatic_snapshot_test)
