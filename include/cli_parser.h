#ifndef CLI_PARSER_H
#define CLI_PARSER_H

#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Config {
    const char *policy;
    int quantum_ms;
    const char *workload_path;
    bool verbose;
} Config;

void print_usage(const char *prog);

bool parse_positive_int(const char *s, int *out);

bool parse_args(int argc, char *argv[], Config *config);





#endif
