#include <stdio.h>
#include <unistd.h>

int main() {
    int current_num = 1;
    while (1) {
        printf("%d\n", current_num);
        sleep(1);
        current_num += 2;
    }
    return 0;
}