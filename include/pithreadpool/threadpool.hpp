#pragma once

#include <memory>
#include <optional>
#include <functional>
#include <stdexcept>

namespace pi::threadpool {
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
    class TaskFuture {
    public:
        std::shared_ptr<TaskFutureInternalState> internal_state;

        TaskFuture() : internal_state(internal::MakeFutureInternalState()) {
        }

        TaskFuture(TaskFuture &other) = delete;

        TaskFuture(TaskFuture &&other) noexcept : internal_state(std::move(other.internal_state)) {
            other.internal_state = nullptr;
        }

        TaskFuture &operator=(TaskFuture &other) = delete;

        TaskFuture &operator=(TaskFuture &&other) = default;

        void join() const {
            internal::JoinFuture(internal_state);
        }

        T &get() const {
            join();
            return internal::GetResult<T>(internal_state);
        }
    };

    template<>
    class TaskFuture<void> {
    public:
        std::shared_ptr<TaskFutureInternalState> internal_state;

        TaskFuture() : internal_state(internal::MakeFutureInternalState()) {
        }

        TaskFuture(TaskFuture &) = delete;

        TaskFuture(TaskFuture &&other) noexcept : internal_state(std::move(other.internal_state)) {
            other.internal_state = nullptr;
        }

        TaskFuture &operator=(TaskFuture &) = delete;

        TaskFuture &operator=(TaskFuture &&) = default;

        void join() const {
            internal::JoinFuture(internal_state);
        }

        void get() const {
            join();
        }
    };

    template<typename T>
    class MultiTaskResult {
        std::vector<TaskFuture<T>> futures;

    public:
        explicit MultiTaskResult(std::vector<TaskFuture<T>> &futures) : futures(std::move(futures)) {
        }

        MultiTaskResult() = default;

        MultiTaskResult(MultiTaskResult &&other) noexcept : futures(std::move(other.futures)) {
            other.futures.clear();
        }

        MultiTaskResult &operator=(MultiTaskResult &&other) noexcept {
            if (this != &other) {
                futures = std::move(other.futures);
                other.futures.clear();
            }
            return *this;
        }

        MultiTaskResult(const MultiTaskResult &) = delete;

        void join() const {
            for (const auto &future : futures) {
                future.join();
            }
        }

        T &get(size_t idx) const {
            if (idx >= futures.size()) {
                throw std::out_of_range("Index out of range");
            }
            return futures[idx].get();
        }

        [[nodiscard]] size_t size() const {
            return futures.size();
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
         * @param func the task to be scheduled that returns a value
         * @return a future that completes when the task was successfully executed
         */
        template<typename F>
        [[nodiscard]] auto scheduleTask(F &&func) const {
            using R = std::invoke_result_t<F>;
            TaskFuture<R> future;
            ScheduleTaskOnFreeThread(
                internal_state,
                future.internal_state,
                [fn = std::forward<F>(func)]() -> ResultWrapper {
                    if constexpr (std::is_void_v<R>) {
                        fn();
                        return ResultWrapper{};
                    } else {
                        return ResultWrapper::FromResult(fn());
                    }
                }
            );

            return future;
        }

        /**
         * Schedules a task to be performed on the threadpool.
         * May not be called before @code startup@endcode has been called.
         * @param func the task to be scheduled that returns a value
         * @return a future that completes when the task was successfully executed
         */
        template<typename T>
        [[nodiscard]] TaskFuture<T> scheduleTask(const std::function<T()> func) const {
            if (!isPoolRunning()) {
                throw std::runtime_error("pi::threadpool::ThreadPool::scheduleTask called before startup");
            }
            TaskFuture<T> future{};
            ScheduleTaskOnFreeThread(internal_state, future.internal_state, [func]() -> ResultWrapper {
                return ResultWrapper::FromResult(func());
            });
            return future;
        }

        /**
         * Schedules a sequence of tasks to be performed on the threadpool.
         */
        template<typename T>
        [[nodiscard]] MultiTaskResult<T> scheduleSequence(const size_t start, const size_t end,
                                                          const std::function<T(size_t)> func) const {
            if (!isPoolRunning()) {
                throw std::runtime_error("pi::threadpool::ThreadPool::scheduleTask called before startup");
            }
            std::vector<TaskFuture<T>> futures{};
            for (size_t i = start; i < end; ++i) {
                futures.emplace_back(scheduleTask([i, func] { return func(i); }));
            }
            return MultiTaskResult<T>(futures);
        }


        ~ThreadPool();
    };
}
