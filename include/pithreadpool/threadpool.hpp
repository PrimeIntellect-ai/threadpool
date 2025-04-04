#pragma once

#include <memory>
#include <optional>
#include <functional>
#include <stdexcept>

namespace pi::threadpool {
    typedef uint32_t void_t;

    using OpaquePtr = std::unique_ptr<void, void(*)(void *)>;

    struct ResultWrapper {
        std::optional<OpaquePtr> result = std::nullopt;

        template<typename T>
        static ResultWrapper FromResult(const T &result) {
            return ResultWrapper{
                OpaquePtr{
                    new T(result),
                    [](void *ptr) { delete static_cast<T *>(ptr); }
                }
            };
        }

        template<typename T>
        T &get() const {
            if (!result.has_value()) {
                throw std::logic_error("Result has no result");
            }
            return *static_cast<T *>(result->get());
        }
    };

    struct TaskFutureInternalState;
    struct ThreadPoolInternalState;

    namespace internal {
        const ResultWrapper &GetResultWrapper(const std::shared_ptr<TaskFutureInternalState> &future_state);

        template<typename T>
        T &GetResult(const std::shared_ptr<TaskFutureInternalState> &internal_state) {
            const auto &result_wrapper = GetResultWrapper(internal_state);
            return result_wrapper.get<T>();
        }

        void JoinFuture(const std::shared_ptr<TaskFutureInternalState> &internal_state);

        std::shared_ptr<TaskFutureInternalState> MakeFutureInternalState();
    };

    template<typename T>
    struct TaskFuture {
        std::shared_ptr<TaskFutureInternalState> internal_state;

        TaskFuture() : internal_state(internal::MakeFutureInternalState()) {
        }

        TaskFuture(TaskFuture &other) = delete;

        TaskFuture(TaskFuture &&other) noexcept : internal_state(std::move(other.internal_state)) {
            other.internal_state = nullptr;
        }

        TaskFuture& operator=(TaskFuture &other) = delete;
        TaskFuture& operator=(TaskFuture &&other) = default;

        void join() const {
            internal::JoinFuture(internal_state);
        }

        T &get() const {
            join();
            return internal::GetResult<T>(internal_state);
        }
    };

    void ScheduleTaskOnFreeThread(const ThreadPoolInternalState *pool_state,
                                  const std::shared_ptr<TaskFutureInternalState> &future_state,
                                  const std::function<ResultWrapper()> &task);

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

        [[nodiscard]] bool isPoolRunning() const;

        /**
         * Schedules a task to be performed on the threadpool.
         * May not be called before @code startup@endcode has been called.
         * @param task the task to be scheduled that returns a value
         * @return a future that completes when the task was successfully executed
         */
        template<typename T>
        [[nodiscard]] TaskFuture<T> scheduleTaskWithResult(const std::function<T()> &task) const {
            if (!isPoolRunning()) {
                throw std::runtime_error("pi::threadpool::ThreadPool::scheduleTask called before startup");
            }
            TaskFuture<T> future{};
            ScheduleTaskOnFreeThread(internal_state, future.internal_state, [task]() -> ResultWrapper {
                return ResultWrapper::FromResult(task());
            });
            return future;
        }

        [[nodiscard]] TaskFuture<void_t> scheduleTask(const std::function<void()> &task) const;

        ~ThreadPool();
    };
}
