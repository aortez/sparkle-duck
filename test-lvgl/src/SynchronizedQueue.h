#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

/**
 * @brief Thread-safe queue for event processing.
 *
 * This queue supports multiple producers and a single consumer.
 * It provides both blocking and non-blocking operations.
 */
template <typename T>
class SynchronizedQueue {
public:
    SynchronizedQueue() = default;

    /**
     * @brief Push an item onto the queue.
     * @param item The item to push.
     */
    void push(T item)
    {
        {
            std::unique_lock lock(mutex_);
            queue_.push_back(std::move(item));
        }
        cv_.notify_one();
    }

    /**
     * @brief Try to pop an item from the queue (non-blocking).
     * @return The item if available, std::nullopt otherwise.
     */
    std::optional<T> tryPop()
    {
        std::unique_lock lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    /**
     * @brief Pop an item from the queue (blocking).
     * @return The next item from the queue.
     */
    T pop()
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shouldStop_; });

        if (shouldStop_ && queue_.empty()) {
            throw std::runtime_error("Queue stopped");
        }

        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    /**
     * @brief Get the current queue size.
     * @return Number of items in the queue.
     */
    size_t size() const
    {
        std::unique_lock lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief Check if the queue is empty.
     * @return true if empty, false otherwise.
     */
    bool empty() const
    {
        std::unique_lock lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief Clear all items from the queue.
     */
    void clear()
    {
        std::unique_lock lock(mutex_);
        queue_.clear();
    }

    /**
     * @brief Stop the queue (unblocks waiting threads).
     */
    void stop()
    {
        {
            std::unique_lock lock(mutex_);
            shouldStop_ = true;
        }
        cv_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
    bool shouldStop_ = false;
};