# Parallel BMP Image Convolution

This project implements a parallelized convolution filter for 24-bit uncompressed BMP images using C++ and OpenMP. It supports custom convolution kernels and benchmarks performance across different optimization levels and thread counts.

## Features

- Reads and writes 24-bit uncompressed BMP images
- Applies a user-defined convolution kernel to each color channel (R, G, B)
- Parallelized using OpenMP for multi-threaded performance
- Benchmarking script to test various optimization flags and thread counts
- Outputs performance statistics (average time, variance, standard deviation)

## File Structure

- `convolve.cpp` — Main C++ source code for convolution and BMP handling
- `convolve` — Compiled binary (Linux ELF)
- `run_benchmarks.sh` — Bash script to automate compilation, benchmarking, and result collection
- `input*.bmp` — Sample BMP images of various sizes for testing
- `benchmark_results/` — Output directory for logs, CSV results, plots, and summary
- `.vscode/` — VSCode configuration files

## Requirements

- C++ compiler with OpenMP support (e.g., `g++`)
- Bash shell (for running the benchmark script)
- Linux or WSL recommended for full compatibility

## Build

To build the project with optimization and AVX2 support:

```sh
g++ -O3 -march=native -mavx2 -fopenmp -o convolve convolve.cpp
```

Or use the provided VSCode task or the benchmark script.

## Usage

### Run convolution on an image

```sh
OMP_NUM_THREADS=4 ./convolve input.bmp output.bmp [kernel_rows kernel_cols kernel_values...]
```

- If no kernel is specified, a 3x3 identity kernel is used.
- Example with a 3x3 averaging kernel:

```sh
OMP_NUM_THREADS=4 ./convolve input50x50.bmp output.bmp 3 3 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11
```

### Run all benchmarks

```sh
bash run_benchmarks.sh
```

This will:
- Compile the code with `-O0`, `-O1`, `-O2`, `-O3`
- Run with 1, 2, 4, and 8 threads on all input images
- Collect logs and results in `benchmark_results/`

## Output

- Processed images are saved in `benchmark_results/output_images/`
- Performance logs and summary in `benchmark_results/`
- CSV file with average times: `benchmark_results/benchmark_results.csv`
- Full log: `benchmark_results/all_results.txt`

## Example Kernels

- **Identity:** `3 3 0 0 0 0 1 0 0 0 0`
- **Edge Detection:** `3 3 -1 -1 -1 -1 8 -1 -1 -1 -1`
- **Box Blur:** `3 3 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11`

## License

This project is provided for educational purposes.

---

*Author: Danilo Todorović*
