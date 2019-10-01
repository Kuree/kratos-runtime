#include "db.hh"
#include "fmt/format.h"

Database::Database(const std::string& filename) {
    // we assume the file already exists
    storage_ = std::make_unique<Storage>(kratos::init_storage(filename));
    storage_->sync_schema();
}

std::vector<uint32_t> Database::get_breakpoint_id(const std::string& filename, uint32_t line_num) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto bps = storage_->get_all<BreakPoint>(
            where(c(&BreakPoint::filename) == filename and c(&BreakPoint::line_num) == line_num));
        std::vector<uint32_t> result;
        for (auto const& bp : bps) {
            result.emplace_back(bp.id);
        }
        return result;
    } catch (...) {
        return {};
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

std::vector<kratos::Variable> Database::get_variable_mapping(uint32_t breakpoint_id) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto handles = storage_->select(columns(&BreakPoint::handle),
                                        where(c(&BreakPoint::id) == breakpoint_id));
        std::vector<kratos::Variable> result;
        for (auto const& h : handles) {
            auto const& handle = std::get<0>(h);
            auto values = storage_->get_all<Variable>(where(c(&Variable::handle) == handle));
            for (auto const& v : values) {
                result.emplace_back(v);
            }
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

std::vector<kratos::ContextVariable> Database::get_context_variable(uint32_t id) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto result = storage_->get_all<ContextVariable>(where(c(&ContextVariable::id) == id));
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<kratos::Hierarchy> Database::get_hierarchy(std::string handle_name) {
    using namespace sqlite_orm;
    using namespace kratos;
    if (handle_name.empty()) {
        // query the handle name
        try {
            auto results = storage_->get_all<MetaData>(where(c(&MetaData::name) == "top_name"));
            if (!results.empty()) {
                auto const result = results[0];
                handle_name = result.value;
            } else {
                return {};
            }

        } catch (...) {
            return {};
        }
    }
    // query the hierarchy
    try {
        auto result =
            storage_->get_all<Hierarchy>(where(c(&Hierarchy::parent_handle) == handle_name));
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<kratos::Connection> Database::get_connection(const std::string& handle_name) {
    using namespace sqlite_orm;
    using namespace kratos;
    try {
        auto result =
            storage_->get_all<Connection>(where(c(&Connection::handle_from) == handle_name));
        return result;
    } catch (...) {
        return {};
    }
}