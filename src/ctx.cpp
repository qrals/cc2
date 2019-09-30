#include <string>

#include "val.hpp"
#include "ctx.hpp"
#include "gen.hpp"

void t_ctx::enter_switch() {
    cases.clear();
    case_vals.clear();
    case_idx = 0;
    _default_label = "";
}

void t_ctx::def_case(const t_val& v, const str& l) {
    if (case_vals.count(v.u_val()) == 0) {
        case_vals.insert(v.u_val());
        cases.push_back({as(v), l});
    } else {
        throw t_redefinition_error();
    }
}

str t_ctx::get_case_label() {
    auto& res = cases[case_idx].label;
    case_idx++;
    return res;
}

t_asm_val t_ctx::as(const t_val& val) const {
    _ type = val.type().as();
    if (val.is_lvalue()) {
        type += "*";
    }
    return t_asm_val{type, val.as()};
}

str t_ctx::as(const str& ss) const {
    return "@" + ss;
}

t_type t_ctx::complete_type(const t_type& t) const {
    if (t.is_function()) {
        vec<t_type> params;
        for (_& p : t.params()) {
            params.push_back(complete_type(p));
        }
        _ rt = complete_type(t.return_type());
        return make_func_type(rt, std::move(params), t.is_variadic());
    } else if (t.is_array()) {
        _ et = complete_type(t.element_type());
        return make_array_type(et, t.length());
    } else if (t.is_struct() and t.fields().empty()) {
        return get_type_data(t.name()).type;
    } else if (t.is_struct()) {
        vec<t_type> field_types;
        for (size_t i = 0; i < t.length(); i++) {
            field_types.push_back(complete_type(t.fields()[i]));
        }
        return make_struct_type(t.name(), t.field_names(),
                                std::move(field_types), t.as());
    } else if (t.is_pointer()) {
        return make_pointer_type(complete_type(t.pointee_type()));
    }
    return t;
}

void t_ctx::enter_scope() {
    types.enter_scope();
    vars.enter_scope();
}

t_ctx::~t_ctx() {
    for (_& [name, type_data] : types.scope_get()) {
        _& t = type_data.type;
        if (t.is_struct() and t.is_incomplete()) {
            prog.def_opaque_struct(t.as());
        }
    }
}
