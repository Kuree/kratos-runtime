#include <iostream>
#include <unistd.h>
#include "../../src/control.hh"
#include "../../src/std/vpi_user.h"

// provide dummy implementation
vpiHandle vpi_register_cb(p_cb_data) { return nullptr; }

void vpi_get_value(vpiHandle, p_vpi_value) {}
PLI_INT32 vpi_remove_cb(vpiHandle) { return 0; }
PLI_INT32 vpi_free_object(vpiHandle) { return 0; }
vpiHandle vpi_handle_by_name(PLI_BYTE8 *, vpiHandle) { return nullptr; }

int main(int, char **) {
    // initialize the server
    initialize_runtime();
    while (1) {
        usleep(1000000);
        breakpoint_trace(0);
        breakpoint_trace(1);
    }
    return EXIT_SUCCESS;
}