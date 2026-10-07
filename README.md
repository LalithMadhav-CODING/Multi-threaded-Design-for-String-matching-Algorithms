# Multi-Threaded Boyer-Moore String Search

This project implements a highly efficient, multi-threaded version of the Boyer-Moore string search algorithm in C++. By leveraging a custom thread pool and dividing the text into overlapping chunks, it achieves significant performance improvements over sequential search on large datasets.

This project was built as an Operating Systems (OS) project, demonstrating the practical application of multithreading, concurrency, and synchronization mechanisms in C++.

## Features

- **Boyer-Moore Algorithm**: Implements both the Bad Character and Good Suffix heuristics for optimal search performance.
- **Multithreading**: Uses a custom `ThreadPool` to distribute the search workload across multiple CPU cores.
- **Overlap Handling**: Automatically handles overlapping chunks across threads to ensure matches at chunk boundaries are accurately identified.
- **Cross-Platform Building**: Utilizes CMake for easy configuration and building.

## Project Structure

```
├── CMakeLists.txt        # Build configuration for CMake
├── include/              # Header files
│   ├── boyer_moore.h     # Boyer-Moore algorithm definitions
│   ├── thread_pool.h     # Thread pool implementation
│   └── utils.h           # Utility functions (e.g., file reading)
├── src/                  # Source files
│   ├── boyer_moore.cpp
│   ├── main.cpp          # Main executable logic
│   ├── thread_pool.cpp
│   └── utils.cpp
└── tests/                # Test files
    └── test_boyer_moore.cpp # Sequential tests for algorithmic correctness
```

## Prerequisites

- **C++17 Compiler**: GCC, Clang, or MSVC.
- **CMake**: Version 3.10 or higher.

## Building the Project

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Generate the build files using CMake:
   ```bash
   cmake ..
   ```

3. Compile the project:
   ```bash
   cmake --build .
   ```

This will produce two executables in the build directory:
- `bm_search`: The main multi-threaded search program.
- `run_tests`: The sequential testing suite.

## Usage

Run the main executable (`bm_search`) from the build directory.

```bash
./bm_search <filepath> <pattern> [num_threads]
```
*(Note: on Windows, the executable may be named `bm_search.exe` and located inside a `Debug` or `Release` folder depending on your generator).*

### Arguments

- `<filepath>`: Path to the text file you want to search.
- `<pattern>`: The string pattern to search for.
- `[num_threads]` (Optional): The number of threads to use. Defaults to the hardware concurrency of your machine (i.e. number of logical cores).

### Example

```bash
./bm_search ../datasets/sample_text.txt "target_string" 4
```

## How It Works

1. **Preprocessing**: The main thread reads the file and computes the Bad Character and Good Suffix tables for the given pattern.
2. **Chunking**: The text is divided into chunks based on the number of threads. Each chunk's end index is extended by `M - 1` (where `M` is the length of the pattern) to ensure patterns crossing chunk boundaries are found.
3. **Parallel Search**: Tasks are enqueued into the thread pool. Each thread searches its designated chunk using the precomputed tables.
4. **Aggregation**: Once all threads finish, the main thread collects all occurrences, sorts them, and removes any duplicates caused by the overlapping chunks.

## Running Tests

To verify the core correctness of the Boyer-Moore implementation (without threading), run the test executable:

```bash
./run_tests
```
