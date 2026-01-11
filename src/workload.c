#include "job.h"
#include <string.h>
#include <stdlib.h>

Job make_empty_job() {
    Job job;
    job.argv = NULL;
    job.argc = 0;
    job.pid = -1;
    job.status = EMPTY;
    job.exit_code = -1;
    job.terminating_signal = -1;
    return job;
}

void append_to_argv(Job *job, int *token_char_count, int *argv_size, char *token_buffer) {
    job->argv[job->argc] = malloc(sizeof(char) * *token_char_count);
    if (job->argc + 1 == *argv_size) {
        *argv_size *= 2;
        job->argv = realloc(job->argv, sizeof(char*) * *argv_size);
    }
    token_buffer[*token_char_count] = '\0';
    strcpy(job->argv[job->argc], token_buffer);
    job->argc++;
}


Job parse_line(char *cmd_string) {
    Job job = make_empty_job();

    int argv_size = 8;
    job.argv = malloc(sizeof(char*) * argv_size);
    
    enum state { SKIP_WS, IN_TOKEN, IN_S_QUOTE, IN_D_QUOTE };
    enum state parser_state = SKIP_WS;
    char token_buffer[strlen(cmd_string) + 1];
    int token_char_count = 0;
    for (int char_idx = 0; char_idx < strlen(cmd_string) + 1; char_idx++) {

        char current_char = cmd_string[char_idx];

        if (current_char == ' ' || current_char == '\t' || current_char == '\n') {

            if (parser_state == IN_TOKEN) {
                append_to_argv(&job, &token_char_count, &argv_size, &token_buffer);
                token_char_count = 0;
                parser_state = SKIP_WS;
            } else if (parser_state == IN_D_QUOTE || parser_state == IN_S_QUOTE) {
                token_buffer[token_char_count] = current_char;
                token_char_count++;
            }

        } else if (current_char == '\"') {

            if (parser_state == IN_D_QUOTE) {
                append_to_argv(&job, &token_char_count, &argv_size, &token_buffer);
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
                append_to_argv(&job, &token_char_count, &argv_size, &token_buffer);
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
                append_to_argv(&job, &token_char_count, &argv_size, &token_buffer);
                break;
            } else if (parser_state == SKIP_WS) {
                break;
            } 
        }
        else if (current_char == '\0') {
            if (parser_state == IN_TOKEN) {
                append_to_argv(&job, &token_char_count, &argv_size, &token_buffer);
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

void print_jobs_debug_temp(Job *jobs, int num_jobs) {
    printf("Current job statuses:\n");
    for (int i = 0; i < num_jobs; i++) {
        printf("index: %d\n", i);
        printf("    argc: %d\n", jobs[i].argc);
        if (jobs[i].argv != NULL) {
            printf("    argv:\n");
            for (int arg = 0; arg < jobs[i].argc; arg++) {
                printf("        %d. %s\n", arg, jobs[i].argv[arg]);
            }
        } else {
            printf("    argv: NULL\n");
        }
        printf("    pid: %d\n", jobs[i].pid);
        printf("    status: %s\n", job_status_str[jobs[i].status]);
        printf("    exit code: %d\n", jobs[i].exit_code);
        printf("    terminating signal: %d\n", jobs[i].terminating_signal);
        printf("\n");
    }
}


Job *parse_lines(char *cmd_strings[], int num_strings, int *num_valid_jobs) {
    Job *job_array = malloc(sizeof(Job) * num_strings);
    for (int i = 0; i < num_strings; i++) {
        job_array[i] = parse_line(cmd_strings[i]);
    }

    print_jobs_debug_temp(job_array, num_strings);

    // clearing jobs that can't be exec'd due to being empty or syntax error

    int valid_jobs = 0;
    for (int i = 0; i < num_strings; i++) {
        if (job_array[i].status == EXEC_READY) {
            valid_jobs++;
        }
    }

    Job *cleaned_job_array = malloc(sizeof(Job) * valid_jobs);
    int cleaned_job_arr_idx = 0;
    for (int i = 0; i < num_strings; i++) {
        if (job_array[i].status == EXEC_READY) {
            cleaned_job_array[cleaned_job_arr_idx] = job_array[i];
            cleaned_job_arr_idx++;
        }
    }

    print_jobs_debug_temp(cleaned_job_array, valid_jobs);

    *num_valid_jobs = valid_jobs;

    print_jobs_debug_temp(cleaned_job_array, *num_valid_jobs);

    free(job_array);

    return cleaned_job_array;

}