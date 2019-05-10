
#include "svd.h"


using Eigen::MatrixXd;
using namespace Eigen;
using namespace std;

#define BATCH_SIZE 1000
#define PARSE_THROUGH 1


void decompose_CPU(stringstream& buffer,
    int batch_size,
    int num_users,
    int num_items,
    int num_f,
    float step_size,
    float regulation) {

  MatrixXf P(num_users, num_f);
    MatrixXf Q(num_f, num_items);

    MatrixXf R(num_users, num_items);
    gaussianFill(P, num_users, num_f);
    gaussianFill(Q, num_f, num_items);
    vector< vector<int> > data = vector< vector<int> > ();
    // create a vector to store all of training data

 int review_idx = 0;
    for (string user_rate; getline(buffer, user_rate); ++review_idx) {
        int host_buffer[3];
        readData(user_rate, &host_buffer[0]);
        host_buffer[0]--; // transform 1-based data to 0-based index
        host_buffer[1]--;
        vector<int> line(begin(host_buffer), end(host_buffer));
        data.push_back(line);
        R(host_buffer[0], host_buffer[1]) = host_buffer[2]; // record the rating
    }



    float RMS = 0;
    float RMS_new = 0;
    float delta = 1;
    float delta_new = 1;

    MatrixXf R_1 = P * Q;
    for (int i = 0; i < num_users; ++i) {
        for (int j = 0; j < num_items; ++j) {
            if (R(i, j) != 0) {
                RMS += (R_1(i, j) - R(i, j)) * (R_1(i, j) - R(i, j));
            }
        }
    }
    RMS /= review_idx;
    RMS = sqrt(RMS);
 //   cout << RMS << endl;
    RMS_new = RMS;

  while (delta_new / delta >= 0.02) {
        cout << "stop condition: " << (delta_new / delta) << endl;
        RMS = RMS_new;
        delta = delta_new;
        int iteration = data.size() / batch_size;
        for (int i = 0; i < iteration; ++i) {
            for (int j = 0; j < batch_size; ++j) {
                vector<int> rating = data[i * batch_size + j];
        //      int xxx=rating.size();
        //      for(i=0;i<xxx;i++)
        //              cout << rating[i] << endl;
                float e = rating[2] - P.row(rating[0]).dot(Q.col(rating[1]));
                P.row(rating[0]) += step_size * (e * (Q.col(rating[1])).transpose() - regulation * P.row(rating[0]));
                Q.col(rating[1]) += step_size * (e * (P.row(rating[0])).transpose() - regulation * Q.col(rating[1]));
            }
            R_1 = P * Q;
            RMS_new = 0;
            for (int k = 0; k < num_users; ++k) {
                for (int j = 0; j < num_items; ++j) {
                    if (R(k, j) != 0) {
                        RMS_new += (R_1(k, j) - R(k, j)) * (R_1(k, j) - R(k, j));
                    }
                }
            }
            RMS_new /= review_idx;
            RMS_new = sqrt(RMS_new);
//          cout << "RMS: " << RMS_new << endl;
        }
        delta_new = RMS - RMS_new;
        cout << "delta_new: " << delta_new << endl;



	        if (PARSE_THROUGH) {
            break;
        } else {
            random_shuffle(data.begin(), data.end());
        }
    }
    printf("Training complete, writing result rating matrix to CSV....\n");
    writeCSV(R_1, "output_CPU.csv");
}

void decompose_GPU(stringstream& buffer,
    int batch_size,
    int num_users,
    int num_items,
    int num_f,
    float step_size,
    float regulation) {

    float *host_P = (float*)malloc(sizeof(float) * num_users * num_f); // host of R
    float *host_Q = (float*)malloc(sizeof(float) * num_f * num_items); // host of Q

    float *host_R = (float*)malloc(sizeof(float) * num_users * num_items); // host of R, 'correct result'

    gaussianFill(host_P, num_users, num_f);
    gaussianFill(host_Q, num_f, num_items);
    memset(host_R, 0, sizeof(float) * num_users * num_items);

      int review_idx = 0;
    const unsigned int blocks = 64;
    const unsigned int threadsPerBlock = 64;

    float *dev_P;
    cudaMalloc((void**) &dev_P, sizeof(float) * num_users * num_f);
    cudaMemcpy(dev_P, host_P, sizeof(float) * num_users * num_f, cudaMemcpyHostToDevice);

    float *dev_Q;
    cudaMalloc((void**) &dev_Q, sizeof(float) * num_f * num_items);
    cudaMemcpy(dev_Q, host_Q, sizeof(float) * num_f * num_items, cudaMemcpyHostToDevice);

    int *host_buffer = (int*) malloc(sizeof(int) * 3 * batch_size);
    int *dev_data;
    cudaMalloc((void**) &dev_data, sizeof(int) * 3 * batch_size);

     for (string user_rate; getline(buffer, user_rate); ++review_idx) {
        int idx = review_idx % batch_size;
        readData(user_rate, &host_buffer[3 * idx]);
        host_buffer[3 * idx]--; // the user and item are 1 indexed, in the matrix it should be 0 indexed
        host_buffer[3 * idx + 1]--;
        if (idx == batch_size - 1) { // when the buffer is full

            gpuErrChk(cudaMemcpy(dev_data, host_buffer, sizeof(int) * 3 * batch_size, cudaMemcpyHostToDevice));
            cudaCallTrainingKernel(blocks,
                    threadsPerBlock,
                    dev_data,
                    dev_P,
                    dev_Q,
                    step_size,
                    regulation,
                    num_users,
                    num_items,
                    num_f,
                    batch_size);
            gpuErrChk(cudaMemcpy(host_P, dev_P, sizeof(float) * num_users * num_f, cudaMemcpyDeviceToHost));

        }
        host_R[ host_buffer[3 * idx] * num_items + host_buffer[3 * idx + 1] ] = host_buffer[3 * idx + 2]; // read in the R data
    }



       // the correct R matrix
    float *dev_R0;
    gpuErrChk(cudaMalloc((void**) &dev_R0, sizeof(float) * num_users * num_items));
    gpuErrChk(cudaMemcpy(dev_R0, host_R, sizeof(float) * num_users * num_items, cudaMemcpyHostToDevice));

    float *dev_R1;
    gpuErrChk(cudaMalloc((void**) &dev_R1, sizeof(float) * num_users * num_items));
    gpuErrChk(cudaMemset(dev_R1, 0, sizeof(float) * num_users * num_items));

    float RMS = 0;

    cudaCallMultiplyKernel(blocks,
        threadsPerBlock,
        dev_P,
        dev_Q,
        dev_R1,
        num_users,
        num_items,
        num_f);
    // compare the RMS loss between dev_R0 and dev_R1
    RMS = cudaCallFindRMSKernel(blocks,
        threadsPerBlock,
        dev_R0,
        dev_R1,
        num_users,
        num_items);
    cout << "GPU SUM of RMS: " << RMS << endl;
    RMS /= review_idx;
    RMS = sqrt(RMS);
    cout << "GPU RMS: " << RMS << endl;

        float *host_R_1 = (float*)malloc(sizeof(float) * num_users * num_items);
    gpuErrChk(cudaMemcpy(host_R_1, dev_R1, sizeof(float) * num_users * num_items, cudaMemcpyDeviceToHost));
    printf("Training complete in GPU, writing result rating matrix to CSV....\n");
    writeCSV(host_R_1, num_users, num_items, "output_GPU.csv");

    free(host_P);
    free(host_Q);
    free(host_R);

    cudaFree(dev_P);
    cudaFree(dev_Q);
    cudaFree(dev_R0);
    cudaFree(dev_R1);
    free(host_R_1);
    free(host_buffer);
    cudaFree(dev_data);

}

int main(int argc, char* argv[]) {
    int num_users;
    int num_items;
    int num_f;
    if (argc == 2) {
        num_users = 943;
        num_items = 1682;
        num_f = 30;
    } else if (argc == 5){
        num_users = atoi(argv[2]);
        num_items = atoi(argv[3]);
        num_f = atoi(argv[4]);
    } else {
        printf("./cuda_SVD <path to training datafile> optional \
            (<number of users> <number of items> <number of dimensions f>)\n");
        return -1;
    }
    const float gamma = 0.001;
    const float lamda = 0.0005;

    float time_initial, time_final;

    ifstream infile_cpu(argv[1]); // the training data
    ifstream infile_gpu(argv[1]); // the testing data

    stringstream buffer1, buffer2;
    buffer1 << infile_cpu.rdbuf();
    buffer2 << infile_gpu.rdbuf();

    // CPU decomposition
    time_initial = clock();
    decompose_CPU(buffer1, BATCH_SIZE, num_users, num_items, num_f, gamma, lamda);
    time_final = clock();
    printf("Total time to run decomposition on CPU: %f (s)\n", (time_final - time_initial) / CLOCKS_PER_SEC);
    // end of CPU decomposition

    // GPU decomposition
    float gputime = -1;
    START_TIMER();
    decompose_GPU(buffer2, BATCH_SIZE, num_users, num_items, num_f, gamma, lamda);
    STOP_RECORD_TIMER(gputime);
    printf("Total time to run decomposition on GPU: %f (s)\n", gputime/1000);
    // end of GPU decomposition

    return 1;
}
                                                                                                                            264,1         Bot

