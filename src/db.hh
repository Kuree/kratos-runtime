#ifndef KRATOS_RUNTIME_DB_HH
#define KRATOS_RUNTIME_DB_HH

#include <memory>
#include <any>
#include "kratos/src/db.hh"
#include "sqlite_orm/sqlite_orm.h"

class Database {
public:
    explicit Database(const std::string &filename);

    std::vector<uint32_t> get_breakpoint_id(const std::string &filename, uint32_t line_num);
    std::vector<kratos::Variable> get_variable_mapping(
        uint32_t breakpoint_id);
    std::vector<uint32_t> get_all_breakpoints(const std::string &filename);
    std::vector<std::string> get_all_files();
    std::optional<std::pair<std::string, uint32_t>> get_breakpoint_info(uint32_t id);
    std::vector<kratos::ContextVariable> get_context_variable(uint32_t id);
    std::vector<kratos::Hierarchy> get_hierarchy(std::string handle_name);
    std::vector<kratos::Connection> get_connection_to(const std::string &handle_name);
    std::vector<kratos::Connection> get_connection_from(const std::string &handle_name);

private:
    // see https://github.com/fnc12/sqlite_orm/wiki/FAQ
    using Storage = decltype(kratos::init_storage(""));
    std::unique_ptr<Storage> storage_;
};

#endif  // KRATOS_RUNTIME_DB_HH
