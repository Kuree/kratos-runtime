#ifndef KRATOS_RUNTIME_DB_HH
#define KRATOS_RUNTIME_DB_HH

#include <memory>
#include "kratos/src/debug.hh"
#include "sqlite_orm/sqlite_orm.h"

class Database {
public:
    explicit Database(const std::string &filename);

    std::optional<uint32_t> get_breakpoint_id(const std::string &filename, uint32_t line_num);
    std::map<std::string, std::pair<std::string, std::string>> get_variable_mapping(
        uint32_t breakpoint_id);
    std::vector<uint32_t> get_all_breakpoints(const std::string &filename);
    std::vector<std::string> get_all_files();

private:
    using Storage = decltype(kratos::init_storage(""));
    std::unique_ptr<Storage> storage_;
};

#endif  // KRATOS_RUNTIME_DB_HH
