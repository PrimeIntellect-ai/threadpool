#include <iostream>
#include <pithreadpool/threadpool.hpp>

int main() {
    const pi::threadpool::ThreadPool pool{1, 64};
    pool.startup();
    std::vector<pi::threadpool::TaskFuture<int>> futures{};
    for (int i = 0; i < 10; i++) {
        auto future = pool.scheduleTaskWithResult<int>([i] {
            std::cout << "Hello World: " << i << std::endl;
            return i;
        });
        futures.emplace_back(std::move(future));
    }
    int i = 0;
    for (const auto &f : futures) {
        if (f.get() != i) {
            std::cerr << "Mismatch" << std::endl;
            std::abort();
        }
        i++;
    }
    pool.shutdown();
    return 0;
}