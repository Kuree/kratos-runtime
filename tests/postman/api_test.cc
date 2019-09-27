#include <iostream>
#include <unistd.h>
#include "../../src/control.hh"
#include "../../src/std/vpi_user.h"

#include "../vpi_impl.hh"

int main(int, char **) {
    // initialize the server
    initialize_runtime();
    while (1) {
        usleep(1000000);
        breakpoint_trace(1);
        breakpoint_trace(3);
    }
    return EXIT_SUCCESS;
}
