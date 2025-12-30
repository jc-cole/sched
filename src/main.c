#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

    // attempt to open provided text file

    FILE *workload_file = fopen(workload_filename, "r");
    if (workload_file == NULL) {
        printf("Unable to open file \"%s\"\n", workload_filename);
        exit(1);
    }

    fseek(workload_file, 0, SEEK_END);
    long file_size = ftell(workload_file);

    char file_contents[file_size];
    if (fgets(file_contents, file_size, workload_file) == NULL) {
        printf("Error reading provided text file. \n");
        exit(1);
    }

    char *cmds_array[256];
    int cmds_count = 0;

    // split on newlines
    char *delim = "\n";
    
    char *token = strtok(file_contents, delim);

    while (token != NULL) {
        cmds_array[cmds_count] = token;
        cmds_count++;
        token = strtok(NULL, delim);
    }

    for (int i = 0; i < cmds_count; i++) {
        printf("%s", cmds_array[i]);
    }



    return 0;
}