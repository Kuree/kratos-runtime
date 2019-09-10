#ifndef KRATOS_RUNTIME_SIM_HH
#define KRATOS_RUNTIME_SIM_HH

#include <cinttypes>

void add_break_point(uint32_t id);
void remove_break_point(uint32_t id);
bool should_continue_simulation(uint32_t id);
bool continue_simulation();

#endif  // KRATOS_RUNTIME_SIM_HH
