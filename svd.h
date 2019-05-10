
#ifndef _INCL_GUARD

#define _INCL_GUARD
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <iterator>

#include <eigen3/Eigen/Dense>
//#include <cuda.h>
#include <cuda_runtime.h>
#include "svd.cuh"

using namespace Eigen;
using namespace std;

#define gpuErrChk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(
    cudaError_t code,
    const char *file,
    int line,
    bool abort=true)
{
    if (code != cudaSuccess) {
        fprintf(stderr,"GPUassert: %s %s %d\n",
            cudaGetErrorString(code), file, line);
        exit(code);
    }
}


cudaEvent_t start;
cudaEvent_t stop;

#define START_TIMER() {                         \
    gpuErrChk(cudaEventCreate(&start));         \
    gpuErrChk(cudaEventCreate(&stop));          \
    gpuErrChk(cudaEventRecord(start));          \
}

#define STOP_RECORD_TIMER(name) {                           \
    gpuErrChk(cudaEventRecord(stop));                       \
    gpuErrChk(cudaEventSynchronize(stop));                  \
    gpuErrChk(cudaEventElapsedTime(&name, start, stop));    \
    gpuErrChk(cudaEventDestroy(start));                     \
    gpuErrChk(cudaEventDestroy(stop));                      \
}


void gaussianFill(MatrixXf &output, int size_row, int size_col) {
    std::default_random_engine generator(2015);
    std::normal_distribution<float> distribution(0.0, 0.1);
    for (int i=0; i < size_row; ++i) {
        for (int j = 0; j < size_col; ++j) {
            output(i, j) = distribution(generator);
        }
    }
}



void gaussianFill(float* output, int size_row, int size_col) {
    std::default_random_engine generator(2015);
    std::normal_distribution<float> distribution(0.0, 0.1);
    for (int i=0; i < size_row * size_col; ++i) {
        output[i] = distribution(generator);
    }
}


void writeCSV(MatrixXf R, string filename) {
    int r = R.rows();
    int c = R.cols();
    ofstream outputfile;
    outputfile.open(filename);
    for (int i = 0; i < r; ++i) {
        for (int j = 0; j < c; ++j) {
            outputfile << R(i,j) << ",";
        }
        outputfile << "\n";
    }
    outputfile.close();
}

void writeCSV(float* R, int num_users, int num_items, string filename) {
    ofstream outputfile;
    outputfile.open(filename);
    for (int i = 0; i < num_users; ++i) {
        for (int j = 0; j < num_items; ++j) {
            outputfile << R[i * num_items + j] << ",";
        }
        outputfile << "\n";
    }
    outputfile.close();
}


void readData(string str, int *output) {
    stringstream stream(str);
    int idx = 0;
    for (string component; getline(stream, component, '\t'); ++idx) {
        if (idx == 3) break;
        output[idx] = atoi(component.c_str());
    }
}

void decompose_CPU(stringstream& buffer,
    int batch_size,
    int num_users,
    int num_items,
    int num_f,
    float step_size,
    float regualtion);



void decompose_GPU(stringstream& buffer,
    int batch_size,
    int num_users,
    int num_items,
    int num_f,
    float step_size,
    float regulation);

#endif
                                                                                                                            132,1         Bot

