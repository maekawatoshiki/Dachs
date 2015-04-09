#if !defined DACHS_CODEGEN_LLVMIR_ALLOCATION_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_ALLOCATION_EMITTER_HPP_INCLUDED

#include <cassert>

#include "dachs/semantics/type.hpp"

#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>

namespace dachs {
namespace codegen {
namespace llvmir {
namespace detail {

// Note:
// Should I define our original malloc() and realloc() in runtime?

class allocation_emitter {
    context &ctx;
    type_ir_emitter &type_emitter;
    llvm::Module &module;
    llvm::Function *realloc_func = nullptr;

    using val = llvm::Value *;

    llvm::Function *emit_realloc_func()
    {
        if (realloc_func) {
            return realloc_func;
        }

        auto const func_type = llvm::FunctionType::get(
                ctx.builder.getInt8PtrTy(),
                {
                    ctx.builder.getInt8PtrTy(),
                    ctx.builder.getIntPtrTy(ctx.data_layout)
                },
                false
            );

        return llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                "realloc",
                &module
            );
    }

    val emit_malloc_call(llvm::BasicBlock *const insert_end, llvm::Type *const elem_ty, val const size_value)
    {
        auto *const intptr_ty = ctx.builder.getIntPtrTy(ctx.data_layout);
        auto *const emitted
            = llvm::CallInst::CreateMalloc(
                    insert_end,
                    intptr_ty,
                    elem_ty,
                    llvm::ConstantInt::get(intptr_ty, ctx.data_layout->getTypeAllocSize(elem_ty)),
                    size_value,
                    nullptr /*malloc func*/,
                    "malloc.call"
                );
        ctx.builder.Insert(emitted);

        assert(emitted->getType() == elem_ty->getPointerTo());

        return emitted;
    }

    val emit_bit_cast(val const from_val, llvm::Type *const to_ty, llvm::BasicBlock *const insert_end) const
    {
        auto const ty = from_val->getType();
        assert(ty->isPointerTy() && to_ty->isPointerTy());

        if (ty == to_ty) {
            return from_val;
        }

        return new llvm::BitCastInst(from_val, to_ty, "", insert_end);
    }

    val emit_realloc_call(llvm::BasicBlock *const insert_end, val const ptr_value, val const size_value)
    {
        auto *const intptr_ty = ctx.builder.getIntPtrTy(ctx.data_layout);
        auto *const ptr_ty = ptr_value->getType();
        auto *const elem_ty = ptr_ty->getPointerElementType();
        assert(elem_ty);

        auto const elem_size = ctx.data_layout->getTypeAllocSize(elem_ty);
        auto *const elem_size_value = llvm::ConstantInt::get(intptr_ty, elem_size);

        val const casted_ptr = emit_bit_cast(ptr_value, ctx.builder.getInt8PtrTy(), insert_end);

        auto *const size_constant = llvm::dyn_cast<llvm::ConstantInt>(size_value);
        val const new_size_value
            = size_constant ?
                val{llvm::ConstantInt::get(intptr_ty, size_constant->getZExtValue() * elem_size)} :
                llvm::BinaryOperator::CreateMul(size_value, elem_size_value, "newsize");

        auto *const reallocated
            = llvm::CallInst::Create(
                    emit_realloc_func(),
                    {
                        casted_ptr,
                        new_size_value
                    },
                    "realloccall"
                );

        return emit_bit_cast(reallocated, ptr_ty, insert_end);
    }

    template<class AllocEmitter>
    val emit_null_on_zero_otherwise(val const size_value, AllocEmitter unless_zero)
    {
        assert(!llvm::isa<llvm::Constant>(size_value));

        auto *const zero_block = ctx.builder.GetInsertBlock();
        auto *const parent = zero_block->getParent();
        auto *const nonzero_block = llvm::BasicBlock::Create(ctx.llvm_context, "alloc.nonzero", parent);
        auto *const merge_block = llvm::BasicBlock::Create(ctx.llvm_context, "alloc.merge", parent);

        auto *const cond_value = ctx.builder.CreateICmpEQ(size_value, ctx.builder.getInt64(0u));
        ctx.builder.CreateCondBr(cond_value, merge_block, nonzero_block);

        ctx.builder.SetInsertPoint(nonzero_block);
        auto *const nonnull_value = unless_zero(nonzero_block);
        ctx.builder.CreateBr(merge_block);

        assert(then_value->getType() == nonnull_value->getType());

        ctx.builder.SetInsertPoint(merge_block);

        auto *const ptr_ty = llvm::dyn_cast<llvm::PointerType>(nonnull_value->getType());
        assert(ptr_ty);

        auto *const merged = ctx.builder.CreatePHI(ptr_ty, 2, "alloc.phi");
        merged->addIncoming(llvm::ConstantPointerNull::get(ptr_ty), zero_block);
        merged->addIncoming(nonnull_value, nonzero_block);

        return merged;
    }

public:

    allocation_emitter(context &c, type_ir_emitter &e, llvm::Module &m) noexcept
        : ctx(c), type_emitter(e), module(m)
    {}

    val emit_malloc(type::type const& elem_type, std::size_t const array_size)
    {
        auto *const elem_ty = type_emitter.emit_alloc_type(elem_type);
        if (array_size == 0u) {
            return llvm::ConstantPointerNull::get(elem_ty->getPointerTo());
        } else {
            return emit_malloc_call(
                    ctx.builder.GetInsertBlock(),
                    elem_ty,
                    llvm::ConstantInt::get(ctx.builder.getIntPtrTy(ctx.data_layout), array_size)
                );
        }
    }

    val emit_malloc(type::type const& elem_type, val const size_value)
    {
        if (auto *const const_size = llvm::dyn_cast<llvm::ConstantInt>(size_value)) {
            // Note:
            // Although it optimizes and removes the branch, I do that by my hand to be sure
            return emit_malloc(elem_type, const_size->getZExtValue());
        }

        return emit_null_on_zero_otherwise(
                size_value,
                [&elem_type, size_value, this](auto const else_block)
                {
                    return emit_malloc_call(
                        else_block,
                        type_emitter.emit_alloc_type(elem_type),
                        size_value
                    );
                }
            );
    }

    val emit_malloc(type::type const& elem_type)
    {
        return emit_malloc(elem_type, 1u);
    }

    val emit_realloc(val const ptr_value, std::size_t const array_size)
    {
        auto const ptr_ty = llvm::dyn_cast<llvm::PointerType>(ptr_value->getType());
        assert(ptr_ty);

        if (array_size == 0u) {
            return llvm::ConstantPointerNull::get(ptr_ty);
        } else {
            return emit_realloc_call(
                    ctx.builder.GetInsertBlock(),
                    ptr_value,
                    llvm::ConstantInt::get(ctx.builder.getIntPtrTy(ctx.data_layout), array_size)
                );
        }
    }

    val emit_realloc(val const ptr_value, val const size_value)
    {
        if (auto *const const_size = llvm::dyn_cast<llvm::ConstantInt>(size_value)) {
            // Note:
            // Although it optimizes and removes the branch, I do that by my hand to be sure
            return emit_realloc(ptr_value, const_size->getZExtValue());
        }

        return emit_null_on_zero_otherwise(
                size_value,
                [ptr_value, size_value, this](auto const else_block)
                {
                    return emit_realloc_call(
                        else_block,
                        ptr_value,
                        size_value
                    );
                }
            );
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_ALLOCATION_EMITTER_HPP_INCLUDED
