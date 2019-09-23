#include "db.hh"
#include "kratos/src/debug.hh"

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
        auto bps = storage_->get_all<BreakPoint>(
            where(c(&BreakPoint::filename) == filename));
       result.reserve(bps.size());
       for (auto const &bp: bps) {
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
    for (auto const &name: names) {
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
        for (auto const &[handle, front_var, var]: values) {
            result.emplace(front_var, std::make_pair(handle, var));
        }
        return result;
    } catch (...) {
        return {};
    }
}