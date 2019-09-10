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
	// Create an instance of our module under test
    initialize_runtime();
	Vtest *tb = new Vtest();

    tb->a = 0;
    tb->eval();
    main_time++;
    tb->a = 1;
    tb->eval();
    tb->final();

    delete tb;
    teardown_runtime();

	exit(EXIT_SUCCESS);
}
