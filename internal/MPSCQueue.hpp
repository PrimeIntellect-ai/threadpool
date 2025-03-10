#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>

template <typename T>
class MPSCQueue {
public:
    /// Create a queue with the given capacity.
    /// For correct usage, capacity >= 2 is recommended
    explicit MPSCQueue(const std::size_t capacity)
        : count_(0), head_(0), tail_(0), max_(capacity) {
        // Allocate the ring buffer. Each element is an atomic pointer to T.
        buffer_ = new std::atomic<T*>[max_];
        for (std::size_t i = 0; i < max_; i++) {
            buffer_[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~MPSCQueue() {
        delete[] buffer_;
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    [[nodiscard]] bool enqueue(T* obj, const bool seq_cst) {
        // Atomically increment count to reserve a slot
        const std::size_t count = count_.fetch_add(1, seq_cst ? std::memory_order_seq_cst : std::memory_order_acquire);
        if (count >= max_) {
            // The queue was full; roll back
            count_.fetch_sub(1, seq_cst ? std::memory_order_seq_cst : std::memory_order_release);
            return false;
        }

        // Get a unique index in the ring for this producer
        const std::size_t head = head_.fetch_add(1, seq_cst ? std::memory_order_seq_cst : std::memory_order_acquire);
        std::size_t idx = head % max_;

        // The original code asserts that the slot should be empty
        // (since we never overwrite a slot until it's consumed).
        assert(buffer_[idx].load(std::memory_order_relaxed) == nullptr);

        // Atomically set the pointer in the slot
        T* old = buffer_[idx].exchange(obj, seq_cst ? std::memory_order_seq_cst : std::memory_order_release);
        assert(old == nullptr);
        return true;
    }

    T* dequeue(const bool seq_cst) {
        // Atomically swap out the pointer at 'tail'
        T* ret = buffer_[tail_].exchange(nullptr, seq_cst ? std::memory_order_seq_cst : std::memory_order_acquire);
        if (!ret) {
            // The queue is empty (or the producer hasn't finished writing).
            return nullptr;
        }

        // Move consumer index forward (wrap around if needed)
        if (++tail_ >= max_) {
            tail_ = 0;
        }

        // Decrement the overall count of items in the queue
        const std::size_t r = count_.fetch_sub(1, std::memory_order_release);
        assert(r > 0);
        return ret;
    }

    [[nodiscard]] std::size_t size() const {
        // Relaxed is enough here because we just want an approximate count
        return count_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t capacity() const {
        return max_;
    }

private:
    /// The total number of items currently in the queue
    std::atomic<std::size_t> count_;

    /// The producer head index
    std::atomic<std::size_t> head_;

    /// The consumer tail index (single consumer, so it can be non-atomic)
    std::size_t tail_;

    /// Maximum number of items (capacity of buffer_)
    std::size_t max_;

    /// Dynamically allocated ring buffer of atomic pointers
    std::atomic<T*>* buffer_;
};