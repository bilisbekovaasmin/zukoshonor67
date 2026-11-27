
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <stddef.h>

/* Resource information from resources.csv */
typedef struct {
    int id;
    char scheme[16];
    char host[256];
    char path[1024];
    size_t size_bytes;
    int origin_latency_ms;
} ResourceInfo;

/* Request information from requests_reference.csv */
typedef struct {
    char timestamp[32];
    char client_id[64];
    char method[16];
    char scheme[16];
    char host[256];
    char path[1024];
} Request;

/* Function declarations */
int load_resources(const char *filename,
                   ResourceInfo **resources);

int load_requests(const char *filename,
                  Request **requests);

int find_resource(const ResourceInfo *resources,
                  int count,
                  const Request *request);

#endif
