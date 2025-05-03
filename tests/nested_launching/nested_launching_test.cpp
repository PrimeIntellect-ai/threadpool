#include <iostream>
#include <pithreadpool/threadpool.hpp>

int main() {
    const pi::threadpool::ThreadPool pool{2, 64};
    pool.startup();
    std::vector<pi::threadpool::TaskFuture<void>> futures{};
    for (int i = 0; i < 10; i++) {
        auto future = pool.scheduleTask([i, &pool] {
            std::vector<pi::threadpool::TaskFuture<void>> inner_futures{};
            for (int j = 0; j < 10; j++) {
                try {
                    auto inner_future = pool.scheduleTask([i, j] {
                        std::cout << "Hello World: " << i << ", " << j << std::endl;
                    });
                    inner_futures.emplace_back(std::move(inner_future));
                } catch (const std::runtime_error &) {
                    continue;
                }
                std::cerr << "Did not throw exception when scheduling task from inside thread pool" << std::endl;
                std::abort();
            }
        });
        future.join();
    }
    pool.shutdown();
    return 0;
}
