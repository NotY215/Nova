#include "NativeCompiler.hpp"
#include "native/NativeRuntime.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/DynamicLibrarySearchGenerator.h>

#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vayu {

    // ===========================================================================
    // Internal codegen state
    // ===========================================================================

    class NativeCompiler::Impl {
    public:
        llvm::LLVMContext                                    ctx;
        std::unique_ptr<llvm::Module>                        module;
        std::unique_ptr<llvm::orc::LLJIT>                    jit;

        // Per-function codegen state
        llvm::IRBuilder<>* builder = nullptr;
        llvm::Function* currentFn = nullptr;

        // Variable slots (all i64 for now)
        std::unordered_map<std::string, llvm::AllocaInst*>   varSlots;

        // ---- helpers ----
        llvm::Type* i64Ty();
        llvm::Type* i1Ty();
        llvm::Type* voidTy();

        void ensureJIT();
        void collectVariables(const Block& program);

        // codegen
        llvm::Value* emitExpr(const Expr* e);
        void         emitStmt(const Stmt* s);
        void         emitBlock(const Block& b);

        // builtin lookup: returns nullptr if name isn't an intrinsic
        llvm::FunctionCallee lookupBuiltin(const std::string& name);
    };

    llvm::Type* NativeCompiler::Impl::i64Ty() { return llvm::Type::getInt64Ty(ctx); }
    llvm::Type* NativeCompiler::Impl::i1Ty() { return llvm::Type::getInt1Ty(ctx); }
    llvm::Type* NativeCompiler::Impl::voidTy() { return llvm::Type::getVoidTy(ctx); }

    // ===========================================================================
    // JIT initialization
    // ===========================================================================

    void NativeCompiler::Impl::ensureJIT() {
        if (jit) return;

        // Make sure LLVM knows about the host target.
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        auto jitOrErr = llvm::orc::LLJITBuilder().create();
        if (!jitOrErr) {
            std::string msg;
            llvm::handleAllErrors(jitOrErr.takeError(),
                [&](llvm::ErrorInfoBase& e) { msg = e.message(); });
            throw std::runtime_error("failed to create LLJIT: " + msg);
        }
        jit = std::move(*jitOrErr);

        // Let the JIT resolve symbols (vayu_print_int, ...) against this process.
        auto& jd = jit->getMainJITDylib();
        auto genOrErr = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            jit->getDataLayout().getGlobalPrefix());
        if (!genOrErr) {
            std::string msg;
            llvm::handleAllErrors(genOrErr.takeError(),
                [&](llvm::ErrorInfoBase& e) { msg = e.message(); });
            throw std::runtime_error("failed to create dynamic symbol generator: " + msg);
        }
        jd.addGenerator(std::move(*genOrErr));
    }

    // ===========================================================================
    // Variable collection (walk the AST once, before codegen)
    // ===========================================================================

    static void collectVarsFromBlock(const Block& b,
        std::unordered_set<std::string>& out);

    static void collectVarsFromStmt(const Stmt* s,
        std::unordered_set<std::string>& out) {
        if (!s) return;
        switch (s->kind) {
        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            if (n->target->kind == ExprKind::NameRef)
                out.insert(static_cast<const NameRefExpr*>(n->target.get())->name);
            break;
        }
        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            out.insert(n->name);
            break;
        }
        case StmtKind::Def: {
            auto* n = static_cast<const DefStmt*>(s);
            out.insert(n->name);
            collectVarsFromBlock(n->body, out);
            break;
        }
        case StmtKind::If: {
            auto* n = static_cast<const IfStmt*>(s);
            collectVarsFromBlock(n->thenBody, out);
            for (auto& ec : n->elifs) collectVarsFromBlock(ec.body, out);
            if (n->elseBody) collectVarsFromBlock(*n->elseBody, out);
            break;
        }
        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);
            collectVarsFromBlock(n->body, out);
            break;
        }
        case StmtKind::For: {
            auto* n = static_cast<const ForStmt*>(s);
            out.insert(n->targetName);
            collectVarsFromBlock(n->body, out);
            break;
        }
        case StmtKind::Try: {
            auto* n = static_cast<const TryStmt*>(s);
            collectVarsFromBlock(n->tryBody, out);
            for (auto& h : n->handlers) {
                if (!h.varName.empty()) out.insert(h.varName);
                collectVarsFromBlock(h.body, out);
            }
            if (n->finallyBody) collectVarsFromBlock(*n->finallyBody, out);
            break;
        }
        default: break;
        }
    }

    static void collectVarsFromBlock(const Block& b,
        std::unordered_set<std::string>& out) {
        for (auto& s : b.stmts) collectVarsFromStmt(s.get(), out);
    }

    void NativeCompiler::Impl::collectVariables(const Block& program) {
        std::unordered_set<std::string> names;
        collectVarsFromBlock(program, names);

        // Create an alloca i64 for each, initialized to 0.
        builder->SetInsertPoint(&currentFn->getEntryBlock(),
            currentFn->getEntryBlock().begin());
        llvm::AllocaInst* anySlot = nullptr;
        for (auto& name : names) {
            // Skip function names — they're CFunctions in a later phase.
            auto* slot = builder->CreateAlloca(i64Ty(), nullptr, name);
            builder->CreateStore(llvm::ConstantInt::get(i64Ty(), 0), slot);
            varSlots[name] = slot;
            if (!anySlot) anySlot = slot;
        }
        // Position the builder after all allocas so the entry block stays tidy.
        builder->SetInsertPoint(&currentFn->getEntryBlock(),
            currentFn->getEntryBlock().end());
    }

    // ===========================================================================
    // Builtin lookup
    // ===========================================================================

    llvm::FunctionCallee NativeCompiler::Impl::lookupBuiltin(const std::string& name) {
        if (name == "print") {
            // We only support print(int) for now.  Later phases will add
            // an overload-resolution layer.
            return module->getOrInsertFunction(
                "vayu_print_int",
                llvm::FunctionType::get(voidTy(), { i64Ty() }, false));
        }
        if (name == "print_bool") {
            return module->getOrInsertFunction(
                "vayu_print_bool",
                llvm::FunctionType::get(voidTy(), { i1Ty() }, false));
        }
        return nullptr;
    }

    // ===========================================================================
    // Expression codegen
    // ===========================================================================

    llvm::Value* NativeCompiler::Impl::emitExpr(const Expr* e) {
        if (!e) return llvm::ConstantInt::get(i64Ty(), 0);

        switch (e->kind) {

        case ExprKind::IntLit: {
            auto* n = static_cast<const IntLitExpr*>(e);
            return llvm::ConstantInt::get(i64Ty(), (uint64_t)n->value, /*signed=*/true);
        }
        case ExprKind::BoolLit: {
            auto* n = static_cast<const BoolLitExpr*>(e);
            return llvm::ConstantInt::get(i64Ty(), n->value ? 1 : 0);
        }
        case ExprKind::NoneLit:
            return llvm::ConstantInt::get(i64Ty(), 0);

        case ExprKind::FloatLit:
            // Floats arrive in 6E.
            throw std::runtime_error("native: float literals not yet supported");

        case ExprKind::StringLit:
            // Strings arrive in 6E.
            throw std::runtime_error("native: string literals not yet supported");

        case ExprKind::NameRef: {
            auto* n = static_cast<const NameRefExpr*>(e);
            auto it = varSlots.find(n->name);
            if (it == varSlots.end()) {
                throw std::runtime_error(
                    "native: '" + n->name + "' is not defined");
            }
            return builder->CreateLoad(i64Ty(), it->second, n->name);
        }

        case ExprKind::Grouping:
            return emitExpr(static_cast<const GroupingExpr*>(e)->inner.get());

        case ExprKind::Unary: {
            auto* n = static_cast<const UnaryExpr*>(e);
            llvm::Value* v = emitExpr(n->operand.get());
            switch (n->op) {
            case UnOp::Neg:
                return builder->CreateSub(
                    llvm::ConstantInt::get(i64Ty(), 0), v, "neg");
            case UnOp::Pos:
                return v;
            case UnOp::Not: {
                // !v  →  v == 0  →  zext to i64
                llvm::Value* cmp = builder->CreateICmpEQ(
                    v, llvm::ConstantInt::get(i64Ty(), 0), "not");
                return builder->CreateZExt(cmp, i64Ty(), "not.i64");
            }
            }
            return v;
        }

        case ExprKind::Binary: {
            auto* n = static_cast<const BinaryExpr*>(e);

            if (n->op == BinOp::And) {
                // Short-circuit: result is 1 if both are non-zero.
                llvm::Value* a = emitExpr(n->lhs.get());
                llvm::Value* b = emitExpr(n->rhs.get());
                llvm::Value* an = builder->CreateICmpNE(
                    a, llvm::ConstantInt::get(i64Ty(), 0));
                llvm::Value* bn = builder->CreateICmpNE(
                    b, llvm::ConstantInt::get(i64Ty(), 0));
                llvm::Value* both = builder->CreateAnd(an, bn);
                return builder->CreateZExt(both, i64Ty(), "and.i64");
            }
            if (n->op == BinOp::Or) {
                llvm::Value* a = emitExpr(n->lhs.get());
                llvm::Value* b = emitExpr(n->rhs.get());
                llvm::Value* an = builder->CreateICmpNE(
                    a, llvm::ConstantInt::get(i64Ty(), 0));
                llvm::Value* bn = builder->CreateICmpNE(
                    b, llvm::ConstantInt::get(i64Ty(), 0));
                llvm::Value* either = builder->CreateOr(an, bn);
                return builder->CreateZExt(either, i64Ty(), "or.i64");
            }

            llvm::Value* l = emitExpr(n->lhs.get());
            llvm::Value* r = emitExpr(n->rhs.get());

            switch (n->op) {
            case BinOp::Add:      return builder->CreateAdd(l, r, "add");
            case BinOp::Sub:      return builder->CreateSub(l, r, "sub");
            case BinOp::Mul:      return builder->CreateMul(l, r, "mul");
            case BinOp::Div:
            case BinOp::FloorDiv: return builder->CreateSDiv(l, r, "div");
            case BinOp::Mod:      return builder->CreateSRem(l, r, "rem");
            case BinOp::Pow:
                throw std::runtime_error(
                    "native: '**' on int not yet lowered (needs runtime helper)");

            case BinOp::Eq: {
                llvm::Value* c = builder->CreateICmpEQ(l, r, "eq");
                return builder->CreateZExt(c, i64Ty(), "eq.i64");
            }
            case BinOp::NotEq: {
                llvm::Value* c = builder->CreateICmpNE(l, r, "ne");
                return builder->CreateZExt(c, i64Ty(), "ne.i64");
            }
            case BinOp::Lt: {
                llvm::Value* c = builder->CreateICmpSLT(l, r, "lt");
                return builder->CreateZExt(c, i64Ty(), "lt.i64");
            }
            case BinOp::Gt: {
                llvm::Value* c = builder->CreateICmpSGT(l, r, "gt");
                return builder->CreateZExt(c, i64Ty(), "gt.i64");
            }
            case BinOp::LtEq: {
                llvm::Value* c = builder->CreateICmpSLE(l, r, "le");
                return builder->CreateZExt(c, i64Ty(), "le.i64");
            }
            case BinOp::GtEq: {
                llvm::Value* c = builder->CreateICmpSGE(l, r, "ge");
                return builder->CreateZExt(c, i64Ty(), "ge.i64");
            }

            case BinOp::In:
            case BinOp::Is:
                throw std::runtime_error(
                    std::string("native: operator '") +
                    binOpName(n->op) + "' not yet supported");

            case BinOp::And: case BinOp::Or: break;
            }
            return llvm::ConstantInt::get(i64Ty(), 0);
        }

        case ExprKind::Call: {
            auto* n = static_cast<const CallExpr*>(e);

            // Only plain-name calls for now.
            if (n->callee->kind != ExprKind::NameRef)
                throw std::runtime_error(
                    "native: only plain-name calls are supported in 6B/6C");

            const auto* nm = static_cast<const NameRefExpr*>(n->callee.get());

            // print(...) — dispatch on the shape of the argument.
            if (nm->name == "print") {
                if (n->args.size() != 1)
                    throw std::runtime_error(
                        "native: print() with " +
                        std::to_string(n->args.size()) +
                        " args not yet supported (only 1 arg)");

                llvm::Value* arg = emitExpr(n->args[0].value.get());

                // Look at the AST shape to decide i64 vs bool.
                bool argIsBool = (n->args[0].value->kind == ExprKind::BoolLit);
                if (!argIsBool && n->args[0].value->kind == ExprKind::Binary) {
                    auto* b = static_cast<const BinaryExpr*>(n->args[0].value.get());
                    switch (b->op) {
                    case BinOp::Eq: case BinOp::NotEq:
                    case BinOp::Lt: case BinOp::Gt:
                    case BinOp::LtEq: case BinOp::GtEq:
                    case BinOp::And: case BinOp::Or:
                        argIsBool = true;
                        break;
                    default: break;
                    }
                }
                if (n->args[0].value->kind == ExprKind::Unary) {
                    auto* u = static_cast<const UnaryExpr*>(n->args[0].value.get());
                    if (u->op == UnOp::Not) argIsBool = true;
                }

                llvm::FunctionCallee callee = argIsBool
                    ? lookupBuiltin("print_bool")
                    : lookupBuiltin("print");

                if (argIsBool) {
                    // Caller pass i64; convert to i1 for vayu_print_bool.
                    llvm::Value* b = builder->CreateICmpNE(
                        arg, llvm::ConstantInt::get(i64Ty(), 0), "print.bool");
                    builder->CreateCall(callee, { b });
                }
                else {
                    builder->CreateCall(callee, { arg });
                }
                return llvm::ConstantInt::get(i64Ty(), 0);
            }

            throw std::runtime_error(
                "native: call to '" + nm->name + "' not yet supported");
        }

        case ExprKind::ListLit:
        case ExprKind::MapLit:
        case ExprKind::Attr:
        case ExprKind::Index:
        case ExprKind::Lambda:
        case ExprKind::GenericType:
            throw std::runtime_error(
                "native: this expression is not yet lowered (arrives in 6D+)");
        }
        return llvm::ConstantInt::get(i64Ty(), 0);
    }

    // ===========================================================================
    // Statement codegen
    // ===========================================================================

    void NativeCompiler::Impl::emitStmt(const Stmt* s) {
        if (!s) return;

        switch (s->kind) {
        case StmtKind::Expr: {
            auto* n = static_cast<const ExprStmt*>(s);
            (void)emitExpr(n->expr.get());
            return;
        }

        case StmtKind::Assign: {
            auto* n = static_cast<const AssignStmt*>(s);
            if (n->target->kind != ExprKind::NameRef)
                throw std::runtime_error(
                    "native: only simple-name assignment is supported");
            const auto* nm = static_cast<const NameRefExpr*>(n->target.get());
            llvm::Value* v = emitExpr(n->value.get());
            auto it = varSlots.find(nm->name);
            if (it == varSlots.end()) {
                // Late-definition: create an alloca on the fly.
                llvm::IRBuilder<> tmp(&currentFn->getEntryBlock(),
                    currentFn->getEntryBlock().begin());
                auto* slot = tmp.CreateAlloca(i64Ty(), nullptr, nm->name);
                tmp.CreateStore(llvm::ConstantInt::get(i64Ty(), 0), slot);
                it = varSlots.emplace(nm->name, slot).first;
            }
            builder->CreateStore(v, it->second);
            return;
        }

        case StmtKind::AnnotAssign: {
            auto* n = static_cast<const AnnotAssignStmt*>(s);
            auto it = varSlots.find(n->name);
            if (it == varSlots.end()) {
                llvm::IRBuilder<> tmp(&currentFn->getEntryBlock(),
                    currentFn->getEntryBlock().begin());
                auto* slot = tmp.CreateAlloca(i64Ty(), nullptr, n->name);
                tmp.CreateStore(llvm::ConstantInt::get(i64Ty(), 0), slot);
                it = varSlots.emplace(n->name, slot).first;
            }
            llvm::Value* v = n->value
                ? emitExpr(n->value.get())
                : llvm::ConstantInt::get(i64Ty(), 0);
            builder->CreateStore(v, it->second);
            return;
        }

        case StmtKind::If: {
            auto* n = static_cast<const IfStmt*>(s);

            llvm::Value* cond = emitExpr(n->cond.get());
            llvm::Value* condBool = builder->CreateICmpNE(
                cond, llvm::ConstantInt::get(i64Ty(), 0), "if.cond");

            llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(
                ctx, "if.then", currentFn);
            llvm::BasicBlock* elseBB = nullptr;
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(
                ctx, "if.end", currentFn);

            bool hasElse = (n->elseBody || !n->elifs.empty());
            if (hasElse) {
                elseBB = llvm::BasicBlock::Create(ctx, "if.else", currentFn);
                builder->CreateCondBr(condBool, thenBB, elseBB);
            }
            else {
                builder->CreateCondBr(condBool, thenBB, endBB);
            }

            // Then
            builder->SetInsertPoint(thenBB);
            emitBlock(n->thenBody);
            builder->CreateBr(endBB);

            // Else / elif chain — desugar elif to nested ifs.
            if (hasElse) {
                builder->SetInsertPoint(elseBB);

                // Build the nested chain right-to-left.
                // For each elif, we synthesize a fresh IfStmt-style block.
                // Simplest implementation: emit elifs as a chain.
                llvm::BasicBlock* curBB = elseBB;
                for (size_t i = 0; i < n->elifs.size(); ++i) {
                    auto& ec = n->elifs[i];

                    llvm::Value* ecCond = emitExpr(ec.cond.get());
                    llvm::Value* ecBool = builder->CreateICmpNE(
                        ecCond, llvm::ConstantInt::get(i64Ty(), 0), "elif.cond");

                    llvm::BasicBlock* ecThen = llvm::BasicBlock::Create(
                        ctx, "elif.then", currentFn);
                    llvm::BasicBlock* ecElse = (i + 1 == n->elifs.size())
                        ? (n->elseBody
                            ? llvm::BasicBlock::Create(ctx, "elif.else", currentFn)
                            : endBB)
                        : llvm::BasicBlock::Create(ctx, "elif.else", currentFn);

                    builder->CreateCondBr(ecBool, ecThen, ecElse);

                    builder->SetInsertPoint(ecThen);
                    emitBlock(ec.body);
                    builder->CreateBr(endBB);

                    builder->SetInsertPoint(ecElse);
                    curBB = ecElse;
                }

                if (n->elseBody) {
                    emitBlock(*n->elseBody);
                    builder->CreateBr(endBB);
                }
                else if (n->elifs.empty()) {
                    // no else, no elifs — plain if with an else? shouldn't happen
                    builder->CreateBr(endBB);
                }
            }

            builder->SetInsertPoint(endBB);
            return;
        }

        case StmtKind::While: {
            auto* n = static_cast<const WhileStmt*>(s);

            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(
                ctx, "while.cond", currentFn);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(
                ctx, "while.body", currentFn);
            llvm::BasicBlock* endBB = llvm::BasicBlock::Create(
                ctx, "while.end", currentFn);

            builder->CreateBr(condBB);

            builder->SetInsertPoint(condBB);
            llvm::Value* cond = emitExpr(n->cond.get());
            llvm::Value* condBool = builder->CreateICmpNE(
                cond, llvm::ConstantInt::get(i64Ty(), 0), "while.cond");
            builder->CreateCondBr(condBool, bodyBB, endBB);

            builder->SetInsertPoint(bodyBB);
            emitBlock(n->body);
            builder->CreateBr(condBB);

            builder->SetInsertPoint(endBB);
            return;
        }

        case StmtKind::Pass:
            return;

        case StmtKind::Struct:
        case StmtKind::Class:
            // Type declarations have no runtime footprint in 6A/B/C.
            return;

        case StmtKind::For:
        case StmtKind::Def:
        case StmtKind::Return:
        case StmtKind::Break:
        case StmtKind::Continue:
        case StmtKind::Try:
        case StmtKind::Raise:
        case StmtKind::Import:
        case StmtKind::FromImport:
            throw std::runtime_error(
                "native: this statement is not yet lowered (arrives in 6D+)");
        }
    }

    void NativeCompiler::Impl::emitBlock(const Block& b) {
        for (auto& s : b.stmts) emitStmt(s.get());
    }

    // ===========================================================================
    // Public entry points
    // ===========================================================================

    NativeCompiler::NativeCompiler() : impl_(std::make_unique<Impl>()) {}
    NativeCompiler::~NativeCompiler() = default;

    static void buildModule(NativeCompiler::Impl& I, const Block& program) {
        I.module = std::make_unique<llvm::Module>("vayu_module", I.ctx);
        I.varSlots.clear();

        // Signature: i64 @vayu_main()  (we ignore the return value)
        llvm::FunctionType* fnTy = llvm::FunctionType::get(
            I.i64Ty(), /*params=*/{}, /*isVarArg=*/false);
        I.currentFn = llvm::Function::Create(
            fnTy, llvm::GlobalValue::ExternalLinkage, "vayu_main", I.module.get());

        llvm::BasicBlock* entry = llvm::BasicBlock::Create(
            I.ctx, "entry", I.currentFn);

        llvm::IRBuilder<> builder(entry);
        I.builder = &builder;

        I.collectVariables(program);

        I.emitBlock(program);

        builder.CreateRet(llvm::ConstantInt::get(I.i64Ty(), 0));

        // Verify — catches malformed IR at compile time.
        std::string verifyErr;
        llvm::raw_string_ostream vstream(verifyErr);
        if (llvm::verifyModule(*I.module, &vstream)) {
            throw std::runtime_error("native: LLVM module verification failed:\n" +
                vstream.str());
        }
    }

    int NativeCompiler::compileAndRun(const Block& program) {
        lastError_.clear();
        try {
            buildModule(*impl_, program);
            impl_->ensureJIT();

            auto tsm = llvm::orc::ThreadSafeModule(
                std::move(impl_->module),
                std::make_unique<llvm::LLVMContext>());
            // NB: we move `impl_->ctx` out — subsequent compilations need a
            // fresh context.  For a JIT-per-invocation this is fine.
            // Reset ctx by moving from a new one:
            impl_->ctx = llvm::LLVMContext();

            if (auto err = impl_->jit->addIRModule(std::move(tsm))) {
                std::string msg;
                llvm::handleAllErrors(std::move(err),
                    [&](llvm::ErrorInfoBase& e) { msg = e.message(); });
                lastError_ = "JIT add failed: " + msg;
                return 1;
            }

            auto sym = impl_->jit->lookup("vayu_main");
            if (!sym) {
                std::string msg;
                llvm::handleAllErrors(sym.takeError(),
                    [&](llvm::ErrorInfoBase& e) { msg = e.message(); });
                lastError_ = "JIT lookup failed: " + msg;
                return 1;
            }

            using MainFn = int64_t(*)();
            auto fn = sym->toPtr<MainFn>();
            fn();
            return 0;
        }
        catch (const std::exception& e) {
            lastError_ = e.what();
            return 1;
        }
    }

    void NativeCompiler::dumpIR(const Block& program) {
        lastError_.clear();
        try {
            buildModule(*impl_, program);
            impl_->module->print(llvm::outs(), nullptr);
        }
        catch (const std::exception& e) {
            lastError_ = e.what();
        }
    }

} // namespace vayu