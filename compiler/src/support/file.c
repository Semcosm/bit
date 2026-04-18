#include "bit/file.h"

#include <stdio.h>
#include <stdlib.h>

int bit_read_file(const char *path, char **buffer_out, size_t *length_out) {
    FILE *file;
    char *buffer;
    long file_size;
    size_t bytes_read;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "error: failed to open '%s'\n", path);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: failed to seek '%s'\n", path);
        fclose(file);
        return 1;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "error: failed to measure '%s'\n", path);
        fclose(file);
        return 1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: failed to rewind '%s'\n", path);
        fclose(file);
        return 1;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer) {
        fprintf(stderr, "error: out of memory while reading '%s'\n", path);
        fclose(file);
        return 1;
    }

    bytes_read = fread(buffer, 1, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "error: failed to read '%s'\n", path);
        free(buffer);
        fclose(file);
        return 1;
    }

    buffer[bytes_read] = '\0';
    fclose(file);

    *buffer_out = buffer;
    *length_out = bytes_read;
    return 0;
}
