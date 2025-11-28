#include <gtest/gtest.h>
#include <optional>
#include "kvstore/KVStore.hpp" 

TEST(KVStoreTest_GET, Get_ReturnsValue_WhenKeyExists) {
    KVStore store;
    const std::string test_key = "Alice";
    const std::string test_value = "5"; 

    store.Put(test_key, test_value);

    std::optional<std::string> result = store.Get(test_key);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result.value(), test_value);
}

TEST(KVStoreTest_GET, Get_ReturnsNullOpt_WhenKeyMissing) {
    KVStore store; // empty store

    std::optional<std::string> result = store.Get("Bob");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result, std::nullopt);
}



TEST(KVStoreTEST_APPEND, Append_Concatenates_Value) {
    KVStore store;
    const std::string key = "LogKey";

    store.Put(key, "Hello"); 

    store.Append(key, " World!"); 

    std::optional<std::string> result = store.Get(key);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Hello World!");
}

TEST(KVStoreTEST_APPEND, Append_Initializes_NonExisting_Value) {
    KVStore store;
    const std::string key = "Bob";
    const std::string initial_value = "Entry1";

    store.Append(key, initial_value);

    std::optional<std::string> result = store.Get(key);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), initial_value);
}
