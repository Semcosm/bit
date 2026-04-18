#ifndef BIT_FILE_H
#define BIT_FILE_H

#include <stddef.h>

int bit_read_file(const char *path, char **buffer_out, size_t *length_out);

#endif
