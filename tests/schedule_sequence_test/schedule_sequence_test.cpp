#include <iostream>
#include <atomic>
#include <pithreadpool/threadpool.hpp>

int main() {
    const pi::threadpool::ThreadPool pool{2, 64};
    pool.startup();
    std::atomic_int counter{0};
    const pi::threadpool::MultiTaskResult<int> result = pool.scheduleSequence<int>(0u, 10u, [&counter](const size_t i) {
        std::cout << "Hello World: " << i << std::endl;
        ++counter;
        return static_cast<int>(i);
    });
    result.join();
    if (counter != 10) {
        std::cerr << "Counter mismatch: expected 10, got " << counter << std::endl;
        std::abort();
    }
    for (size_t i = 0; i < result.size(); ++i) {
        if (result.get(i) != static_cast<int>(i)) {
            std::cerr << "Mismatch" << std::endl;
            std::abort();
        }
    }

    pool.shutdown();
    return 0;
}
