#if !defined DACHS_SEMANTICS_SCOPE_HPP_INCLUDED
#define      DACHS_SEMANTICS_SCOPE_HPP_INCLUDED

#include <vector>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <cstddef>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include "dachs/warning.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/ast/ast_fwd.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {

// Dynamic resources to use actually
namespace scope {

using dachs::helper::make;

} // namespace scope

// Implementation of nodes of scope tree
namespace scope_node {

using dachs::helper::variant::apply_lambda;
using function_set = std::unordered_set<scope::func_scope>;

struct basic_scope {
    // Note:
    // I don't use base class pointer to check the parent is alive or not.
    // Or should I?
    scope::enclosing_scope_type enclosing_scope;

    template<class AnyScope>
    explicit basic_scope(AnyScope const& parent) noexcept
        : enclosing_scope(parent)
    {}

    basic_scope() noexcept
    {}

    virtual ~basic_scope() noexcept
    {}

    using maybe_func_t = boost::optional<scope::func_scope>;
    using maybe_class_t = boost::optional<scope::class_scope>;
    using maybe_var_t = boost::optional<symbol::var_symbol>;

    template<class Symbol>
    bool define_symbol(std::vector<Symbol> &container, Symbol const& symbol)
    {
        if (boost::algorithm::starts_with(symbol->name, "__builtin_")) {
            semantics::output_semantic_error(symbol->ast_node.get_shared(), "  '__builtin_' prefix is only permitted for built-in names");
            return false;
        }

        static_assert(std::is_base_of<symbol_node::basic_symbol, typename Symbol::element_type>::value, "define_symbol(): Not a symbol");
        if (auto maybe_duplication = helper::find_if(container, [&symbol](auto const& s){ return *symbol == *s; })) {
            semantics::print_duplication_error(symbol->ast_node.get_shared(), (*maybe_duplication)->ast_node.get_shared(), symbol->name);
            return false;
        }

        // TODO: raise warning when a variable shadows other variables
        container.push_back(symbol);
        return true;
    }

    // TODO resolve member variables and member functions

    virtual function_set resolve_func(std::string const& name, std::vector<type::type> const& args) const
    {
        // TODO:
        // resolve_func() now searches function scopes directly.
        // But it should search variables, check the result is funcref type, then resolve function overloads.
        return apply_lambda(
                [&](auto const& s)
                    -> function_set
                {
                    return s.lock()->resolve_func(name, args);
                }, enclosing_scope);
    }

    virtual maybe_class_t resolve_class_by_name(std::string const& name) const
    {
        return apply_lambda(
                [&name](auto const& s)
                    -> maybe_class_t
                {
                    return s.lock()->resolve_class_by_name(name);
                }, enclosing_scope);
    }

    virtual maybe_class_t resolve_class_template(std::string const& name, std::vector<type::type> const& specified) const
    {
        return apply_lambda(
                [&](auto const& s)
                    -> maybe_class_t
                {
                    return s.lock()->resolve_class_template(name, specified);
                }, enclosing_scope);
    }

    virtual maybe_var_t resolve_var(std::string const& name) const
    {
        return apply_lambda(
                [&name](auto const& s)
                    -> maybe_var_t
                {
                    return s.lock()->resolve_var(name);
                }, enclosing_scope);
    }

    virtual maybe_var_t resolve_receiver() const
    {
        return apply_lambda(
                [](auto const& s)
                    -> maybe_var_t
                {
                    return s.lock()->resolve_receiver();
                }, enclosing_scope);
    }

    struct enclosing_func_resolver : boost::static_visitor<maybe_func_t> {
        maybe_func_t operator()(scope::weak_func_scope const& f) const
        {
            return f.lock();
        }

        template<class S>
        maybe_func_t operator()(S const& s) const
        {
            return s.lock()->get_enclosing_func();
        }
    };

    virtual maybe_func_t get_enclosing_func() const
    {
        return boost::apply_visitor(
                    enclosing_func_resolver{},
                    enclosing_scope
                );
    }

    void warn_or_check_shadowing_var_recursively(maybe_var_t const& maybe_shadowing, symbol::var_symbol const& new_var) const;
    virtual void check_shadowing_variable(symbol::var_symbol const& new_var) const;
};

struct global_scope final : public basic_scope {
    std::vector<scope::func_scope> functions;
    std::vector<symbol::var_symbol> const_symbols;
    std::vector<scope::class_scope> classes;
    std::weak_ptr<ast::node_type::inu> ast_root;
    std::vector<scope::func_scope> cast_funcs;

    template<class RootType>
    global_scope(RootType const& ast_root) noexcept
        : basic_scope(), ast_root(ast_root)
    {}

    // Note:
    // Check function duplication after forward analysis because of overload resolution
    void define_function(scope::func_scope const& new_func) noexcept;

    bool define_variable(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(const_symbols, new_var);
    }

    // Note:
    // Do not check duplication because of overloaded functions.  Check for overloaded functions
    // is already done by define_function()
    void force_define_constant(symbol::var_symbol const& new_var) noexcept
    {
        // TODO:
        // The same name variable as the function can be defined now.
        // It should be detected and error should be raised.
        const_symbols.push_back(new_var);
    }

    void define_class(scope::class_scope const& new_class) noexcept
    {
        // Note:
        // Do not check the duplication of class here because it will be checked after in forward analyzer.
        classes.push_back(new_class);
    }

    function_set resolve_func(std::string const& name, std::vector<type::type> const& args) const override;

    maybe_func_t resolve_cast_func(type::type const& from, type::type const& to) const;

    maybe_class_t resolve_class_by_name(std::string const& name) const override
    {
        return helper::find_if(classes, [&name](auto const& c){ return c->name == name; });
    }

    maybe_class_t resolve_class_template(std::string const& name, std::vector<type::type> const& specified) const override;

    maybe_var_t resolve_var(std::string const& name) const override
    {
        return helper::find_if(const_symbols, [&name](auto const& v){ return v->name == name; });
    }

    void check_shadowing_variable(symbol::var_symbol const&) const override
    {
        // Note:
        // Do nothing
        // Global variables (including functions and classes) can be shadowed implicitly.
    }

    maybe_func_t get_enclosing_func() const override
    {
        return boost::none;
    }
};

struct local_scope final : public basic_scope {
    std::vector<scope::local_scope> children;
    std::vector<symbol::var_symbol> local_vars;
    std::vector<scope::func_scope> unnamed_funcs;

    template<class AnyScope>
    explicit local_scope(AnyScope const& enclosing) noexcept
        : basic_scope(enclosing)
    {}

    void define_child(scope::local_scope const& child) noexcept
    {
        children.push_back(child);
    }

    bool define_variable(symbol::var_symbol const& new_var) noexcept
    {
        check_shadowing_variable(new_var);
        return define_symbol(local_vars, new_var);
    }

    bool define_variable_without_shadowing_check(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(local_vars, new_var);
    }

    bool define_unnamed_func(scope::func_scope const& new_func) noexcept
    {
        return define_symbol(unnamed_funcs, new_func);
    }

    maybe_var_t resolve_var(std::string const& name) const override
    {
        auto const target_var = helper::find_if(local_vars, [&name](auto const& v){ return v->name == name; });
        return target_var ?
                *target_var :
                apply_lambda([&name](auto const& s){ return s.lock()->resolve_var(name); }, enclosing_scope);
    }

    void check_shadowing_variable(symbol::var_symbol const& new_var) const override
    {
        warn_or_check_shadowing_var_recursively(
                helper::find_if(local_vars, [&new_var](auto const& v){ return v->name == new_var->name; }),
                new_var
            );
    }
};

struct func_scope final : public basic_scope, public symbol_node::basic_symbol {
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;
    boost::optional<type::type> ret_type;
    bool is_member_func = false;
    boost::optional<bool> is_const_ = boost::none;

    template<class Node, class P>
    explicit func_scope(
              Node const& n
            , P const& p
            , std::string const& s
            , bool const is_builtin = false
    ) noexcept
        : basic_scope(p)
        , basic_symbol(n, s, is_builtin)
    {}

    bool is_main_func() const noexcept
    {
        return name == "main" && !is_member_func;
    }

    func_scope(func_scope const&) = default;

    bool define_param(symbol::var_symbol const& new_var) noexcept
    {
        check_shadowing_variable(new_var);
        return define_symbol(params, new_var);
    }

    void force_push_front_param(symbol::var_symbol const& new_param) noexcept
    {
        params.insert(std::begin(params), new_param);
    }

    bool is_template() const noexcept
    {
        return boost::algorithm::any_of(
                    params,
                    [](auto const& p){ return p->type.is_template(); }
                );
    }

    bool is_const() const noexcept
    {
        return is_const_ && *is_const_;
    }

    ast::node::function_definition get_ast_node() const noexcept;

    bool is_anonymous() const noexcept
    {
        return boost::algorithm::starts_with(name, "lambda.");
    }

    bool is_ctor() const noexcept
    {
        return name == "dachs.init";
    }

    bool is_copier() const noexcept
    {
        return name == "dachs.copy";
    }

    bool is_converter() const noexcept
    {
        return name == "dachs.conv";
    }

    std::string to_string() const noexcept;

    maybe_var_t resolve_var(std::string const& name) const override
    {
        auto const target_var = helper::find_if(params, [&name](auto const& v){ return v->name == name; });
        return target_var ?
                *target_var :
                apply_lambda([&name](auto const& s)
                        {
                            return s.lock()->resolve_var(name);
                        }, enclosing_scope);
    }

    void check_shadowing_variable(symbol::var_symbol const& new_var) const override
    {
        warn_or_check_shadowing_var_recursively(
                helper::find_if(params, [&new_var](auto const& v){ return v->name == new_var->name; }),
                new_var
            );
    }

    maybe_var_t resolve_receiver() const override
    {
        if (!is_member_func) {
            return boost::none;
        }

        assert(!params.empty());
        assert(params[0]->name == "self");

        return params[0];
    }

    boost::optional<scope::class_scope> get_receiver_class_scope() const;

    func_scope &operator=(func_scope const& rhs) = default;

    // Compare with rhs considering overloading
    bool operator==(func_scope const& rhs) const noexcept;

    bool operator!=(func_scope const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct class_scope final : public basic_scope, public symbol_node::basic_symbol {
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::var_symbol> instance_var_symbols;

    // std::vector<type> for instanciated types (if this isn't template, it should contains only one element)

    template<class Node, class Parent>
    explicit class_scope(Node const& ast_node, Parent const& p, std::string const& name, bool const is_builtin = false) noexcept
        : basic_scope(p)
        , basic_symbol(ast_node, name, is_builtin)
    {}

    void define_member_func(scope::func_scope const& new_func) noexcept
    {
        // Do not check because of overload
        member_func_scopes.push_back(new_func);
    }

    bool define_variable(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(instance_var_symbols, new_var);
    }

    maybe_var_t resolve_instance_var(std::string const& name) const
    {
        return helper::find_if(instance_var_symbols, [&name](auto const& i){ return i->name == name; });
    }

    bool is_template() const noexcept
    {
        return boost::algorithm::any_of(instance_var_symbols, [](auto const& s){ return s->type.is_template(); });
    }

    function_set resolve_ctor(std::vector<type::type> const& arg_types) const
    {
        return resolve_func("dachs.init", arg_types);
    }

    boost::optional<size_t> get_instance_var_offset_of(std::string const& name) const noexcept
    {
        for (auto offset = 0u; offset < instance_var_symbols.size(); ++offset) {
            if (name == instance_var_symbols[offset]->name) {
                return offset;
            }
        }

        return boost::none;
    }

    ast::node::class_definition get_ast_node() const noexcept;

    std::string to_string() const noexcept;

    bool operator==(class_scope const& rhs) const noexcept;
    bool operator!=(class_scope const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

} // namespace scope_node

namespace ast {
struct ast;
} // namespace ast

namespace scope {

struct var_symbol_resolver
    : boost::static_visitor<boost::optional<symbol::var_symbol>> {
    std::string const& name;

    explicit var_symbol_resolver(std::string const& n) noexcept
        : name{n}
    {}

    template<class T>
    result_type operator()(std::shared_ptr<T> const& scope) const noexcept
    {
        return scope->resolve_var(name);
    }
};

} // namespace scope

} // namespace dachs

#endif    // DACHS_SEMANTICS_SCOPE_HPP_INCLUDED
