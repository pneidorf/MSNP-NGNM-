#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <pcap.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#define ETHERNET_HEADER_SIZE 14

typedef unsigned char byte;

void display_packet_info(const byte *data) {
    struct ip *ip_header = (struct ip *)(data + ETHERNET_HEADER_SIZE);
    const byte *payload = data + ETHERNET_HEADER_SIZE + (ip_header->ip_hl * 4); 
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);
    
    printf("Source IP: %s\n", src_ip);
    printf("Destination IP: %s\n", dst_ip);
    printf("IP Header Size: %d bytes\n", ip_header->ip_hl * 4);
    printf("Total Size: %d bytes\n", ip_header->ip_len);
    printf("Payload:\n");
    
    for (int i = 0; i < ip_header->ip_len - (ip_header->ip_hl * 4); ++i) {
        printf("%x", (byte)*(payload + i));
        if (i != 0 && i % 40 == 0) {
            printf("\n");
            if (i > 1000) {
                break;
            }
        }
    }
    printf("\n---\n");
}

void process_packet(byte *args, const struct pcap_pkthdr *header, const byte *data) {
    printf("Processing packet...\n");
}

int initialize_packet(size_t length, const byte *data) {
    struct pcap_pkthdr hdr;
    hdr.ts.tv_sec = time(NULL); 
    hdr.ts.tv_usec = 0; 
    hdr.caplen = length; 
    hdr.len = length; 
    display_packet_info(data);
}

#define OPTION 2

#if OPTION == 1

int main(void) {
    void *zmq_context = zmq_ctx_new();
    void *zmq_responder = zmq_socket(zmq_context, ZMQ_REP);  // Изменено имя переменной
    int result_code = zmq_bind(zmq_responder, "tcp://*:2001");
    assert(result_code == 0);

    char *interface_name;
    char error_buffer[PCAP_ERRBUF_SIZE];
    pcap_t *capture_handle;
    pcap_if_t *devices;
    const byte *packet_data;
    struct pcap_pkthdr packet_header;
    int max_packets = 10;
    int timeout_ms = 10000; /* In milliseconds */

#if 1
    printf("%s:%d\n", __func__, __LINE__);

    pcap_findalldevs(&devices, error_buffer);
    if (devices == NULL) {
        printf("Error finding devices: %s\n", error_buffer);
        return 1;
    }

    capture_handle = pcap_open_live(
            devices->name,
            BUFSIZ,
            max_packets,
            timeout_ms,
            error_buffer
        );
    printf("%s:%d\n", __func__, __LINE__);
#endif

    printf("%s:%d\n", __func__, __LINE__);

    while (1) {
        char recv_buffer[1024];

        pcap_loop(capture_handle, 0, process_packet, (byte *)zmq_responder);
        
        printf("%s:%d\n", __func__, __LINE__);
  
        printf("Received Hello\n");
        sleep(1);        
        zmq_send(zmq_responder, "World", 5, 0);
    }
    return 0;
}
#endif

#if OPTION == 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <zmq.h>

void process_packet_v2(byte *args, const struct pcap_pkthdr *header, const byte *data) {
    void *zmq_socket = args;
    printf("%s:%d\n", __func__, __LINE__);

    display_packet_info(data);
}

int main() {
    void *zmq_context = zmq_ctx_new();
    void *zmq_publisher = zmq_socket(zmq_context, ZMQ_PUSH);  // Изменено имя переменной
    zmq_bind(zmq_publisher, "tcp://*:2001");
    printf("%s:%d\n", __func__, __LINE__);

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *all_devices, *current_device;

    if (pcap_findalldevs(&all_devices, errbuf) == -1) {
        fprintf(stderr, "Couldn't find devices: %s\n", errbuf);
        return 1;
    }
    
    printf("Available interfaces:\n");
    for (current_device = all_devices; current_device; current_device = current_device->next) {
        if (strcmp(current_device->name, "lo") == 0) {
            break;
        }
    }
    
    if (current_device == NULL) {
        fprintf(stderr, "No devices found.\n");
        return 1;
    }
    
    pcap_t *capture_handle = pcap_open_live(current_device->name, BUFSIZ, 1, 1000, errbuf);
    if (capture_handle == NULL) {
        fprintf(stderr, "Could not open device %s: %s\n", current_device->name, errbuf);
        pcap_freealldevs(all_devices);
        return 1;
    }
    
    printf("%s:%d\n", __func__, __LINE__);

    int status = pcap_loop(capture_handle, 0, process_packet_v2, (byte *)zmq_publisher);
    printf("%s:%d\n", __func__, __LINE__);
    
    pcap_close(capture_handle);
    zmq_close(zmq_publisher);
    zmq_ctx_destroy(zmq_context);
    pcap_freealldevs(all_devices);

    return (status == -1) ? 1 : 0;
}

#endif

