#include <cstdio>
#include "Vtest.h"
#include "verilated.h"
#include "verilated_vpi.h"  // Required to get definitions

// this is only necessary for verilator
void initialize_runtime();
void teardown_runtime();

vluint64_t main_time = 0;       // Current simulation time

double sc_time_stamp () {       // Called by $time in Verilog
    return main_time;           // converts to double, to match
                                // what SystemC does
}

int main(int argc, char **argv) {
    Vtest *tb = new Vtest();
    // Verilated::internalsDump();  // See scopes to help debug
    // Create an instance of our module under test
    initialize_runtime();

    for (int i = 0; i < 4; i++) {
        tb->in1 = i;
        tb->in2 = i;
        main_time++;
        tb->eval();
        VerilatedVpi::callValueCbs(); // required to call callbacks
    }
    tb->final();

    delete tb;
    teardown_runtime();

    exit(EXIT_SUCCESS);
}
