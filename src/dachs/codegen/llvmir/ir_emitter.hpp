#if !defined DACHS_CODEGEN_LLVMIR_CODE_GENERATOR_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_CODE_GENERATOR_HPP_INCLUDED

#include <llvm/IR/Module.h>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/codegen/llvmir/context.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

llvm::Module &emit_llvm_ir(ast::ast const& a, semantics::semantics_context const& t, context &ctx);

} // namespace llvm
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_CODE_GENERATOR_HPP_INCLUDED
