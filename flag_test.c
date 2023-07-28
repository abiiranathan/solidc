#include <stdint.h>

#define FLAG_CAPACITY 8

// Or Pass -DFLAG_CAPACITY=8 to compiler

#include "flag.h"

int main(int argc, char** argv) {
    char** addr = flag_str("addr", "localhost:8080", "The server address");
    long int* port = flag_int("port", 8080, "The port to listen on");
    float* minutes = flag_float("minutes", 60.0f, "The TTL");
    size_t* storage = flag_size("storage", 500, "The amount of bytes to store");
    double* price = flag_double("price", 500.05, "Price in dollars");
    uint64_t* ptr = flag_uint64("ptr", 8, "Pointer size");
    bool* use_tls = flag_bool("tls", false, "Use TLS");

    flag_parse(argc, argv);

    printf("Addr: %s\n", *addr);
    printf("Port: %ld\n", *port);
    printf("Min: %lf\n", *minutes);
    printf("TLS Enabled: %d\n", *use_tls);
    printf("Bytes to store: %lu\n", *storage);
    printf("Price: %lf\n", *price);
    printf("Ptr size: %lu\n", *ptr);
}
