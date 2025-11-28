cmake_minimum_required(VERSION 3.20)
project(ShardKV CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

#  gRPC build options to prevent dependency errors - There are some compatibility issue with gRPC version and cmake
set(gRPC_BUILD_TESTS OFF CACHE BOOL "Disable gRPC tests")
set(gRPC_BUILD_GRPC_CPP_PLUGIN_CSHARP OFF CACHE BOOL "Disable CSharp plugin")
set(gRPC_INSTALL OFF CACHE BOOL "Do not try to install gRPC's targets")
set(gRPC_ABSL_PROVIDER "package" CACHE STRING "Use Abseil from the package")
set(gRPC_PROTOBUF_PROVIDER "package" CACHE STRING "Use Protobuf from the package")

FetchContent_Declare(
    gRPC
    GIT_REPOSITORY https://github.com/grpc/grpc
    GIT_TAG        v1.76.0  
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(gRPC)
find_package(Protobuf REQUIRED)

// Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.0 
)

set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(googletest)

# ShardKV_Headers INTERFACE Library, a library that holds all the of files in include/ 
add_library(ShardKV_Headers INTERFACE)
target_include_directories(ShardKV_Headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
add_library(ShardKV::Headers ALIAS ShardKV_Headers)


# ShardKV_Generated INTERFACE Library, a library that holds all the of files in proto/ and the dependencies
add_library(ShardKV_Generated INTERFACE)
target_include_directories(ShardKV_Generated INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${PROTO_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/proto>
)

target_link_libraries(ShardKV_Generated INTERFACE
    # Link to the necessary gRPC and Protobuf targets
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
)

//May not be necessary
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
file(GLOB PROTO_FILES "${PROTO_DIR}/*.proto") # Finds raft.proto

# Protobuf_generate function comes from find_package(Protobuf)
protobuf_generate(
    TARGET ShardKV_Generated
    LANGUAGE cpp
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${PROTO_DIR}
    # This automatically adds the generated files to the 'ShardKV_Generated' target
)

get_target_property(GRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin LOCATION)

protobuf_generate(
    TARGET ShardKV_Generated
    LANGUAGE grpc
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${PROTO_DIR}
    # Tell protoc to use the gRPC C++ plugin
    PLUGIN "protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
    # Specify the extensions for the gRPC generated files
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
)

add_subdirectory(src)
add_subdirectory(tests)
