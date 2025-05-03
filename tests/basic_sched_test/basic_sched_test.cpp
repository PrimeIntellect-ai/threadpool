#include <iostream>
#include <pithreadpool/threadpool.hpp>

int main() {
    const pi::threadpool::ThreadPool pool{1, 64};
    pool.startup();
    std::vector<pi::threadpool::TaskFuture<void>> futures{};
    for (int i = 0; i < 10; i++) {
        auto future = pool.scheduleTask([i] {
            std::cout << "Hello World: " << i << std::endl;
        });
        futures.emplace_back(std::move(future));
    }
    for (const auto &f : futures) {
        f.join();
    }
    pool.shutdown();
    return 0;
}