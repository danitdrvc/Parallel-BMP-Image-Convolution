#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>

using namespace std;

#pragma pack(push, 1)
struct BMPFileHeader {
    char signature[2];
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
};

struct BMPInfoHeader {
    uint32_t headerSize;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t importantColors;
};
#pragma pack(pop)

void readBMP(const char* filename, vector<uint8_t>& B, vector<uint8_t>& G, vector<uint8_t>& R, int& width, int& height) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;

    // Check fread return values
    if (fread(&fileHeader, sizeof(BMPFileHeader), 1, file) != 1) {
        perror("Error reading BMP header");
        fclose(file);
        exit(1);
    }
    if (fread(&infoHeader, sizeof(BMPInfoHeader), 1, file) != 1) {
        perror("Error reading BMP info header");
        fclose(file);
        exit(1);
    }

    if (infoHeader.bitsPerPixel != 24 || infoHeader.compression != 0) {
        fprintf(stderr, "Only 24-bit uncompressed BMP files are supported.\n");
        fclose(file);
        exit(1);
    }

    width = infoHeader.width;
    height = abs(infoHeader.height);
    bool topDown = infoHeader.height < 0;

    if (fseek(file, fileHeader.dataOffset, SEEK_SET) != 0) {
        perror("Error seeking to pixel data");
        fclose(file);
        exit(1);
    }

    int rowSize = (width * 3 + 3) & ~3;
    vector<uint8_t> row(rowSize);

    B.resize(width * height);
    G.resize(width * height);
    R.resize(width * height);

    for (int i = 0; i < height; ++i) {
        if (fread(row.data(), 1, rowSize, file) != rowSize) {
            perror("Error reading pixel data");
            fclose(file);
            exit(1);
        }
        int y = topDown ? i : height - 1 - i;
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            B[idx] = row[x * 3];
            G[idx] = row[x * 3 + 1];
            R[idx] = row[x * 3 + 2];
        }
    }

    fclose(file);
}

void writeBMP(const char* filename, const vector<uint8_t>& B, const vector<uint8_t>& G, const vector<uint8_t>& R, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Error creating file");
        exit(1);
    }

    BMPFileHeader fileHeader = {'B', 'M'};
    BMPInfoHeader infoHeader = {
        sizeof(BMPInfoHeader), width, height, 1, 24, 0, 0, 0, 0, 0, 0
    };

    int rowSize = (width * 3 + 3) & ~3;
    infoHeader.imageSize = rowSize * height;
    fileHeader.fileSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + infoHeader.imageSize;
    fileHeader.dataOffset = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

    fwrite(&fileHeader, sizeof(BMPFileHeader), 1, file);
    fwrite(&infoHeader, sizeof(BMPInfoHeader), 1, file);

    vector<uint8_t> row(rowSize);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            row[x * 3] = B[idx];
            row[x * 3 + 1] = G[idx];
            row[x * 3 + 2] = R[idx];
        }
        fwrite(row.data(), 1, rowSize, file);
    }

    fclose(file);
}

void convolveChannel(const vector<uint8_t>& input, int inWidth, int inHeight,
                     const vector<float>& kernel, int kernelRows, int kernelCols,
                     vector<uint8_t>& output) {
    int outHeight = inHeight - kernelRows + 1;
    int outWidth = inWidth - kernelCols + 1;
    output.resize(outHeight * outWidth);

    #pragma omp parallel for
    for (int i = 0; i < outHeight; ++i) {
        for (int j = 0; j < outWidth; ++j) {
            float sum = 0.0f;
            for (int kr = 0; kr < kernelRows; ++kr) {
                for (int kc = 0; kc < kernelCols; ++kc) {
                    int inputRow = i + kr;
                    int inputCol = j + kc;
                    sum += input[inputRow * inWidth + inputCol] * kernel[kr * kernelCols + kc];
                }
            }
            sum = max(0.0f, min(sum, 255.0f));
            output[i * outWidth + j] = static_cast<uint8_t>(sum + 0.5f);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.bmp output.bmp [kernel_rows kernel_cols kernel_values...]\n", argv[0]);
        return 1;
    }

    int kernelRows = 3, kernelCols = 3;
    vector<float> kernel(9, 0.0f);
    kernel[4] = 1.0f; // Default 3x3 identity kernel

    if (argc >= 5) {
        kernelRows = atoi(argv[3]);
        kernelCols = atoi(argv[4]);
        int kernelSize = kernelRows * kernelCols;
        if (argc != 5 + kernelSize) {
            fprintf(stderr, "Invalid number of kernel values. Expected %d\n", kernelSize);
            return 1;
        }
        kernel.resize(kernelSize);
        for (int i = 0; i < kernelSize; ++i)
            kernel[i] = atof(argv[5 + i]);
    }

    int width, height;
    vector<uint8_t> B, G, R;
    readBMP(argv[1], B, G, R, width, height);
    
    vector<uint8_t> outB, outG, outR;

    const int runs = 100;
    std::vector<long> timings;

    // WARMUP
    for (int i = 0; i < runs; ++i) {
        convolveChannel(B, width, height, kernel, kernelRows, kernelCols, outB);
        convolveChannel(G, width, height, kernel, kernelRows, kernelCols, outG);
        convolveChannel(R, width, height, kernel, kernelRows, kernelCols, outR);
    }

    // EXECUTION
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        convolveChannel(B, width, height, kernel, kernelRows, kernelCols, outB);
        convolveChannel(G, width, height, kernel, kernelRows, kernelCols, outG);
        convolveChannel(R, width, height, kernel, kernelRows, kernelCols, outR);
        auto end = std::chrono::high_resolution_clock::now();
        timings.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    }
    // Compute average
    long avg = std::accumulate(timings.begin(), timings.end(), 0L) / runs;

    // Compute variance
    long variance = 0;
    for (const auto& time : timings) {
        long diff = time - avg;
        variance += diff * diff;
    }
    variance /= runs;

    std::cout << "Average time: " << avg << " microseconds" << std::endl;
    std::cout << "Variance: " << variance << " microseconds" << std::endl;
    std::cout << "Std Dev: " << std::sqrt(variance) << " microseconds\n";

    
    int outWidth = width - kernelCols + 1;
    int outHeight = height - kernelRows + 1;
    vector<uint8_t> combinedB(outB.size()), combinedG(outG.size()), combinedR(outR.size());
    copy(outB.begin(), outB.end(), combinedB.begin());
    copy(outG.begin(), outG.end(), combinedG.begin());
    copy(outR.begin(), outR.end(), combinedR.begin());

    writeBMP(argv[2], combinedB, combinedG, combinedR, outWidth, outHeight);

    return 0;
}



// g++ -O0 -march=native -mavx2 -fopenmp -o convolve convolve.cpp
// g++ -O1 -march=native -mavx2 -fopenmp -o convolve convolve.cpp
// g++ -O2 -march=native -mavx2 -fopenmp -o convolve convolve.cpp
// g++ -O3 -march=native -mavx2 -fopenmp -o convolve convolve.cpp


// OMP_NUM_THREADS=1 ./convolve input50x50.bmp output.bmp 3 3 -1 -1 -1 -1 8 -1 -1 -1 -1

// OMP_NUM_THREADS=2 ./convolve input50x50.bmp output.bmp 3 3 -1 -1 -1 -1 8 -1 -1 -1 -1

// OMP_NUM_THREADS=4 ./convolve input50x50.bmp output.bmp 3 3 -1 -1 -1 -1 8 -1 -1 -1 -1

// OMP_NUM_THREADS=8 ./convolve input50x50.bmp output.bmp 3 3 -1 -1 -1 -1 8 -1 -1 -1 -1

// input10x10
// input50x50
// input500x500
// input1000x1000
// input2000x2000