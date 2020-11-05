#include "sim.hh"
#include "util.hh"
#include <unordered_set>

std::unordered_set<uint32_t> break_points;

void add_break_point(uint32_t id) {
    printf("Breakpoint inserted to %d\n", id);
    break_points.emplace(id);
}
void remove_break_point(uint32_t id) {
    if (break_points.find(id) != break_points.end()) {
        printf("Breakpoint removed from %d\n", id);
        break_points.erase(id);
    }
}
bool should_continue_simulation(uint32_t id) {
    return break_points.find(id) == break_points.end();
}