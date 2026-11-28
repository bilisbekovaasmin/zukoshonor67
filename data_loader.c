
#include "data_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_LINE 4096

/* Split a CSV row, including quoted fields */
static int split_csv(char *line, char *fields[], int max_fields)
{
    int count = 0;
    int quoted = 0;

    if (max_fields <= 0)
        return -1;

    fields[count++] = line;

    for (char *p = line; *p != '\0'; p++) {
        if (*p == '"') {
            quoted = !quoted;
        }
        else if (*p == ',' && !quoted) {
            if (count >= max_fields)
                return -1;

            *p = '\0';
            fields[count++] = p + 1;
        }
        else if ((*p == '\n' || *p == '\r') && !quoted) {
            *p = '\0';
            break;
        }
    }

    return quoted ? -1 : count;
}


/* Read resource information */
int load_resources(const char *filename,
                   ResourceInfo **resources)
{
    *resources = NULL;

    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror(filename);
        return -1;
    }

    char line[MAX_LINE];
    char *fields[8];
    int count = 0;

    /* Skip the CSV header */
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {

        if (split_csv(line, fields, 8) != 8)
            goto error;

        ResourceInfo *temp = realloc(
            *resources,
            (count + 1) * sizeof(ResourceInfo)
        );

        if (temp == NULL)
            goto error;

        *resources = temp;

        ResourceInfo *r = &(*resources)[count];

        r->id = count + 1;

        snprintf(r->scheme, sizeof(r->scheme),
                 "%s", fields[0]);

        snprintf(r->host, sizeof(r->host),
                 "%s", fields[1]);

        snprintf(r->path, sizeof(r->path),
                 "%s", fields[2]);

        r->size_bytes = (size_t)strtoull(
            fields[5], NULL, 10
        );

        r->origin_latency_ms = atoi(fields[6]);

        count++;
    }

    if (ferror(file))
        goto error;

    fclose(file);
    return count;

error:
    fclose(file);
    free(*resources);
    *resources = NULL;
    return -1;
}


/* Read client requests */
int load_requests(const char *filename,
                  Request **requests)
{
    *requests = NULL;

    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror(filename);
        return -1;
    }

    char line[MAX_LINE];
    char *fields[6];
    int count = 0;

    /* Skip the CSV header */
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {

        if (split_csv(line, fields, 6) != 6)
            goto error;

        Request *temp = realloc(
            *requests,
            (count + 1) * sizeof(Request)
        );

        if (temp == NULL)
            goto error;

        *requests = temp;

        Request *r = &(*requests)[count];

        snprintf(r->timestamp, sizeof(r->timestamp),
                 "%s", fields[0]);

        snprintf(r->client_id, sizeof(r->client_id),
                 "%s", fields[1]);

        snprintf(r->method, sizeof(r->method),
                 "%s", fields[2]);

        snprintf(r->scheme, sizeof(r->scheme),
                 "%s", fields[3]);

        snprintf(r->host, sizeof(r->host),
                 "%s", fields[4]);

        snprintf(r->path, sizeof(r->path),
                 "%s", fields[5]);

        count++;
    }

    if (ferror(file))
        goto error;

    fclose(file);
    return count;

error:
    fclose(file);
    free(*requests);
    *requests = NULL;
    return -1;
}


/* Find the resource associated with a request */
int find_resource(const ResourceInfo *resources,
                  int count,
                  const Request *request)
{
    for (int i = 0; i < count; i++) {

        if (strcmp(resources[i].scheme,
                   request->scheme) == 0 &&

            strcmp(resources[i].host,
                   request->host) == 0 &&

            strcmp(resources[i].path,
                   request->path) == 0) {

            return i;
        }
    }

    return -1;
}
