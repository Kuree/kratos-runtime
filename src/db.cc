#include "db.hh"
#include "fmt/format.h"


Database::Database(const std::string& filename) {
    // we assume the file already exists
    storage_ = std::make_unique<Storage>(kratos::init_storage(filename));
    storage_->sync_schema();
}

std::optional<uint32_t> Database::get_breakpoint_id(const std::string& filename,
                                                    uint32_t line_num) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto bps = storage_->get_all<BreakPoint>(
            where(c(&BreakPoint::filename) == filename and c(&BreakPoint::line_num) == line_num));
        if (bps.size() == 1) {
            return bps.front().id;
        } else {
            return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<uint32_t> Database::get_all_breakpoints(const std::string& filename) {
    using namespace sqlite_orm;
    using namespace kratos;
    std::vector<uint32_t> result;
    try {
        auto bps = storage_->get_all<BreakPoint>(where(c(&BreakPoint::filename) == filename));
        result.reserve(bps.size());
        for (auto const& bp : bps) {
            result.emplace_back(bp.id);
        }
    } catch (...) {
    }
    return result;
}

std::vector<std::string> Database::get_all_files() {
    using namespace sqlite_orm;
    // select distinct
    auto names = storage_->select(distinct(&kratos::BreakPoint::filename));
    std::vector<std::string> result;
    result.reserve(names.size());
    for (auto const& name : names) {
        result.emplace_back(name);
    }
    return names;
}

std::map<std::string, std::pair<std::string, std::string>> Database::get_variable_mapping(
    uint32_t breakpoint_id) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto values =
            storage_->select(columns(&Variable::handle, &Variable::front_var, &Variable::var),
                             where(c(&Variable::id) == breakpoint_id));
        std::map<std::string, std::pair<std::string, std::string>> result;
        for (auto const& [handle, front_var, var] : values) {
            result.emplace(front_var, std::make_pair(handle, var));
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::optional<std::pair<std::string, uint32_t>> Database::get_breakpoint_info(uint32_t id) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto bps = storage_->get_all<BreakPoint>(where(c(&BreakPoint::id) == id));
        if (!bps.empty()) {
            auto bp = bps[0];
            return std::make_pair(bp.filename, bp.line_num);
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::vector<std::string> Database::get_port_names(uint32_t breakpoint_id) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto handles =
            storage_->select(columns(&Variable::handle), where(c(&Variable::id) == breakpoint_id));
        if (!handles.empty()) {
            auto module_handle = std::get<0>(handles[0]);
            // find all the ports it has connections
            auto ports_from = storage_->select(columns(&Connection::var_from),
                                               where(c(&Connection::handle_from) == module_handle));
            auto ports_to = storage_->select(columns(&Connection::var_to),
                                             where(c(&Connection::handle_to) == module_handle));
            std::vector<std::string> result;
            result.reserve(ports_from.size() + ports_to.size());
            for (auto const &iter: ports_from) {
                auto port = std::get<0>(iter);
                result.emplace_back(fmt::format("{0}.{1}", module_handle, port));
            }
            for (auto const &iter: ports_from) {
                auto port = std::get<0>(iter);
                result.emplace_back(fmt::format("{0}.{1}", module_handle, port));
            }
            return result;
        }
    } catch (...) {
        return {};
    }
    return {};
}