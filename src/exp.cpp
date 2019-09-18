#include <string>
#include <cassert>

#include "asm_gen.hpp"

using namespace std;

bool is_modifiable_lvalue(const t_val& x) {
    return true;
}

t_val make_constant(ull x) {
    t_val res;
    res.value = std::to_string(x);
    res.type = u_long_type;
    return res;
}

t_val make_constant(int x) {
    t_val res;
    res.value = std::to_string(x);
    res.type = int_type;
    return res;
}

t_val apply(string op, const t_val& x, const t_val& y, const t_ctx& ctx) {
    t_val res;
    res.type = x.type;
    res.value = prog.apply(op, ctx.get_asm_val(x), ctx.get_asm_val(y));
    return res;
}

t_type make_type(const t_ast& ast, const t_ctx& ctx) {
    auto res = make_base_type(ast[0], ctx);
    if (ast.children.size() == 2) {
        unpack_declarator(res, ast[1]);
    }
    return res;
}

t_val gen_array_elt(const t_val& v, int i, t_ctx& ctx) {
    t_val res;
    res.value = prog.member(ctx.get_asm_val(v), i);
    res.type = v.type.get_element_type();
    res.is_lvalue = true;
    return res;
}

t_val convert_lvalue_to_rvalue(const t_val& v, t_ctx& ctx) {
    t_val res;
    res.type = v.type;
    res.value = prog.load(ctx.get_asm_val(v));
    res.is_lvalue = false;
    return res;
}

t_val gen_neg(const t_val& x, const t_ctx& ctx) {
    return gen_sub({"0", int_type}, x, ctx);
}

t_val gen_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx) {
    prog.store(ctx.get_asm_val(rhs), ctx.get_asm_val(lhs));
    return lhs;
}

t_val gen_convert_assign(const t_val& lhs, const t_val& rhs, t_ctx& ctx) {
    return gen_assign(lhs, gen_conversion(lhs.type, rhs, ctx), ctx);
}

auto gen_eq_ptr_conversions(t_val& x, t_val& y,
                            const t_ctx& ctx) {
    if (is_pointer_type(x.type) and is_pointer_type(y.type)) {
        if (is_qualified_void(y.type.get_pointee_type())) {
            x = gen_conversion(y.type, x, ctx);
        } else if (is_qualified_void(x.type.get_pointee_type())) {
            y = gen_conversion(x.type, y, ctx);
        }
    } else if (is_pointer_type(x.type) and is_integral_type(y.type)) {
        y = gen_conversion(x.type, y, ctx);
    } else if (is_pointer_type(y.type) and is_integral_type(x.type)) {
        x = gen_conversion(y.type, x, ctx);
    }
}

t_val gen_is_zero(const t_val& x, const t_ctx& ctx) {
    return gen_eq(x, {"0", int_type}, ctx);
}

t_val gen_is_nonzero(const t_val& x, const t_ctx& ctx) {
    auto y = gen_eq(x, {"0", int_type}, ctx);
    auto res = t_val{prog.make_new_id(), bool_type};
    let(res, fun("xor i1", y.value, "1"));
    return res;
}

void gen_int_promotion(t_val& x, const t_ctx& ctx) {
    if (x.type == char_type or x.type == s_char_type
        or x.type == u_char_type or x.type == short_type
        or x.type == u_short_type) {
        x = gen_conversion(int_type, x, ctx);
    }
}

t_val dereference(const t_val& v) {
    return t_val{v.value, v.type.get_pointee_type(), true};
}

t_val gen_struct_member(const t_val& v, int i, t_ctx& ctx) {
    t_val res;
    res.type = v.type.get_member_type(i);
    res.value = prog.member(ctx.get_asm_val(v), i);
    res.is_lvalue = true;
    return res;
}

void gen_arithmetic_conversions(t_val& x, t_val& y,
                                const t_ctx& ctx) {
    auto promote = [&](auto& t) {
        if (y.type == t) {
            x = gen_conversion(t, x, ctx);
            return;
        }
        if (x.type == t) {
            y = gen_conversion(t, y, ctx);
            return;
        }
    };
    promote(long_double_type);
    promote(double_type);
    promote(float_type);
    gen_int_promotion(x, ctx);
    gen_int_promotion(y, ctx);
    promote(u_long_type);
    promote(long_type);
    promote(u_int_type);
}

auto gen_and(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
        throw t_bad_operands();
    }
    gen_arithmetic_conversions(x, y, ctx);
    return apply("and", x, y, ctx);
}

auto gen_xor(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
        throw t_bad_operands();
    }
    gen_arithmetic_conversions(x, y, ctx);
    return apply("xor", x, y, ctx);
}

auto gen_or(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_integral_type(x.type)
             and is_integral_type(y.type))) {
        throw t_bad_operands();
    }
    gen_arithmetic_conversions(x, y, ctx);
    t_val res;
    res.type = x.type;
    res.value = prog.apply("or", ctx.get_asm_val(x), ctx.get_asm_val(y));
    return res;
}

t_val gen_eq(t_val x, t_val y, const t_ctx& ctx) {
    auto res = t_val{prog.make_new_id(), bool_type};
    if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
        gen_arithmetic_conversions(x, y, ctx);
        string op;
        if (is_floating_type(x.type)) {
            op = "fcmp oeq";
        } else {
            op = "icmp eq";
        }
        let(res, fun(op + " " + ctx.get_asm_type(x.type), x, y));
    } else if (is_pointer_type(x.type) or is_pointer_type(y.type)) {
        gen_eq_ptr_conversions(x, y, ctx);
        let(res, fun("icmp eq " + ctx.get_asm_type(x.type), x, y));
    } else {
        throw t_bad_operands("==");
    }
    return res;
}

t_val gen_conversion(const t_type& t, const t_val& v,
                           const t_ctx& ctx) {
    t_val res;
    if (t == int_type and v.type == bool_type) {
        res.type = t;
        res.value = prog.convert("zext",
                                 ctx.get_asm_val(v),
                                 ctx.get_asm_type(t));
        return res;
    }
    if (t == void_type) {
        res.type = t;
        return res;
    }
    if (not is_scalar_type(t) or not is_scalar_type(v.type)) {
        throw t_conversion_error(v.type, t);
    }
    if (not compatible(t, v.type)) {
        res.type = t;
        auto x = ctx.get_asm_val(v);
        auto w = ctx.get_asm_type(t);
        string op;
        if (is_integral_type(t) and is_integral_type(v.type)) {
            if (t.get_size() < v.type.get_size()) {
                op = "trunc";
            } else if (t.get_size() > v.type.get_size()) {
                if (is_signed_integer_type(v.type)) {
                    op = "sext";
                } else {
                    op = "zext";
                }
            } else {
                return v;
            }
        } else if (is_pointer_type(t) and is_integral_type(v.type)) {
            op = "inttoptr";
        } else if (is_pointer_type(t) and is_pointer_type(v.type)) {
            op = "bitcast";
        } else if (is_integral_type(t) and is_pointer_type(v.type)) {
            op = "ptrtoint";
        } else if (is_floating_type(t) and is_floating_type(v.type)) {
            if (t.get_size() < v.type.get_size()) {
                op = "fptrunc";
            } else if (t.get_size() > v.type.get_size()) {
                op = "fpext";
            } else {
                return v;
            }
        } else if (is_floating_type(t) and is_integral_type(v.type)) {
            if (is_signed_integer_type(v.type)) {
                op = "sitofp";
            } else {
                op = "uitofp";
            }
        } else if (is_integral_type(t) and is_floating_type(v.type)) {
            if (is_signed_integer_type(v.type)) {
                op = "fptosi";
            } else {
                op = "fptoui";
            }
        } else {
            throw t_conversion_error(v.type, t);
        }
        res.value = prog.convert(op, x, w);
    } else {
        res = v;
    }
    return res;
}

t_val gen_sub(t_val x, t_val y, const t_ctx& ctx) {
    t_val res;
    if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
        gen_arithmetic_conversions(x, y, ctx);
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "sub nsw";
        } else if (is_floating_type(x.type)) {
            op = "fsub";
        } else {
            op = "sub";
        }
        res = apply(op, x, y, ctx);
    } else if (is_pointer_type(x.type) and is_pointer_type(y.type)
               and compatible(unqualify(x.type.get_pointee_type()),
                              unqualify(y.type.get_pointee_type()))) {
        auto& w = x.type.get_pointee_type();
        x = gen_conversion(uintptr_t_type, x, ctx);
        y = gen_conversion(uintptr_t_type, y, ctx);
        auto v = apply("sub", x, y, ctx);
        v.type = ptrdiff_t_type;
        res = apply("sdiv exact", v, make_constant(w.get_size()), ctx);
    } else if (is_pointer_type(x.type)
               and is_object_type(x.type.get_pointee_type())
               and is_integral_type(y.type)) {
        y = gen_conversion(uintptr_t_type, y, ctx);
        y = gen_neg(y, ctx);
        res.type = x.type;
        res.value = prog.inc_ptr(ctx.get_asm_val(x), ctx.get_asm_val(y));
    } else {
        throw t_bad_operands("binary -");
    }
    return res;
}

t_val gen_mul(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_arithmetic_type(x.type) and is_arithmetic_type(y.type))) {
        throw t_bad_operands();
    }
    gen_arithmetic_conversions(x, y, ctx);
    string op;
    if (is_signed_integer_type(x.type)) {
        op = "mul nsw";
    } else if (is_floating_type(x.type)) {
        op = "fmul";
    } else {
        op = "mul";
    }
    return apply(op, x, y, ctx);
}

t_val gen_mod(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
        throw t_bad_operands();
    }
    gen_arithmetic_conversions(x, y, ctx);
    string op;
    if (is_signed_integer_type(x.type)) {
        op = "srem";
    } else {
        op = "urem";
    }
    return apply(op, x, y, ctx);
}

t_val gen_shr(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
        throw t_bad_operands();
    }
    gen_int_promotion(x, ctx);
    gen_int_promotion(y, ctx);
    string op;
    if (is_signed_integer_type(x.type)) {
        op = "ashr";
    } else {
        op = "lshr";
    }
    return apply(op, x, y, ctx);
}

t_val gen_shl(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_integral_type(x.type) and is_integral_type(y.type))) {
        throw t_bad_operands();
    }
    gen_int_promotion(x, ctx);
    gen_int_promotion(y, ctx);
    return apply("shl", x, y, ctx);
}

t_val gen_div(t_val x, t_val y, const t_ctx& ctx) {
    if (not (is_arithmetic_type(x.type) and is_arithmetic_type(y.type))) {
        throw t_bad_operands();
    }
    gen_arithmetic_conversions(x, y, ctx);
    string op;
    if (is_floating_type(x.type)) {
        op = "fdiv";
    } else if (is_signed_integer_type(x.type)) {
        op = "sdiv";
    } else {
        op = "udiv";
    }
    return apply(op, x, y, ctx);
}

t_val gen_add(t_val x, t_val y, const t_ctx& ctx) {
    if (is_integral_type(x.type) and is_pointer_type(y.type)) {
        swap(x, y);
    }
    if (is_arithmetic_type(x.type) and is_arithmetic_type(y.type)) {
        gen_arithmetic_conversions(x, y, ctx);
        string op;
        if (is_signed_integer_type(x.type)) {
            op = "add nsw";
        } else if (is_floating_type(x.type)) {
            op = "fadd";
        } else {
            op = "add";
        }
        return apply(op, x, y, ctx);
    } else if (is_pointer_type(x.type) and is_integral_type(y.type)) {
        y = gen_conversion(uintptr_t_type, y, ctx);
        t_val res;
        res.value = prog.inc_ptr(ctx.get_asm_val(x), ctx.get_asm_val(y));
        res.type = x.type;
        return res;
    } else {
        throw t_bad_operands();
    }
}

t_val gen_exp_(const t_ast& ast, t_ctx& ctx,
               bool convert_lvalue) {
    auto assign_op = [&](auto& op) {
        auto x = gen_exp(ast[0], ctx, false);
        auto y = gen_exp(ast[1], ctx);
        auto xv = convert_lvalue_to_rvalue(x, ctx);
        auto z = op(xv, y, ctx);
        return gen_convert_assign(x, z, ctx);
    };
    auto& op = ast.uu;
    auto arg_cnt = ast.children.size();
    t_val x;
    t_val y;
    t_val res;
    if (op == "integer_constant") {
        res.value = ast.vv;
        res.type = int_type;
    } else if (op == "floating_constant") {
        res.value = ast.vv;
        res.type = double_type;
    } else if (op == "string_literal") {
        res.value = prog.def_str(ast.vv);
        res.type = make_array_type(char_type, ast.vv.length() + 1);
        res.is_lvalue = true;
    } else if (op == "identifier") {
        auto& id = ast.vv;
        res.value += ctx.get_var_asm_var(id);
        res.type = ctx.get_var_type(id);
        res.is_lvalue = true;
    } else if (op == "+" and arg_cnt == 1) {
        res = gen_exp(ast[0], ctx);
        if (not is_arithmetic_type(res.type)) {
            throw t_bad_operands();
        }
        gen_int_promotion(res, ctx);
    } else if (op == "&" and arg_cnt == 1) {
        res = gen_exp(ast[0], ctx, false);
        if (not res.is_lvalue) {
            throw t_bad_operands();
        }
        res.type = make_pointer_type(res.type);
        res.is_lvalue = false;
    } else if (op == "*" and arg_cnt == 1) {
        auto e = gen_exp(ast[0], ctx);
        if (not is_pointer_type(e.type)) {
            throw t_bad_operands();
        }
        res = dereference(e);
    } else if (op == "-" and arg_cnt == 1) {
        auto e = gen_exp(ast[0], ctx);
        if (not is_arithmetic_type(e.type)) {
            throw t_bad_operands();
        }
        res = gen_neg(e, ctx);
    } else if (op == "!" and arg_cnt == 1) {
        auto e = gen_exp(ast[0], ctx);
        if (not is_scalar_type(e.type)) {
            throw t_bad_operands();
        }
        auto z = gen_eq(e, {"0", int_type}, ctx);
        res = gen_conversion(int_type, z, ctx);
    } else if (op == "~" and arg_cnt == 1) {
        x = gen_exp(ast[0], ctx);
        if (not is_integral_type(x.type)) {
            throw t_bad_operands();
        }
        gen_int_promotion(x, ctx);
        res.value = prog.make_new_id();
        res.type = x.type;
        let(res, fun("xor " + ctx.get_asm_type(x.type), "-1", x.value));
        res.type = int_type;
    } else if (op == "=") {
        res = gen_convert_assign(gen_exp(ast[0], ctx, false),
                                 gen_exp(ast[1], ctx),
                                 ctx);
    } else if (op == "+") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_add(x, y, ctx);
    } else if (op == "-") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_sub(x, y, ctx);
    } else if (op == "*") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_mul(x, y, ctx);
    } else if (op == "/") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_div(x, y, ctx);
    } else if (op == "%") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_mod(x, y, ctx);
    } else if (op == "<<") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_shl(x, y, ctx);
    } else if (op == ">>") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_shr(x, y, ctx);
    } else if (op == "<=") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        if (is_arithmetic_type(x.type)
            and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "icmp sle";
            } else if (is_floating_type(x.type)) {
                op = "fcmp ole";
            } else {
                op = "icmp ule";
            }
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun(op + " " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else if (is_pointer_type(x.type)
                   and is_pointer_type(y.type)
                   and compatible(unqualify(x.type),
                                  unqualify(y.type))) {
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun("icmp ule " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else {
            throw t_bad_operands();
        }
    } else if (op == "<") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        if (is_arithmetic_type(x.type)
            and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "icmp slt";
            } else if (is_floating_type(x.type)) {
                op = "fcmp olt";
            } else {
                op = "icmp ult";
            }
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun(op + " " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else if (is_pointer_type(x.type)
                   and is_pointer_type(y.type)
                   and compatible(unqualify(x.type),
                                  unqualify(y.type))) {
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun("icmp ult " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else {
            throw t_bad_operands();
        }
    } else if (op == ">") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        if (is_arithmetic_type(x.type)
            and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "icmp sgt";
            } else if (is_floating_type(x.type)) {
                op = "fcmp ogt";
            } else {
                op = "icmp ugt";
            }
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun(op + " " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else if (is_pointer_type(x.type)
                   and is_pointer_type(y.type)
                   and compatible(unqualify(x.type),
                                  unqualify(y.type))) {
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun("icmp ugt " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else {
            throw t_bad_operands();
        }
    } else if (op == ">=") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        if (is_arithmetic_type(x.type)
            and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_signed_integer_type(x.type)) {
                op = "icmp sge";
            } else if (is_floating_type(x.type)) {
                op = "fcmp oge";
            } else {
                op = "icmp uge";
            }
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun(op + " " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else if (is_pointer_type(x.type)
                   and is_pointer_type(y.type)
                   and compatible(unqualify(x.type),
                                  unqualify(y.type))) {
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun("icmp uge " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else {
            throw t_bad_operands();
        }
    } else if (op == "==") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        auto tmp = gen_eq(x, y, ctx);
        res = gen_conversion(int_type, tmp, ctx);
    } else if (op == "!=") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        if (is_arithmetic_type(x.type)
            and is_arithmetic_type(y.type)) {
            gen_arithmetic_conversions(x, y, ctx);
            string op;
            if (is_floating_type(x.type)) {
                op = "fcmp one";
            } else {
                op = "icmp ne";
            }
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun(op + " " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else if (is_pointer_type(x.type)
                   or is_pointer_type(y.type)) {
            gen_eq_ptr_conversions(x, y, ctx);
            auto tmp = t_val{prog.make_new_id(), bool_type};
            let(tmp,
                fun("icmp eq " + ctx.get_asm_type(x.type), x, y));
            res = gen_conversion(int_type, tmp, ctx);
        } else {
            throw t_bad_operands();
        }
    } else if (op == "&") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_and(x, y, ctx);
    } else if (op == "^") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_xor(x, y, ctx);
    } else if (op == "|") {
        x = gen_exp(ast[0], ctx);
        y = gen_exp(ast[1], ctx);
        res = gen_or(x, y, ctx);
    } else if (op == "&&") {
        auto l0 = make_label();
        auto l1 = make_label();
        auto l2 = make_label();
        auto l3 = make_label();
        x = gen_exp(ast[0], ctx);
        put_label(l0);
        prog.cond_br(gen_is_zero(x, ctx).value, l1, l2);
        put_label(l2, false);
        y = gen_exp(ast[1], ctx);
        if (not (is_scalar_type(x.type)
                 and is_scalar_type(y.type))) {
            throw t_bad_operands();
        }
        auto z = gen_is_nonzero(y, ctx);
        put_label(l3);
        put_label(l1);
        auto tmp = t_val{prog.make_new_id(), bool_type};
        let(tmp, ("phi i1 [ false, " + l0 + " ], [ "
                  + z.value + ", " + l3 + " ]"));
        res = gen_conversion(int_type, tmp, ctx);
    } else if (op == "||") {
        auto l0 = make_label();
        auto l1 = make_label();
        auto l2 = make_label();
        auto l3 = make_label();
        x = gen_exp(ast[0], ctx);
        put_label(l0);
        prog.cond_br(gen_is_zero(x, ctx).value, l2, l1);
        put_label(l2, false);
        y = gen_exp(ast[1], ctx);
        if (not (is_scalar_type(x.type)
                 and is_scalar_type(y.type))) {
            throw t_bad_operands();
        }
        auto z = gen_is_nonzero(y, ctx);
        put_label(l3);
        put_label(l1);
        auto tmp = t_val{prog.make_new_id(), bool_type};
        let(tmp, ("phi i1 [ true, " + l0 + " ], [ "
                  + z.value + ", " + l3 + " ]"));
        res = gen_conversion(int_type, tmp, ctx);
    } else if (op == "*=") {
        res = assign_op(gen_mul);
    } else if (op == "/=") {
        res = assign_op(gen_div);
    } else if (op == "%=") {
        res = assign_op(gen_mod);
    } else if (op == "+=") {
        res = assign_op(gen_add);
    } else if (op == "-=") {
        res = assign_op(gen_sub);
    } else if (op == "<<=") {
        res = assign_op(gen_shl);
    } else if (op == ">>=") {
        res = assign_op(gen_shr);
    } else if (op == "&=") {
        res = assign_op(gen_and);
    } else if (op == "^=") {
        res = assign_op(gen_xor);
    } else if (op == "|=") {
        res = assign_op(gen_or);
    } else if (op == ",") {
        gen_exp(ast[0], ctx);
        res = gen_exp(ast[1], ctx);
    } else if (op == "function_call") {
        if (ast[0] == t_ast("identifier", "printf")) {
            res.type = int_type;
            res.value = prog.make_new_id();
            string args_str;
            for (auto i = size_t(1); i < ast.children.size(); i++) {
                auto val = gen_exp(ast[i], ctx);
                if (val.type == float_type) {
                    val = gen_conversion(double_type, val, ctx);
                }
                auto arg = ctx.get_asm_type(val.type) + " " + val.value;
                if (args_str.empty()) {
                    args_str += arg;
                } else {
                    args_str += ", " + arg;
                }
            }
            a(res.value
              + " = call i32 (i8*, ...) @printf(" + args_str + ")");
        }
    } else if (op == "struct_member") {
        x = gen_exp(ast[0], ctx, false);
        auto member_id = ast[1].vv;
        if (not is_struct_type(x.type)) {
            throw t_bad_operands();
        }
        auto struct_name = x.type.get_name();
        auto t = ctx.get_type(struct_name);
        assert(t.is_complete());
        auto idx = t.get_member_index(member_id);
        if (idx == -1) {
            throw t_bad_operands();
        }
        if (x.is_lvalue) {
            res.is_lvalue = true;
        }
        res.type = t.get_member_type(idx);
        res.value = prog.make_new_id();
        auto at = ctx.get_type_asm_var(struct_name);
        a(res.value + " = getelementptr inbounds " + at
          + ", " + at + "* " + x.value + ", i32 0, i32 " + to_string(idx));
    } else if (op == "array_subscript") {
        try {
            auto z = gen_add(gen_exp(ast[0], ctx),
                             gen_exp(ast[1], ctx), ctx);
            res = dereference(z);
        } catch (const t_bad_operands& e) {
            throw t_bad_operands();
        }
    } else if (op == "postfix_increment") {
        auto e = gen_exp(ast[0], ctx, false);
        res = convert_lvalue_to_rvalue(e, ctx);
        if (not is_scalar_type(unqualify(res.type))
            or not is_modifiable_lvalue(res)) {
            throw t_bad_operands();
        }
        auto z = gen_add(res, {"1", int_type}, ctx);
        gen_assign(e, z, ctx);
    } else if (op == "postfix_decrement") {
        auto e = gen_exp(ast[0], ctx, false);
        res = convert_lvalue_to_rvalue(e, ctx);
        if (not is_scalar_type(unqualify(res.type))
            or not is_modifiable_lvalue(res)) {
            throw t_bad_operands();
        }
        auto z = gen_sub(res, {"1", int_type}, ctx);
        gen_assign(e, z, ctx);
    } else if (op == "prefix_increment") {
        auto e = gen_exp(ast[0], ctx, false);
        auto z = convert_lvalue_to_rvalue(e, ctx);
        if (not is_scalar_type(unqualify(z.type))
            or not is_modifiable_lvalue(z)) {
            throw t_bad_operands();
        }
        res = gen_add(z, {"1", int_type}, ctx);
        gen_assign(e, res, ctx);
    } else if (op == "prefix_decrement") {
        auto e = gen_exp(ast[0], ctx, false);
        auto z = convert_lvalue_to_rvalue(e, ctx);
        if (not is_scalar_type(unqualify(z.type))
            or not is_modifiable_lvalue(z)) {
            throw t_bad_operands();
        }
        res = gen_sub(z, {"1", int_type}, ctx);
        gen_assign(e, res, ctx);
    } else if (op == "cast") {
        auto e = gen_exp(ast[1], ctx);
        auto t = make_type(ast[0], ctx);
        res = gen_conversion(t, e, ctx);
    } else {
        throw logic_error("unhandled operator " + op);
    }
    if (convert_lvalue and res.is_lvalue) {
        if (is_array_type(res.type)) {
            res = gen_array_elt(res, 0, ctx);
            res.type = make_pointer_type(res.type);
            res.is_lvalue = false;
        } else {
            res = convert_lvalue_to_rvalue(res, ctx);
        }
    }
    return res;
}
t_val gen_exp(const t_ast& ast, t_ctx& ctx, bool convert_lvalue) {
    try {
        return gen_exp_(ast, ctx, convert_lvalue);
    } catch (t_bad_operands) {
        auto op = ast.uu;
        if (ast.vv != "") {
            op += " " + ast.vv;
        }
        throw t_bad_operands(op, ast.loc);
    }
}
