#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

#define MAX_PROCESSES 256
#define MAX_LINE_SIZE 256
#define MAX_COMMAND_LINE_ARGS 63


typedef struct ProcessNode {
    char *path_to_executable;
    char **argv;
    int argc;
    pid_t pid;
    ProcessNode *next;
} ProcessNode;


void free_parse_result(ProcessNode parse_result) {
    free(parse_result.argv);
}

// Policy: Splits line on any stretch of tabs/spaces (not newlines)
// Note: Mutates passed string
ProcessNode parse_line(char *line) {

    ProcessNode result;
    result.path_to_executable = NULL;
    result.argc = 1;
    result.argv = (char *) malloc(sizeof(char *) * (MAX_COMMAND_LINE_ARGS + 1));
    for (int i = 0; i < MAX_COMMAND_LINE_ARGS + 1; i++) {
        result.argv[i] = NULL;
    }

    int line_length = strlen(line);

    bool on_leading_spaces = true;

    // first extract path to executable
    int i = 0;
    for (; i < line_length; i++) {
        if ((line[i] == ' ' || line[i] == '\t') && on_leading_spaces) {
            continue;
        } else if ((line[i] == ' ' || line[i] == '\t') && !on_leading_spaces) {
            line[i] = '\0'; // reached end of path to executable but still more args
            i++;
            result.argv[0] = result.path_to_executable;
            break;
        } else if (line[i] == '\0') {
            result.argv[0] = result.path_to_executable;
            result.argv[1] = NULL;
            return result;
        } else if (result.path_to_executable == NULL) {
            on_leading_spaces = false;
            result.path_to_executable = &line[i];
        }
    }

    // now iteratively extract argument strings
    on_leading_spaces = true;
    for (; i < line_length; i++) {
        if ((line[i] == ' ' || line[i] == '\t') && on_leading_spaces) {
            continue;
        } else if ((line[i] == ' ' || line[i] == '\t') && !on_leading_spaces) {
            line[i] = '\0'; // reached end of path to executable but still more args
        } else if (line[i] == '\0') {
            return result;
        } else if (result.argv[result.argc] == NULL) {
            on_leading_spaces = false;
            result.argv[result.argc] = &line[i];
            result.argc++;
        }
    }

    return result;
}


int main(int argc, char *argv[]) {
    char* scheduler_policy = NULL;
    int quantum = 100;
    char* workload_filename = NULL;
    if (argc > 0) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--policy") == 0) {
                scheduler_policy = argv[++i];
            }
            else if (strcmp(argv[i], "--quantum") == 0) {
                quantum = atoi(argv[++i]);
            } else {
                workload_filename = argv[i];
            }
        }
    } else {
        printf("usage: ./sched --policy <policy> --quantum <ms> <workload.txt>\n");
        exit(1);
    }

    FILE *workload_file = fopen(workload_filename, "r");
    if (workload_file == NULL) {
        printf("Unable to open file \"%s\"\n", workload_filename);
        exit(1);
    }

    char cmds_array[MAX_PROCESSES][MAX_LINE_SIZE];
    int cmds_count = 0;

    
    while (fgets(cmds_array[cmds_count], MAX_LINE_SIZE, workload_file)) {
        int command_str_len = strlen(cmds_array[cmds_count]);
        if (cmds_array[cmds_count][command_str_len - 1] == '\n') {
            cmds_array[cmds_count][command_str_len - 1] = '\0';
        }
        cmds_count++;
    }

    ProcessNode processes[cmds_count];
    for (int i = 0; i < cmds_count; i++) {
        processes[i] = parse_line(cmds_array[i]);
    }


    
    return 0;
}