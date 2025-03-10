#include "pithreadpool/threadpool.hpp"

#include <future>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include <unordered_map>

#include <MPSCQueue.hpp>
#include <threadpark.h>


namespace pi::threadpool {
    struct TaskFutureInternalState {
        tpark_handle_t *park_handle;
        std::atomic<bool> done{};

        TaskFutureInternalState() {
            park_handle = tparkCreateHandle();
        }

        ~TaskFutureInternalState() {
            tparkDestroyHandle(park_handle);
        }
    };

    TaskFuture::TaskFuture() : internal_state(std::make_unique<TaskFutureInternalState>()) {
    }

    TaskFuture::TaskFuture(TaskFuture &&other) noexcept : internal_state(std::move(other.internal_state)) {
        other.internal_state = nullptr;
    }

    void TaskFuture::join() const {
        tparkBeginPark(internal_state->park_handle);
        if (internal_state->done.load(std::memory_order_acquire)) {
            tparkEndPark(internal_state->park_handle);
            return;
        }
        tparkWait(internal_state->park_handle, true);
    }

    struct TaskQueueItem {
        std::function<void()> task;
        std::shared_ptr<TaskFutureInternalState> future_state;

        explicit TaskQueueItem(std::function<void()> task,
                               const std::shared_ptr<TaskFutureInternalState> &future_state): task(std::move(task)),
                                                                       future_state(future_state) {
        }
    };

    struct WorkerState {
        MPSCQueue<TaskQueueItem> task_queue;
        tpark_handle_t *park_handle;

        explicit WorkerState(const int max_queue_capacity): task_queue(max_queue_capacity) {
            park_handle = tparkCreateHandle();
        }

        ~WorkerState() {
            tparkDestroyHandle(park_handle);
        }
    };

    struct ThreadPoolInternalState {
        std::atomic<bool> running{false};
        int num_threads{};
        int max_task_capacity{};

        std::vector<std::thread> threads{};
        std::unordered_map<std::thread::id, WorkerState> worker_states{};
    };
}

static void RunTaskQueueItem(const pi::threadpool::TaskQueueItem *entry) {
    entry->task();
    entry->future_state->done.store(true, std::memory_order_release);
    tparkWake(entry->future_state->park_handle);
}

static void WorkerThread(std::future<std::reference_wrapper<pi::threadpool::WorkerState>> worker_state_future,
                         const pi::threadpool::ThreadPoolInternalState *internal_state) {
    pi::threadpool::WorkerState &worker_state = worker_state_future.get();
    while (internal_state->running.load(std::memory_order_acquire)) {
        const pi::threadpool::TaskQueueItem *entry{}; {
            tparkBeginPark(worker_state.park_handle);
            entry = worker_state.task_queue.dequeue(false);
            if (entry == nullptr) {
                tparkWait(worker_state.park_handle, true);
                do {
                    entry = worker_state.task_queue.dequeue(false);
                    // despite the fact that wakes are guaranteed not-spurious and insertion into the queue
                    // should happen before the wake, it still is possible that the queue is empty because
                    // seq cst only guarantees all writes before a seq_cst store are visible to other threads
                    // when we are seq_cst loading a value that was written with seq_cst by the producer thread
                    // from which it can be inferred that a previous store has occurred.
                    // Here, the producer calls wake, where we are at the mercy of the OS as to which
                    // memory model it uses for the atomic store. So we still can't assert the queue
                    // is always non-empty after a wake.
                } while (entry == nullptr && internal_state->running.load(std::memory_order_acquire));
            } else {
                tparkEndPark(worker_state.park_handle);
            }
        }
        if (entry == nullptr) {
            break; // this woke up the thread with no task to run, which only happens on shutdown
        }
        RunTaskQueueItem(entry);
        delete entry;
    }
}

static std::thread::id CreateWorkerThread(pi::threadpool::ThreadPoolInternalState *internal_state) {
    std::promise<std::reference_wrapper<pi::threadpool::WorkerState>> taskqueue_promise{};
    std::future<std::reference_wrapper<pi::threadpool::WorkerState>> taskqueue_future =
            taskqueue_promise.get_future();

    const auto &worker_thread = internal_state->threads.emplace_back(WorkerThread, std::move(taskqueue_future),
                                                                     internal_state);
    const std::thread::id worker_thread_id = worker_thread.get_id();

    const auto [task_queue_it, _] = internal_state->worker_states.emplace(
        worker_thread_id, internal_state->max_task_capacity);
    auto &task_queue = task_queue_it->second;
    taskqueue_promise.set_value(task_queue);
    return worker_thread_id;
}

static std::thread::id GetSchedDstThread(const pi::threadpool::ThreadPoolInternalState &state) {
    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, state.threads.size() - 1);
    return state.threads.at(dist(rng)).get_id();
}

static void ScheduleTaskOnFreeThread(pi::threadpool::ThreadPoolInternalState &state,
                                     const pi::threadpool::TaskQueueItem &item) {
    const std::thread::id thread_id = GetSchedDstThread(state);
    auto *enqueued_item = new pi::threadpool::TaskQueueItem(item.task, item.future_state);
    pi::threadpool::WorkerState &worker_state = state.worker_states.at(thread_id);
    if (!worker_state.task_queue.enqueue(enqueued_item, false)) {
        throw std::runtime_error(
            "pi::threadpool::Threadpool task queue does not have enough capacity to enqueue the task. Please increase max_task_queue_size."
        );
    }

    // wake worker thread
    tparkWake(worker_state.park_handle);
}

pi::threadpool::ThreadPool::ThreadPool(const int num_threads, const int max_task_queue_size) : internal_state(
    new ThreadPoolInternalState{
        .num_threads = num_threads,
        .max_task_capacity = max_task_queue_size,
    }) {
}

void pi::threadpool::ThreadPool::startup() const {
    // create and start worker threads
    for (int i = 0; i < internal_state->num_threads; ++i) {
        CreateWorkerThread(internal_state);
    }
    internal_state->running.store(true, std::memory_order_release);
}

void pi::threadpool::ThreadPool::shutdown() const {
    if (!internal_state->running.load(std::memory_order_acquire)) {
        throw std::runtime_error("pi::threadpool::ThreadPool::shutdown called before startup");
    }

    if (internal_state->running.exchange(false, std::memory_order_release)) {
        for (auto &worker_state: internal_state->worker_states) {
            tparkWake(worker_state.second.park_handle);
        }
        for (auto &thread: internal_state->threads) {
            thread.join();
        }
    }
}

pi::threadpool::TaskFuture pi::threadpool::ThreadPool::scheduleTask(const std::function<void()> &task) const {
    if (!internal_state->running.load(std::memory_order_acquire)) {
        throw std::runtime_error("pi::threadpool::ThreadPool::scheduleTask called before startup");
    }
    TaskFuture future{};
    const TaskQueueItem item{task, future.internal_state};
    ScheduleTaskOnFreeThread(*internal_state, item);
    return future;
}

pi::threadpool::ThreadPool::~ThreadPool() {
    if (internal_state->running.load(std::memory_order_acquire)) {
        shutdown();
    }
    delete internal_state;
}
