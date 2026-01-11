#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>
#include "job.h"

Job *parse_lines(char *cmd_strings[], size_t num_strings, size_t *num_valid_jobs);

#endif
