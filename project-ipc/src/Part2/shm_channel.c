#include <stdlib.h>
#include <stdio.h>

void error_and_die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
