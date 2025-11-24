#ifndef RESULT_H
#define RESULT_H

#include <expected>
#include <utility>

/**
 * Result<T, E>: Thin wrapper around C++23 std::expected.
 *
 * Provides compatibility with existing codebase while using the standard
 * library's optimized implementation. std::expected properly handles
 * uninitialized member warnings that our custom Result class triggered.
 *
 * Uses composition instead of inheritance to avoid name collisions between
 * static factory methods and instance accessors.
 */
template <typename successT, typename failureT>
class Result {
private:
    std::expected<successT, failureT> inner_;

public:
    // Default constructor: creates an error state with default-constructed failureT.
    Result() : inner_(std::unexpected(failureT())) {}

    // Constructors that forward to std::expected.
    Result(successT value) : inner_(std::move(value)) {}
    Result(failureT err) : inner_(std::unexpected(std::move(err))) {}
    Result(std::unexpected<failureT> err) : inner_(std::move(err)) {}
    Result(std::expected<successT, failureT> exp) : inner_(std::move(exp)) {}

    // Static factory method to create a success result with default-constructed value.
    static Result<successT, failureT> okay() { return Result<successT, failureT>(successT()); }

    // Static factory method to create a success result with a specific value.
    // Suppress false positive -Wmaybe-uninitialized with GCC + -O3 + variants.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    static Result<successT, failureT> okay(successT value)
    {
        return Result<successT, failureT>(std::move(value));
    }
#pragma GCC diagnostic pop

    // Static factory method to create an error result with default-constructed error.
    static Result<successT, failureT> error()
    {
        return Result<successT, failureT>(std::unexpected(failureT()));
    }

    // Static factory method to create an error result with a specific error.
    static Result<successT, failureT> error(failureT err)
    {
        return Result<successT, failureT>(std::unexpected(std::move(err)));
    }

    // Compatibility accessors.
    bool isValue() const { return inner_.has_value(); }
    bool isError() const { return !inner_.has_value(); }

    // Value accessor (forward to inner_).
    successT value() const& { return inner_.value(); }
    successT value() && { return std::move(inner_).value(); }

    // Error accessor - renamed to errorValue() to avoid collision with static error() factory.
    const failureT& errorValue() const& { return inner_.error(); }
    failureT& errorValue() & { return inner_.error(); }
    failureT errorValue() && { return std::move(inner_).error(); }
};

#endif // RESULT_H