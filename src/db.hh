#ifndef KRATOS_RUNTIME_DB_HH
#define KRATOS_RUNTIME_DB_HH

#include <any>
#include <memory>
#include "kratos/src/db.hh"
#include "sqlite_orm/sqlite_orm.h"

// wrapper structs
struct Variable {
    std::string name;
    std::string value;
    std::string handle;
    bool is_context;
    bool is_var;
};

struct Hierarchy {
    std::string parent_handle;
    std::string child;
};

struct Connection {
    std::string handle_from;
    std::string var_from;
    std::string handle_to;
    std::string var_to;
};

struct Breakpoint {
    int instance_id;
    int breakpoint_id;
};

class Database {
public:
    explicit Database(const std::string &filename);

    std::vector<uint32_t> get_breakpoint_id(const std::string &filename, uint32_t line_num);
    std::vector<Breakpoint> get_breakpoints(const std::string &filename, uint32_t line_num);
    std::vector<Variable> get_variable_mapping(uint32_t instance_id, uint32_t breakpoint_id);
    std::vector<uint32_t> get_all_breakpoints(const std::string &filename);
    std::vector<std::string> get_all_files();
    std::optional<std::pair<std::string, uint32_t>> get_breakpoint_info(uint32_t id);
    std::vector<Variable> get_context_variable(uint32_t instance_id, uint32_t id);
    std::vector<Hierarchy> get_hierarchy(std::string handle_name);
    std::vector<Connection> get_connection_to(const std::string &handle_name);
    std::vector<Connection> get_connection_from(const std::string &handle_name);
    std::optional<uint32_t> get_instance_id(uint32_t breakpoint_id);
    std::string get_instance_name(uint32_t instance_id);

private:
    // see https://github.com/fnc12/sqlite_orm/wiki/FAQ
    using Storage = decltype(kratos::init_storage(""));
    std::unique_ptr<Storage> storage_;
};

#endif  // KRATOS_RUNTIME_DB_HH
