# threadpool

A fast and lightweight C++ threadpool. Uses [threadpark](https://github.com/PrimeIntellect-ai/threadpark) for parking and unparking worker threads.
The user is responsible for ensuring that writes by the dispatching thread are visible to the worker thread, if such a state access occurs due to the wake up being fast enough for this to be an issue.

## Example usage

The following example showcases the threadpool API:

```cpp
#include <pithreadpool/threadpool.hpp>

#include <iostream>

int main() {
    const pi::threadpool::ThreadPool pool{/*num_threads=*/1, /*max_task_queue_size=*/64};
    pool.startup();
    std::vector<pi::threadpool::TaskFuture<pi::threadpool::void_t>> futures{};
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
```

It is also possible to return values from a task, as long as they are copyable:
```cpp
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
```

## Prerequisites

- Git
- CMake (3.22.1 or higher)
- C++ compiler with C++20 support (MSVC 17+, gcc 11+ or clang 12+)

## Supported architectures

threadpool aims to be compatible with all architectures.
Feel free to create issues for architecture-induced compilation failures.

## Building

Below are platform-specific instructions for installing prerequisites and then building threadpool. If your system is not covered or you wish to see how we do it on multiple platforms, refer to our CI configuration.

### Installing prerequisites

In this section we propose methods of installing the required prerequisites for building threadpark on Windows, macOS, Linux, FreeBSD, and OpenBSD.

#### Windows

With the winget package manager installed & up-to-date from the Microsoft Store, you can install the prerequisites as
follows:

```bash
winget install Microsoft.VisualStudio.2022.Community --silent --override "--wait --quiet --add ProductLang En-us --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
winget install Git.Git --source winget
winget install Kitware.CMake --source winget
```

After installing these packages, make sure to refresh your PATH by restarting your explorer.exe in the Task Manager and
opening a new Terminal, launched by said explorer.

#### macOS

```bash
xcode-select --install # if not already installed

# install Homebrew package manager if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

brew install git        # if not already installed by xcode command line tools
brew install cmake
```

#### Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential
sudo apt install -y git
sudo apt install -y cmake
```

#### FreeBSD

```bash
# Update pkg repository and install prerequisites
sudo pkg update -f
sudo pkg install -y cmake llvm16

# Ensure the newer clang is first in your PATH
export PATH="/usr/local/llvm16/bin:$PATH"
```

#### OpenBSD

```bash
# Set up appropriate PKG_PATH for your OpenBSD release (example shown for 7.6)
export PKG_PATH="https://cdn.openbsd.org/pub/OpenBSD/7.6/packages/amd64/"

# Install prerequisites (version numbers may differ)
pkg_add -I cmake-3.30.1v1 llvm-16.0.6p30

# Ensure the newer clang is first in your PATH
export PATH="/usr/local/llvm/bin:$PATH"
```

### Building the native library & other targets

To build all native targets, run the following commands valid for both Windows with PowerShell (replace backslashes if needed) and Unix-like systems (Linux, macOS, FreeBSD, OpenBSD), starting from the root directory of the repository:

```bash
git submodule update --init --recursive
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
```

If you have multiple C/C++ compilers installed (for instance, a newer LLVM) and wish to make sure CMake uses them, you can specify them explicitly:

```bash
cmake -DCMAKE_C_COMPILER=cc -DCMAKE_CXX_COMPILER=c++ -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
```

### Recommended way to use the threadpool library

The recommended way to use threadpool in a C/C++ project is to clone the repository and link against the `threadpool` library in
CMake:

```bash
git clone --recurse https://github.com/PrimeIntellect-ai/threadpool.git
```

Then add the newly cloned repository as a subdirectory in your CMakeLists file:

```cmake
add_subdirectory(threadpool)
```

Then link against the threadpool library

```cmake
target_link_libraries(YourTarget PRIVATE threadpool)
```

## Testing

### C++ Tests

To run the C++ unit tests, starting from the root directory of the repository, run the following commands valid for both
Windows with PowerShell and Unix-like systems:

```bash
cd build
ctest --verbose --build-config Release --output-on-failure
```

## License

This project is licensed under the MIT License.

## Contributing

Contributions are welcome! Please submit issues and pull requests to help improve threadpool.
