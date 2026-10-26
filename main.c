#include <stdio.h>

#include "ntypes.h"
#include "vgm.h"

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "Missing file path argument\n");
        return 1;
    }
    if (read_vgm(argv[1]) != 0) {
        return 1;
    }
    return 0;
}
