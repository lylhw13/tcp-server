#include "generic.h"

void error(const char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t bytes)
{
    void *ptr;
    ptr = malloc(bytes);
    if (ptr == NULL) {
        perror("xmalloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}