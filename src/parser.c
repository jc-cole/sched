#include "job.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void append_to_argv(Job *job, size_t *token_char_count, size_t *argv_size, char *token_buffer) {
    job->argv[job->argc] = malloc(sizeof(char) * *token_char_count);
    if (job->argc + 1 == *argv_size) {
        *argv_size *= 2;
        job->argv = realloc(job->argv, sizeof(char*) * *argv_size);
    }
    token_buffer[*token_char_count] = '\0';
    strcpy(job->argv[job->argc], token_buffer);
    job->argc++;
}


static Job parse_line(char *cmd_string) {
    Job job = make_empty_job();

    size_t argv_size = 8;
    job.argv = malloc(sizeof(char*) * argv_size);
    
    enum state { SKIP_WS, IN_TOKEN, IN_S_QUOTE, IN_D_QUOTE };
    enum state parser_state = SKIP_WS;

    size_t cmd_str_len = strlen(cmd_string);
    char token_buffer[cmd_str_len + 1];
    size_t token_char_count = 0;
    
    for (size_t char_idx = 0; char_idx < cmd_str_len + 1; char_idx++) {

        char current_char = cmd_string[char_idx];

        if (current_char == ' ' || current_char == '\t' || current_char == '\n') {

            if (parser_state == IN_TOKEN) {
                append_to_argv(&job, &token_char_count, &argv_size, token_buffer);
                token_char_count = 0;
                parser_state = SKIP_WS;
            } else if (parser_state == IN_D_QUOTE || parser_state == IN_S_QUOTE) {
                token_buffer[token_char_count] = current_char;
                token_char_count++;
            }

        } else if (current_char == '\"') {

            if (parser_state == IN_D_QUOTE) {
                append_to_argv(&job, &token_char_count, &argv_size, token_buffer);
                token_char_count = 0;
                parser_state = SKIP_WS;
            } else if (parser_state == IN_S_QUOTE) {
                token_buffer[token_char_count] = current_char;
                token_char_count++;
            } else {
                parser_state = IN_D_QUOTE;
            }

        } else if (current_char == '\'') {

            if (parser_state == IN_S_QUOTE) {
                append_to_argv(&job, &token_char_count, &argv_size, token_buffer);
                token_char_count = 0;
                parser_state = SKIP_WS;
            } else if (parser_state == IN_D_QUOTE) {
                token_buffer[token_char_count] = current_char;
                token_char_count++;
            } else {
                parser_state = IN_S_QUOTE;
            }

        } else if (current_char == '#') {
            if (parser_state == IN_TOKEN) {
                append_to_argv(&job, &token_char_count, &argv_size, token_buffer);
                break;
            } else if (parser_state == SKIP_WS) {
                break;
            } 
        }
        else if (current_char == '\0') {
            if (parser_state == IN_TOKEN) {
                append_to_argv(&job, &token_char_count, &argv_size, token_buffer);
            } else if (parser_state == IN_S_QUOTE || parser_state == IN_D_QUOTE) {
                job.status = SYNTAX_ERROR;
            }
        } else {
            if (parser_state == IN_TOKEN || parser_state == IN_D_QUOTE || parser_state == IN_S_QUOTE) {
                token_buffer[token_char_count] = current_char;
                token_char_count++;
            } else if (parser_state == SKIP_WS) {
                token_buffer[token_char_count] = current_char;
                token_char_count++;
                parser_state = IN_TOKEN;
            }
            
            
        }
    }

    job.argv[job.argc] = NULL;

    if (job.status == SYNTAX_ERROR) {
        return job;
    }

    if (job.argc > 0) {
        job.status = EXEC_READY;
    }

    return job;

}

Job *parse_lines(char *cmd_strings[], size_t num_strings, size_t *num_valid_jobs) {
    Job *job_array = malloc(sizeof(Job) * num_strings);
    for (size_t i = 0; i < num_strings; i++) {
        job_array[i] = parse_line(cmd_strings[i]);
    }

    // clearing jobs that can't be exec'd due to being empty or syntax error

    size_t valid_jobs = 0;
    for (size_t i = 0; i < num_strings; i++) {
        if (job_array[i].status == EXEC_READY) {
            valid_jobs++;
        }
    }

    Job *cleaned_job_array = malloc(sizeof(Job) * valid_jobs);
    int cleaned_job_arr_idx = 0;
    for (size_t i = 0; i < num_strings; i++) {
        if (job_array[i].status == EXEC_READY) {
            cleaned_job_array[cleaned_job_arr_idx] = job_array[i];
            cleaned_job_arr_idx++;
        }
    }

    free(job_array);

    *num_valid_jobs = valid_jobs;

    return cleaned_job_array;
}