#include <iostream>

#include "api/NYC311Service.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    NYC311Service service;
    service.build_indices(); // safe on empty dataset; forces compilation of index/query code.
    std::cout << "mini1 skeleton: Phase 1 indices/query stubs compiled" << std::endl;
    return 0;
}
