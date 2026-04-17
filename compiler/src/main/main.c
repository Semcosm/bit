#include <stdio.h>
#include <string.h>

#include "bit/irgen.h"

static void bit_print_usage(const char *prog) {
    fprintf(stderr, "usage: %s <input.bit> -o <output.ll>\n", prog);
}

int main(int argc, char **argv) {
    const char *input_path = NULL;
    const char *output_path = NULL;

    if (argc < 4) {
        bit_print_usage(argv[0]);
        return 1;
    }

    input_path = argv[1];

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: missing value after -o\n");
                return 1;
            }
            output_path = argv[i + 1];
            ++i;
            continue;
        }

        fprintf(stderr, "error: unknown argument: %s\n", argv[i]);
        bit_print_usage(argv[0]);
        return 1;
    }

    if (!input_path || !output_path) {
        bit_print_usage(argv[0]);
        return 1;
    }

    printf("bitc: input = %s\n", input_path);
    printf("bitc: output = %s\n", output_path);
    printf("bitc: stage0 stub compiler active\n");

    return bit_emit_minimal_module(output_path);
}
