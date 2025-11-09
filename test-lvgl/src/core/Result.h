#ifndef RESULT_H
#define RESULT_H

#include <cassert>
#include <utility>

template <typename successT, typename failureT>
class Result {
public:
    // Default constructor: creates an error state with a default-constructed failureT
    Result() : has_value_(false), error_(failureT()) {}

    // Constructor for success state with a provided value
    Result(successT value) : has_value_(true), value_(std::move(value)) {}

    // Constructor for error state with a provided error
    Result(failureT error) : has_value_(false), error_(std::move(error)) {}

    // Static factory method to create a success result with default-constructed value
    static Result<successT, failureT> okay() { return Result<successT, failureT>(successT()); }

    // Static factory method to create a success result with a specific value
    static Result<successT, failureT> okay(successT value)
    {
        return Result<successT, failureT>(std::move(value));
    }

    // Static factory method to create an error result with a specific error
    static Result<successT, failureT> error(failureT error)
    {
        return Result<successT, failureT>(std::move(error));
    }

    // Returns true if the result contains a value (success)
    bool isValue() const { return has_value_; }

    // Returns true if the result contains an error (failure)
    bool isError() const { return !has_value_; }

    // Access the success value (asserts if not in success state)
    successT value() const
    {
        assert(has_value_);
        return value_;
    }

    // Access the error value (asserts if not in error state)
    failureT error() const
    {
        assert(!has_value_);
        return error_;
    }

private:
    bool has_value_;
    successT value_;
    failureT error_;
};

#endif // RESULT_H