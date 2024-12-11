#include <iostream>
#include <mutex>
#include <zmq.h>
#include <thread>
#include <cstring>
#include <cmath>
#include <unistd.h>

#define BUFFER_MAX 1024 * 1024

static bool run;
std::mutex mutex_send_matlab;

enum class ARGV_CONSOLE {
    ARGV_PORT_1 = 1,
    ARGV_PORT_2,
    ARGV_PORT_1_PROXY,
    ARGV_PORT_2_PROXY,
    ARGV_PORT_API,
    ARGV_MAX
};

void send_to_matlab(char *data, int size, void *socket_api) {
    static char buffer[1000];
    mutex_send_matlab.lock();
    zmq_send(socket_api, data, size, 0);
    // int size_recv = zmq_recv(socket_api, buffer, sizeof(buffer), 0);
    // printf("size msg from matlab: %d\n", size_recv);
    mutex_send_matlab.unlock();
}   

float CostHata(void *data, int size, double h_enb, double h_ue, double d) {
    double fc = 800; // Частота в МГц
    double hte = h_enb; // Высота передающей антенны в метрах
    double hre = h_ue; // Высота приемной антенны в метрах
    double Cm = 0; // Поправочный коэффициент для средних городов и пригородов
    

    double a_hre = (1.1 * log10(fc) - 0.7) * hre - (1.56 * log10(fc) - 0.8);
    

    // double L = 46.3 + 33.9 * log10(fc) - 13.82 * log10(hte) - a_hre + 
    //            (44.9 - 6.55 * log10(hte)) * log10(d) + Cm;
    double L = 46.3 + 33.9 * log10(fc) - 13.82 * log10(hte) - hre + (44.9 - 6.55 * log10(hte)) * log10(d + Cm);


    double ds = pow(10.f, L / 10.f);
    printf("ds = %f, L - %f, d = %f\n", ds, L, d);
    // float *data_f = (float*)data;
    // for (size_t i = 0; i < size/4; ++i) {
    //     // data_f[i] = data_f[i] - (float)L;
    //     if(data_f[i] > 0.f) {
    //         // printf("%f\n", data_f[i]);
    //     }
    // }
    return (float) ds;
}

void processing_matlab(char *data, int size_data, int size_send, int &size_recv, void *socket_api) {
    mutex_send_matlab.lock();
    zmq_send(socket_api, data, size_send, 0);
    size_recv = size_send;
    char *data2 = new char[size_send];
    memcpy(data2, data, size_send);
    // printf("[%d] send request[%d]...\n", id, size_send);


    size_recv = zmq_recv(socket_api, data, size_data, 0);
    mutex_send_matlab.unlock();
}   

int iter = 0;

double arr1_m[] = {10, 50, 100, 300, 500, 1000, 4000, 9000, 12000, 14000, 15000, 16000, 19000, 20000};
int index1 = 0;



void add_noise(float *data, int size) {



    if(iter > 1000) {
        printf("new val: %d, index1: %d\n", arr1_m[index1], index1);
        index1++;
        if(index1 >= sizeof(arr1_m) / 8) {
            index1 = sizeof(arr1_m) / 8 - 1;
        }

        iter = 0;
    }
    ++iter;


    // float db_r = CostHata(data, size, 50, 1.5, arr1_m[index1] / 100000.f);
    float f = 2.56;
    float db_r = 28 + 22 * log10(arr1_m[index1] / 1000.f) + 20* log10(f);
    printf("db_r = %f\n", db_r);
    for(int i = 0; i < size; ++i) {
        // data[i] = data[i] / 1.5 - (float)(rand() % arr1[index1]);
        data[i] = data[i] / db_r - (rand() % 100);
        

    }

}

void processing_data(char *data, int size_data, int size_send, int &size_recv, void *socket_api) {
    // CostHata(data, size_send, 50, 1.5, 10);
    mutex_send_matlab.lock();
    // zmq_send(socket_api, data, size_send, 0);
    size_recv = size_send;
    // char *data2 = new char[size_send];
    // memcpy(data2, data, size_send);
    add_noise((float*)data, size_send / 4);

    // size_recv = zmq_recv(socket_api, data, size_data, 0);
    mutex_send_matlab.unlock();
} 


void thread_proxy(void *zrecv, void *zsend, void *socket_api, int id) {
    
    char buffer[BUFFER_MAX];
    int size;
    int size_recv;
    printf("[%d] start\n", id);
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        size = zmq_recv(zrecv, buffer, sizeof(buffer), 0);
        
        if(size == -1) {
            continue;
        }
        // printf("[%d] send request[%d]...\n", id, size);
        // if(size > 1000)
        //     apply_attenuation(buffer, size, 1070000);
        // processing_matlab(buffer, BUFFER_MAX, size, size_recv, socket_api);
        if(size > 1000)
            processing_data(buffer, BUFFER_MAX, size, size_recv, socket_api);
        
        zmq_send(zsend, buffer, size, 0);
        // send_to_matlab(buffer, size, socket_api);
    }
}

void thread_proxy_2(void *zrecv, void *zsend, void *socket_api, int id) {
    
    char buffer[BUFFER_MAX];
    int size;
    int size_recv;
    printf("[%d] start\n", id);
    while(1) {
        memset(buffer, 0, sizeof(buffer));
        size = zmq_recv(zsend, buffer, sizeof(buffer), 0);
        if(size == -1) {
            continue;
        }
        // printf("[%d] send response[%d]...\n", id, size);
        // if(size > 1000)
        //     apply_attenuation(buffer, size, 1070000);
        // processing_matlab(buffer, BUFFER_MAX, size, size_recv, socket_api);
        processing_data(buffer, BUFFER_MAX, size, size_recv, socket_api);
        
        zmq_send(zrecv, buffer, size, 0);
        
        // send_to_matlab(buffer, size, socket_api);
    }
}


int main(int argc, char *argv[]){

    if(argc < static_cast<int>(ARGV_CONSOLE::ARGV_MAX) - 1) {
        printf("Error: not found argv\n");
        return -1;
    }
    {
        FILE *file = fopen("../statistics/statistics.txt", "w");
        if(file) {
            fclose(file);
        }
    }
    int port1 = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_1)]);
    int port2 = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_2)]);
    int port1_proxy = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_1_PROXY)]);
    int port2_proxy = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_2_PROXY)]);
    int port_api = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_API)]);
    
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    void *requester2 = zmq_socket (context, ZMQ_REQ);
    void *requester_api = zmq_socket (context, ZMQ_REQ);
    printf("%d %d %d %d\n", port1, port2, port1_proxy, port2_proxy);
    std::string addr_recv_1 = "tcp://localhost:" + std::to_string(port1);
    std::string addr_recv_2 = "tcp://localhost:" + std::to_string(port2);
    std::string addr_send_1 = "tcp://*:" + std::to_string(port1_proxy);
    std::string addr_send_2 = "tcp://*:" + std::to_string(port2_proxy);
    std::string addr_socket_api = "tcp://localhost:" + std::to_string(port_api);
    printf("connecting to %s...\n", addr_recv_1.c_str());
    zmq_connect (requester, addr_recv_1.c_str());
    printf("connecting to %s...\n", addr_recv_2.c_str());
    zmq_connect (requester2, addr_recv_2.c_str());
    printf("connecting to %s...\n", addr_socket_api.c_str());
    zmq_connect (requester_api, addr_socket_api.c_str());
    void *socket_proxy1 = zmq_socket(context, ZMQ_REP);
    void *socket_proxy2 = zmq_socket(context, ZMQ_REP);
    if(!socket_proxy1) {
        perror("zmq_socket");
        return 1;
    }
    if(!socket_proxy2) {
        perror("zmq_socket");
        return 1;
    }
    
    if(zmq_bind(socket_proxy1, addr_send_1.c_str()) != 0) {
        perror("zmq_bind");
        return 1;
    }
    if(zmq_bind(socket_proxy2, addr_send_2.c_str())) {
        perror("zmq_bind");
        return 1;
    }
    printf("[Init]\n");
    std::thread thr1;
    std::thread thr2;
    std::thread thr3;
    std::thread thr4;
    thr1 = std::thread(thread_proxy, socket_proxy1, requester, requester_api, 1);
    thr1.detach();
    thr3 = std::thread(thread_proxy_2, socket_proxy1, requester, requester_api, 2);
    thr3.detach();
    thr2 = std::thread(thread_proxy, socket_proxy2, requester2, requester_api, 3);
    thr2.detach();
    thr4 = std::thread(thread_proxy_2, socket_proxy2, requester2, requester_api, 4);
    thr4.detach();
    while(1) {
        sleep(1);
    }
    printf("End client\n");
    zmq_close (requester);
    zmq_close (requester2);
    zmq_close (socket_proxy1);
    zmq_close (socket_proxy2);
    zmq_close (requester_api);
    zmq_ctx_destroy (context);
    printf("[Clear]\n");
    return 0;
}









int maintest(int argc, char *argv[]){

    if(argc < static_cast<int>(ARGV_CONSOLE::ARGV_MAX) - 1) {
        printf("Error: not found argv\n");
        return -1;
    }
    int port1 = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_1)]);
    int port2 = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_2)]);
    int port1_proxy = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_1_PROXY)]);
    int port2_proxy = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_2_PROXY)]);
    int port_api = std::stoi(argv[static_cast<int>(ARGV_CONSOLE::ARGV_PORT_API)]);
    
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    void *requester2 = zmq_socket (context, ZMQ_REQ);
    void *requester_api = zmq_socket (context, ZMQ_REQ);
    printf("%d %d %d %d\n", port1, port2, port1_proxy, port2_proxy);
    std::string addr_recv_1 = "tcp://localhost:" + std::to_string(port1);
    std::string addr_recv_2 = "tcp://localhost:" + std::to_string(port2);
    std::string addr_send_1 = "tcp://*:" + std::to_string(port1_proxy);
    std::string addr_send_2 = "tcp://*:" + std::to_string(port2_proxy);
    std::string addr_socket_api = "tcp://localhost:" + std::to_string(port_api);
    printf("connecting to %s...\n", addr_recv_1.c_str());
    zmq_connect (requester, addr_recv_1.c_str());
    printf("connecting to %s...\n", addr_recv_2.c_str());
    zmq_connect (requester2, addr_recv_2.c_str());
    printf("connecting to %s...\n", addr_socket_api.c_str());
    zmq_connect (requester_api, addr_socket_api.c_str());
    void *socket_proxy1 = zmq_socket(context, ZMQ_REP);
    void *socket_proxy2 = zmq_socket(context, ZMQ_REP);
    if(!socket_proxy1) {
        perror("zmq_socket");
        return 1;
    }
    if(!socket_proxy2) {
        perror("zmq_socket");
        return 1;
    }
    
    if(zmq_bind(socket_proxy1, addr_send_1.c_str()) != 0) {
        perror("zmq_bind");
        return 1;
    }
    if(zmq_bind(socket_proxy2, addr_send_2.c_str())) {
        perror("zmq_bind");
        return 1;
    }
    printf("[Init]\n");

    unsigned char buffer[BUFFER_MAX];
    float data[] = {12, 3.4, 43, 23.3};
    unsigned char *rt = (unsigned char*)data;
    int size = 4 * 4;
    for(int i = 0; i < size; ++i) {
        printf("%u ", rt[i]);
        
    }
    printf("\n");
    zmq_send(requester_api, (char*)data, size, 0);
    printf("size send - %d\n", size);

    int size_recv = zmq_recv(requester_api, buffer, BUFFER_MAX, 0);
    printf("recv: %d\n", size_recv);


    for(int i = 0; i < size_recv; ++i) {
        printf("%u ", buffer[i]);
        
    }
    printf("\n");
    float *dfg = (float*)buffer;
    for(int i = 0; i < size_recv / 4; ++i) {
        printf("%f ", dfg[i]);
        
    }
    printf("\n");
    return 0;
}






























