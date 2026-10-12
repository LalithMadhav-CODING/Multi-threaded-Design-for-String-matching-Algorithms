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

## Datasets

Due to GitHub's file size limits, the large genome dataset files required for extensive testing are not included in this repository. 

To run tests on massive text files like genomes, please download them and place them in the `datasets/` directory:
* **Human Genome**: You can download the human genome sequence (e.g., GRCh38) from the [NCBI Datasets portal](https://www.ncbi.nlm.nih.gov/datasets/genome/GCF_000001405.40/).
* **E. Coli Genome**: You can download the E. Coli genome sequence from the [NCBI Datasets portal](https://www.ncbi.nlm.nih.gov/datasets/genome/GCF_000005845.2/).
* **Wheat Genome** (~14.6 GB, used for the largest benchmark inputs): *Triticum aestivum* IWGSC CS RefSeq v2.1 from the [NCBI Datasets portal](https://www.ncbi.nlm.nih.gov/datasets/genome/GCF_018294505.1/). Save it as `datasets/wheat_genome.fna`.

*(Note: Be sure to extract any `.gz` files into raw `.fna` text format before running the search!)*

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

## Benchmarking and Analysis

The parallelism analysis (small vs large inputs, thread overhead, crossover point) is written up in [`docs/report.md`](docs/report.md). To reproduce it:

1. Build in Release mode (the default) as above. This also produces `bm_bench`.
2. Run the sweep for each dataset; results are appended to one CSV:
   ```bash
   ./bm_bench ../datasets/ecoli_genome.fna ../results/bm_results.csv --name ecoli
   ./bm_bench ../datasets/human_genome.fna ../results/bm_results.csv --name human
   ./bm_bench ../datasets/wheat_genome.fna ../results/bm_results.csv --name wheat
   ```
   Add `--quick` for a short sweep (inputs up to 10 MB, fewer thread counts) to check that everything works.
   The follow-up experiments in report section 7 (core types, chunks per thread, memory bandwidth, clock speed) run from the repository root with `scripts/run_experiments.sh build`. Its CPU numbering is specific to the Intel Core Ultra 9 285.
3. Generate the plots and the summary tables:
   ```bash
   python3 -m venv .venv && .venv/bin/pip install matplotlib pandas
   .venv/bin/python scripts/plot_results.py results/bm_results.csv results/plots
   ```

`bm_bench` keeps only the nucleotide sequence of the FASTA file (headers, line breaks and `N` bases removed, lowercase bases upper-cased) and checks every parallel result against the sequential one. The full human + wheat sweep needs about 30 GB of RAM.

## Running Tests

To verify the core correctness of the Boyer-Moore implementation (without threading), run the test executable:

```bash
./run_tests
```
