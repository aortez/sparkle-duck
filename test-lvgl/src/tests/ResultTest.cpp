#include "core/Result.h"
#include <cassert>
#include <gtest/gtest.h>
#include <iostream>
#include <spdlog/spdlog.h>

TEST(ResultTest, DefaultConstructorCreatesErrorState)
{
    spdlog::info("Starting ResultTest::DefaultConstructorCreatesErrorState test");
    Result<int, std::string> result;
    EXPECT_FALSE(result.isValue());
    EXPECT_TRUE(result.isError());
    // Note: Result uses assert(), so calling value() on error state would terminate program.
    // We'll just test that we can access the error.
    std::string error = result.errorValue();
    EXPECT_EQ(error, std::string()); // Default-constructed error.
}

TEST(ResultTest, SuccessWithDefaultValue)
{
    spdlog::info("Starting ResultTest::SuccessWithDefaultValue test");
    Result<int, std::string> result = Result<int, std::string>::okay();
    EXPECT_TRUE(result.isValue());
    EXPECT_FALSE(result.isError());
    int value = result.value();
    EXPECT_EQ(value, 0); // Default-constructed int.
    // Note: Result uses assert(), so calling error() on success state would terminate program.
    // We'll just test that we can access the value.
}

TEST(ResultTest, SuccessWithSpecificValue)
{
    spdlog::info("Starting ResultTest::SuccessWithSpecificValue test");
    Result<int, std::string> result = Result<int, std::string>::okay(42);
    EXPECT_TRUE(result.isValue());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorWithDefaultValue)
{
    spdlog::info("Starting ResultTest::ErrorWithDefaultValue test");
    Result<int, std::string> result; // Default constructor creates error state.
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorValue(), std::string());
}

TEST(ResultTest, ErrorWithSpecificValue)
{
    spdlog::info("Starting ResultTest::ErrorWithSpecificValue test");
    Result<int, std::string> result = Result<int, std::string>::error("Test error");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorValue(), "Test error");
}