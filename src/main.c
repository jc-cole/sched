#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>


typedef struct ParseResult {
    char *path_to_executable;
    char **argv;
    int argc;
} ParseResult;


void free_parse_result(ParseResult parse_result) {
    free(parse_result.path_to_executable);
    for (int i = 0; i < parse_result.argc; i++) {
        free(parse_result.argv[i]);
    }
    free(parse_result.argv);
}

// Policy:
// 1. Splits line on any stretch of tabs/spaces (not newlines)
// 2. Arguments wrapped in single/double quotes are not split
// 3. Escaped whitespaces/quotes with '\' are treated as any other character by the parser
ParseResult parse_line(char *line) {
    for (int i = 0; i < strlen(line); i++) {
        
    }
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

    int MAX_PROCESSES = 256;
    int MAX_LINE_SIZE = 256;
    char cmds_array[MAX_PROCESSES][MAX_LINE_SIZE];
    int cmds_count = 0;

    
    while (fgets(cmds_array[cmds_count], MAX_LINE_SIZE, workload_file)) {
        int command_str_len = strlen(cmds_array[cmds_count]);
        if (cmds_array[cmds_count][command_str_len - 1] == '\n') {
            cmds_array[cmds_count][command_str_len - 1] = '\0';
        }
        cmds_count++;
    }

    fclose(workload_file);

    return 0;
}