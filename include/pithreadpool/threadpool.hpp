#pragma once

#include <memory>
#include <functional>

namespace pi::threadpool {
    struct TaskFutureInternalState;

    struct TaskFuture {
        std::shared_ptr<TaskFutureInternalState> internal_state;

        TaskFuture();

        TaskFuture(TaskFuture &other) = delete;

        TaskFuture(TaskFuture &&other) noexcept;

        void join() const;
    };

    struct ThreadPoolInternalState;

    class ThreadPool {
        ThreadPoolInternalState *internal_state;

    public:
        explicit ThreadPool(int num_threads, int max_task_queue_size);

        ThreadPool(const ThreadPool &) = delete;

        ThreadPool &operator=(const ThreadPool &) = delete;

        ThreadPool(ThreadPool &&) = delete;

        ThreadPool &operator=(ThreadPool &&) = delete;

        void startup() const;

        void shutdown() const;

        /**
         * Schedules a task to be performed on the threadpool.
         * May not be called before @code startup@endcode has been called.
         * @param task the task to be scheduled
         * @return a future that completes when the task was successfully executed
         */
        [[nodiscard]] TaskFuture scheduleTask(const std::function<void()> &task) const;

        ~ThreadPool();
    };
}
