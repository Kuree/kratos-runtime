#ifndef KRATOS_RUNTIME_CONTROL_HH
#define KRATOS_RUNTIME_CONTROL_HH

#include <cstdint>

void initialize_runtime();
void teardown_runtime();

extern "C" {
// this is the breakpoint insert by kratos for each statement
void breakpoint_trace(uint32_t id);
}

#endif  // KRATOS_RUNTIME_CONTROL_HH
