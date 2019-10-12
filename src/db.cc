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

std::vector<Variable> Database::get_variable_mapping(uint32_t breakpoint_id) {
    using namespace sqlite_orm;
    try {
        std::vector<Variable> result;
        // SELECT variable.name, variable.value, variable.is_var, instance.handle_name
        //     FROM variable, instance, breakpoint WHERE
        //     breakpoint.id = breakpoint_id AND breakpoint.handle = instance.id AND
        //     variable.handle = instance.id and variable.is_context = false
        auto values = storage_->select(
            columns(&kratos::Variable::name, &kratos::Variable::value, &kratos::Variable::is_var,
                    &kratos::Instance::handle_name, &kratos::BreakPoint::id),
            where(is_equal(&kratos::BreakPoint::id, breakpoint_id) and
                  is_equal(&kratos::BreakPoint::handle, &kratos::Variable::handle) and
                  is_equal(&kratos::Instance::id,
                           &kratos::BreakPoint::handle and
                               is_equal(&kratos::Variable::is_context, false))));
        for (auto const& v : values) {
            auto const& [name, value, is_var, handle_name, a] = v;
            (void)(a);
            Variable var{name, value, handle_name, false, is_var};
            result.emplace_back(var);
        }

        return result;
    } catch (...) {
        return {};
    }
}

std::optional<std::pair<std::string, uint32_t>> Database::get_breakpoint_info(uint32_t id) {
    using namespace sqlite_orm;
    try {
        auto bps = storage_->get_all<kratos::BreakPoint>(where(c(&kratos::BreakPoint::id) == id));
        if (!bps.empty()) {
            auto const& bp = bps[0];
            return std::make_pair(bp.filename, bp.line_num);
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::vector<Variable> Database::get_context_variable(uint32_t id) {
    using namespace sqlite_orm;
    try {
        std::vector<Variable> result;
        auto values = storage_->select(
            columns(&kratos::ContextVariable::name, &kratos::Variable::value,
                    &kratos::Variable::is_var, &kratos::Instance::handle_name),
            where(c(&kratos::ContextVariable::breakpoint_id) == id and
                  c(&kratos::ContextVariable::variable_id) == &kratos::Variable::id and
                  c(&kratos::Instance::id) == &kratos::Variable::handle));
        for (auto const& v : values) {
            auto const& [name, value, is_var, handle_name] = v;
            Variable var{name, value, handle_name, true, is_var};
            result.emplace_back(var);
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<Hierarchy> Database::get_hierarchy(std::string handle_name) {
    using namespace sqlite_orm;
    if (handle_name.empty()) {
        // query the handle name
        try {
            auto results = storage_->get_all<kratos::MetaData>(
                where(c(&kratos::MetaData::name) == "top_name"));
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
        std::vector<Hierarchy> result;
        auto values =
            storage_->select(columns(&kratos::Instance::handle_name, &kratos::Hierarchy::child),
                             where(c(&kratos::Instance::handle_name) == handle_name and
                                   c(&kratos::Hierarchy::parent_handle) == &kratos::Instance::id));
        for (auto const& [name, child] : values) {
            Hierarchy h{name, child};
            result.emplace_back(h);
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<Connection> Database::get_connection_to(const std::string& handle_name) {
    using namespace sqlite_orm;
    try {
        // a is from
        using als_a = alias_a<kratos::Instance>;
        using als_b = alias_b<kratos::Instance>;
        // SELECT a.handle_name, c.var_from, b.handle_name, c.var_to FROM
        //     instance a, instance b, connection c WHERE
        //     b.id = c.handle_to AND b.handle_name = handle_name AND a.id = c.handle_from
        auto values = storage_->select(
            columns(
                alias_column<als_a>(&kratos::Instance::handle_name), &kratos::Connection::var_from,
                alias_column<als_b>(&kratos::Instance::handle_name), &kratos::Connection::var_to),
            where(is_equal(alias_column<als_b>(&kratos::Instance::id),
                           &kratos::Connection::handle_to) and
                  is_equal(alias_column<als_b>(&kratos::Instance::handle_name), handle_name) and
                  is_equal(alias_column<als_a>(&kratos::Instance::id),
                           &kratos::Connection::handle_from)));
        std::vector<Connection> result;
        for (auto const& [handle_from, var_from, handle_to, var_to] : values) {
            Connection conn{handle_from, var_from, handle_to, var_to};
            result.emplace_back(conn);
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<Connection> Database::get_connection_from(const std::string& handle_name) {
    using namespace sqlite_orm;
    try {
        // a is from
        using als_a = alias_a<kratos::Instance>;
        using als_b = alias_b<kratos::Instance>;
        // SELECT a.handle_name, c.var_from, b.handle_name, c.var_to FROM
        //     instance a, instance b, connection c WHERE
        //     a.id = c.handle_from AND a.handle_name = handle_name AND b.id = c.handle_to
        auto values = storage_->select(
            columns(
                alias_column<als_a>(&kratos::Instance::handle_name), &kratos::Connection::var_from,
                alias_column<als_b>(&kratos::Instance::handle_name), &kratos::Connection::var_to),
            where(is_equal(alias_column<als_a>(&kratos::Instance::id),
                           &kratos::Connection::handle_from) and
                  is_equal(alias_column<als_a>(&kratos::Instance::handle_name), handle_name) and
                  is_equal(alias_column<als_b>(&kratos::Instance::id),
                           &kratos::Connection::handle_to)));
        std::vector<Connection> result;
        for (auto const& [handle_from, var_from, handle_to, var_to] : values) {
            Connection conn{handle_from, var_from, handle_to, var_to};
            result.emplace_back(conn);
        }
        return result;
    } catch (...) {
        return {};
    }
}