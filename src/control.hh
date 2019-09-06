#ifndef KRATOS_RUNTIME_CONTROL_HH
#define KRATOS_RUNTIME_CONTROL_HH

#include <cstdint>

extern "C" {
// this is the breakpoint insert by kratos for each statement
void breakpoint_trace(uint32_t id);

// this is the dpi function that will be called when the simulation starts
void initialize_runtime();

}

#endif  // KRATOS_RUNTIME_CONTROL_HH
