#include <cstdio>
#include "Vmod.h"
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
    Vmod *tb = new Vmod();
    // Verilated::internalsDump();  // See scopes to help debug
    // Create an instance of our module under test
    initialize_runtime();

    // reset
    tb->rst = 0;
    tb->eval();
    tb->rst = 1;
    tb->eval();
    tb->rst = 0;
    tb->eval();

    for (int i = 0; i < 4; i++) {
        tb->a = i + 1;
        tb->clk = 1;
        main_time++;
        tb->eval();
        VerilatedVpi::callValueCbs(); // required to call callbacks

        tb->clk = 0;
        tb->eval();
    }
    tb->final();

    delete tb;
    teardown_runtime();

    exit(EXIT_SUCCESS);
}
