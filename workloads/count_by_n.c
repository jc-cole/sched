#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int count_by = atoi(argv[1]);
    int sleep_for = atoi(argv[2]);
    int current_num = 0;
    while (1) {
        printf("%d\n", current_num);
        sleep(sleep_for);
        current_num += count_by;
    }
    return 0;
}