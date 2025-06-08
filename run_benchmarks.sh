#!/bin/bash

COMPILER="g++"
SOURCE="convolve.cpp"
BINARY="convolve"
INPUT_FILES=("input10x10.bmp" "input50x50.bmp" "input500x500.bmp" "input1000x1000.bmp" "input2000x2000.bmp")
OPT_LEVELS=("O0" "O1" "O2" "O3")
THREAD_COUNTS=(1 2 4 8)
KERNEL_ARGS=(3 3 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11 0.11)
OUTPUT_DIR="benchmark_results"
LOG_DIR="${OUTPUT_DIR}/logs"

# Creating directories
mkdir -p "${LOG_DIR}"
mkdir -p "${OUTPUT_DIR}/output_images"

# Create CSV header
CSV_FILE="${OUTPUT_DIR}/benchmark_results.csv"
echo "Optimization,Threads,Input,AverageTime(us)" > "${CSV_FILE}"

# Compiling and testing for each optimization level
for opt in "${OPT_LEVELS[@]}"; do
    echo "Compiling with -${opt}..."
    ${COMPILER} -std=c++11 -fopenmp -march=native -mavx2 -${opt} -o ${BINARY} ${SOURCE}
    
    if [ $? -ne 0 ]; then
        echo "Error while compiling with -${opt}, skipping..."
        continue
    fi
    
    # Running tests for each thread count
    for threads in "${THREAD_COUNTS[@]}"; do
        THREAD_DIR="${LOG_DIR}/${opt}/threads_${threads}"
        mkdir -p "${THREAD_DIR}"
        
        # Processing each input image
        for input in "${INPUT_FILES[@]}"; do
            if [ ! -f "${input}" ]; then
                echo "Input file ${input} does not exist, skipping..."
                continue
            fi
            
            OUTPUT_IMAGE="${OUTPUT_DIR}/output_images/${opt}_threads${threads}_${input}"
            LOG_FILE="${THREAD_DIR}/${input%.*}.log"
            
            echo "Starting: ${input} with ${threads} threads (${opt})..."
            
            # Running the program with parameters
            OMP_NUM_THREADS=${threads} ./${BINARY} \
                "${input}" \
                "${OUTPUT_IMAGE}" \
                "${KERNEL_ARGS[@]}" > "${LOG_FILE}" 2>&1

            # Additional processing of the log file
            echo "----------------------------------" >> "${LOG_FILE}"
            echo "Optimization: ${opt}" >> "${LOG_FILE}"
            echo "Number of threads: ${threads}" >> "${LOG_FILE}"
            echo "Finished processing: ${input}" >> "${LOG_FILE}"
            echo "==================================" >> "${LOG_FILE}"

            AVG_TIME=$(grep "Average time" "${LOG_FILE}" | awk '{print $3}')
            echo "Extracted average time: ${AVG_TIME}μs"
            echo "${opt},${threads},${input},${AVG_TIME}" >> "${CSV_FILE}"
        done
    done
done

# Kreiranje rezimea
SUMMARY_FILE="${OUTPUT_DIR}/all_results.txt"
echo "Summary of performance" > "${SUMMARY_FILE}"
echo "==================================" >> "${SUMMARY_FILE}"

find "${LOG_DIR}" -name "*.log" -exec cat {} \; >> "${SUMMARY_FILE}"

echo "All tests completed. Results are in the ${OUTPUT_DIR} directory."