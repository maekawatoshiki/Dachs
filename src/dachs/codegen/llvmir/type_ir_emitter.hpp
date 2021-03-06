#if !defined DACHS_CODEGEN_LLVMIR_TYPE_IR_GENERATOR_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_TYPE_IR_GENERATOR_HPP_INCLUDED

#include <vector>
#include <unordered_map>
#include <type_traits>
#include <cassert>
#include <cstddef>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/codegen/llvmir/ir_utility.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class type_ir_emitter {
    llvm::LLVMContext &context;
    semantics::lambda_captures_type const& lambda_captures;
    std::unordered_map<scope::class_scope, llvm::PointerType *const> class_table;

public:
    type_ir_emitter(llvm::LLVMContext &c, decltype(lambda_captures) const& lc)
        : context(c), lambda_captures(lc)
    {}

    llvm::Type *emit(type::type const& any)
    {
        return helper::variant::apply_lambda(
                [&](auto const& t){ return emit(t); }
                , any.raw_value());
    }

    llvm::Type *emit(type::builtin_type const& builtin)
    {
        llvm::Type *result = nullptr;

        if (builtin->name == "int") {
            result = llvm::Type::getInt64Ty(context);
        } else if (builtin->name == "uint") {
            result = llvm::Type::getInt64Ty(context);
        } else if (builtin->name == "float") {
            result = llvm::Type::getDoubleTy(context);
        } else if (builtin->name == "char") {
            result = llvm::Type::getInt8Ty(context);
        } else if (builtin->name == "bool") {
            result = llvm::Type::getInt1Ty(context);
        } else if (builtin->name == "symbol") {
            result = llvm::Type::getInt64Ty(context);
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        if (!result) {
            error("  Failed to emit a builtin type: " + builtin->to_string());
        }

        return result;
    }

    llvm::PointerType *emit(type::class_type const& t)
    {
        assert(!t->ref.expired());
        auto const scope = t->ref.lock();
        assert(!scope->is_template());

        auto const i = class_table.find(scope);
        if (i != std::end(class_table)) {
            return i->second;
        }

        std::vector<llvm::Type *> elem_types;
        elem_types.reserve(scope->instance_var_symbols.size());
        for (auto const& s : scope->instance_var_symbols) {
            elem_types.push_back(emit(s->type));
        }

        auto *const result
            = check(
                    llvm::PointerType::getUnqual(
                        llvm::StructType::create(
                            context,
                            elem_types,
                            "class." + scope->name
                        )
                    ),
                    "class type"
                );
        class_table.insert({scope, result});

        return result;
    }

    llvm::PointerType *emit(type::tuple_type const& t)
    {
        std::vector<llvm::Type *> element_type_irs;
        element_type_irs.reserve(t->element_types.size());
        for (auto const& t : t->element_types) {
            element_type_irs.push_back(emit(t));
        }

        return check(
            llvm::PointerType::getUnqual(
                llvm::StructType::get(context, element_type_irs)
            )
            , "tuple type"
        );
    }

    llvm::PointerType *emit(type::array_type const& a)
    {
        return llvm::PointerType::getUnqual(emit(a->element_type));
    }

    llvm::PointerType *emit(type::pointer_type const& p)
    {
        auto *const ty = emit(p->pointee_type);
        if (p->pointee_type.is_aggregate()) {
            // Note:
            // i64* -> i64*
            // {i64, double}* -> {i64, double}*
            auto *const ptr = llvm::dyn_cast<llvm::PointerType>(ty);
            assert(ptr);
            return ptr;
        } else {
            // Note:
            // double -> double*
            // i64* -> i64**
            return llvm::PointerType::getUnqual(ty);
        }
    }

    llvm::Type *emit(type::func_type const& f)
    {
        assert(f->return_type);
        auto *const ret_ty = emit(*f->return_type);

        std::vector<llvm::Type *> arg_tys;
        for (auto const& t : f->param_types) {
            arg_tys.push_back(emit(t));
        }

        return llvm::FunctionType::get(
                ret_ty,
                arg_tys,
                false
            )->getPointerTo();
    }

    llvm::PointerType *emit(type::generic_func_type const& g)
    {
        if (!g->ref || g->ref->expired()) {
            return llvm::PointerType::getUnqual(
                    llvm::StructType::get(context, {})
                );
        }

        auto const itr = lambda_captures.find(g);
        if (itr == std::end(lambda_captures)) {
            return llvm::PointerType::getUnqual(
                    llvm::StructType::get(context, {})
                );
        }

        auto const& captures = itr->second;
        std::vector<llvm::Type *> capture_types;
        capture_types.reserve(captures.size());
        for (auto const& capture : captures.get<semantics::tags::offset>()) {
            capture_types.push_back(emit(capture.introduced->type));
        }

        return llvm::PointerType::getUnqual(
                llvm::StructType::get(context, capture_types)
            );
    }

    llvm::Type *emit(type::qualified_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "qualified type LLVM IR generation"};
    }

    llvm::Type *emit(type::template_type const&)
    {
        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
    }

    llvm::Type *emit_alloc_type(type::type const& any)
    {
        return any.apply_lambda([this](auto const& t){ return emit_alloc_type(t); });
    }

    llvm::Type *emit_alloc_type(type::builtin_type const& t)
    {
        // Note:
        // No need to strip pointer type because builtin type is treated by value.
        return emit(t);
    }

    llvm::Type *emit_alloc_type(type::pointer_type const& t)
    {
        // Note:
        // No need to strip pointer type because builtin type is treated by value.
        return emit(t);
    }

    llvm::Type *emit_alloc_type(type::func_type const& t)
    {
        // Note:
        // No need to strip pointer type
        return emit(t);
    }

    template<class T>
    llvm::Type *emit_alloc_type(T const& t)
    {
        auto const ty = emit(t);
        if (ty->isPointerTy()) {
            return ty->getPointerElementType();
        } else {
            return ty;
        }
    }

    llvm::ArrayType *emit_alloc_fixed_array(type::array_type const& a)
    {
        assert(a->size);
        return emit_alloc_fixed_array(a->element_type, *a->size);
    }

    llvm::ArrayType *emit_alloc_fixed_array(type::type const& e, std::size_t const size)
    {
        auto *const ty = e.is_aggregate()
            ? emit(e)
            : emit_alloc_type(e);

        return llvm::ArrayType::get(ty, size);
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_TYPE_IR_GENERATOR_HPP_INCLUDED
