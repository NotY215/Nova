#include "NativeCompiler.hpp"
#include "parser/Parser.hpp"
#include "lexer/Lexer.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#  define _CRT_NONSTDC_NO_DEPRECATE
#  include <process.h>
#  define VAYU_GETPID() _getpid()
#else
#  include <unistd.h>
#  define VAYU_GETPID() getpid()
#endif

namespace vayu {

    NativeCompiler::NativeCompiler() {
#ifdef _WIN32
        qbePath_ = "tools\\qbe.exe";
        ccPath_ = "gcc";
        qbeTarget_ = "amd64_win";
#else
        qbePath_ = "tools/qbe";
        ccPath_ = "cc";
        qbeTarget_ = "amd64_sysv";
#endif
        if (const char* p = std::getenv("VAYU_QBE"))        qbePath_ = p;
        if (const char* p = std::getenv("VAYU_CC"))         ccPath_ = p;
        if (const char* p = std::getenv("VAYU_QBE_TARGET")) qbeTarget_ = p;
    }

    namespace {

        enum class VType { Int, Bool, Str, List, Map, Obj, Exc, Void, Unknown };

        struct VarInfo {
            VType       type = VType::Unknown;
            std::string clsName;
            VType       elemType = VType::Unknown;
            std::string elemClsName;
            VType       valType = VType::Unknown;
        };

        struct ClassInfo {
            std::string                                       name;
            std::string                                       parentName;
            std::vector<std::string>                          fields;
            std::unordered_map<std::string, int>              fieldOffsets;
            int                                               totalSize = 0;
            const ClassStmt* decl = nullptr;
            std::unordered_map<std::string, const DefStmt*>   methods;
            const ClassInfo* parent = nullptr;
            bool                                              hasInit = false;
            const DefStmt* initDecl = nullptr;
            const ClassInfo* initOwner = nullptr;
        };

        static bool isExceptionName(const std::string& n) {
            return n == "Exception" ||
                n == "ValueError" || n == "TypeError" || n == "RuntimeError" ||
                n == "ZeroDivisionError" || n == "IndexError" || n == "KeyError" ||
                n == "NameError" || n == "AttributeError";
        }

        class QbeEmitter {
        public:
            std::string emit(const Block& program, const std::string& sourceDir) {
                sourceDir_ = sourceDir;
                loadImports(program);

                collectClasses(program);
                for (auto& kv : modules_) collectClasses(kv.second);

                // Best-effort pre-pass.  Correctness no longer depends on this
                // finding every literal — see the final assembly step below.
                collectStringLiterals(program);
                for (auto& kv : modules_) collectStringLiterals(kv.second);
                collectExceptionLiterals();

                // Generate all code.  This may add MORE string literals to
                // strLitData_ as expressions like  map["k"] = "v"  are lowered.
                emitCodeBody(program);

                // Assemble: data section first, then code.  By this point every
                // literal referenced anywhere in the code has been registered.
                std::string result;
                if (!strLitData_.empty()) {
                    result += strLitData_;
                    result += "\n";
                }
                result += out_;
                out_.clear();
                return result;
            }

            void emitCodeBody(const Block& program) {
                // ---------- $vayu_main ----------
                resetFunctionState();
                raw("export function $vayu_main() {");
                raw("@start");

                std::unordered_set<std::string> topVars;
                for (auto& s : program.stmts) {
                    if (s->kind == StmtKind::Def) continue;
                    collectVarsStmt(s.get(), topVars);
                }
                for (auto& n : topVars) {
                    std::string slot = "%" + mangle(n) + "_slot";
                    slots_[n] = slot;
                    line(slot + " =l alloc8 8");
                    line("storel 0, " + slot);
                }

                for (auto& kv : modules_)
                    emitModuleTopLevel(kv.first, kv.second);

                for (auto& s : program.stmts) {
                    if (s->kind == StmtKind::Def) continue;
                    if (s->kind == StmtKind::Struct || s->kind == StmtKind::Class) continue;
                    if (s->kind == StmtKind::Import || s->kind == StmtKind::FromImport) continue;
                    emitStmt(s.get());
                    if (terminated_) break;
                }
                if (!terminated_) line("ret");
                raw("}");
                raw("");

                // ---------- user functions ----------
                for (auto& s : program.stmts) {
                    if (s->kind != StmtKind::Def) continue;
                    emitFunction(static_cast<const DefStmt*>(s.get()), nullptr, "");
                    raw("");
                }
                for (auto& kv : modules_) {
                    std::string prefix = mangle(kv.first) + "_";
                    for (auto& s : kv.second.stmts) {
                        if (s->kind != StmtKind::Def) continue;
                        emitFunction(static_cast<const DefStmt*>(s.get()), nullptr, prefix);
                        raw("");
                    }
                }

                // ---------- methods ----------
                for (auto& kv : classes_) {
                    const ClassInfo& ci = kv.second;
                    for (auto& m : ci.decl->methods) {
                        emitFunction(m.get(), &ci, "");
                        raw("");
                    }
                    emitClassCtor(ci);
                    raw("");
                }
            }

        private:
            std::string out_;
            int  nextTemp_ = 0;
            int  nextLabel_ = 0;
            bool terminated_ = false;

            std::unordered_map<std::string, std::string>    slots_;
            std::unordered_map<std::string, VarInfo>        varInfo_;
            std::string                                     currentClass_;
            std::vector<std::pair<std::string, std::string>> loopStack_;

            std::unordered_map<std::string, ClassInfo> classes_;
            std::unordered_map<std::string, int>       fieldGlobals_;
            int                                        nextFieldOffset_ = 0;

            std::string                                  strLitData_;
            std::unordered_map<std::string, std::string> strLitLabels_;
            int                                          nextStrLitId_ = 0;

            std::string                                       sourceDir_;
            std::unordered_map<std::string, Block>            modules_;
            std::unordered_map<std::string, std::string>      fromImports_;
            std::string                                       currentModulePrefix_;

            void resetFunctionState() {
                nextTemp_ = 0;
                nextLabel_ = 0;
                terminated_ = false;
                slots_.clear();
                varInfo_.clear();
                currentClass_.clear();
                loopStack_.clear();
            }

            std::string newTemp() { return "%t" + std::to_string(nextTemp_++); }
            std::string newLabel(const char* p) {
                return "@" + std::string(p) + std::to_string(nextLabel_++);
            }

            void line(const std::string& s) {
                out_ += "    " + s + "\n";
                if (s.rfind("ret", 0) == 0 ||
                    s.rfind("jmp", 0) == 0 ||
                    s.rfind("jnz", 0) == 0)
                    terminated_ = true;
                else
                    terminated_ = false;
            }
            void raw(const std::string& s) {
                if (!s.empty() && s[0] == '@' && !out_.empty() && out_.back() == '\n') {
                    size_t end = out_.size() - 1;
                    if (end > 0) {
                        size_t prevNl = out_.rfind('\n', end - 1);
                        size_t ls = (prevNl == std::string::npos) ? 0 : prevNl + 1;
                        if (ls < end && out_[ls] == '@') out_ += "    jmp " + s + "\n";
                    }
                }
                out_ += s + "\n";
                if (!s.empty() && s[0] == '@') terminated_ = false;
            }

            static std::string mangle(const std::string& s) {
                std::string r = "v";
                for (char c : s) {
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_')
                        r += c;
                    else
                        r += '_';
                }
                return r;
            }

            // =========================================================================
            // Module loading
            // =========================================================================

            bool findModuleFile(const std::string& name, std::string& pathOut) const {
                if (!sourceDir_.empty()) {
                    std::string p = sourceDir_;
                    if (p.back() != '/' && p.back() != '\\') p += '/';
                    p += name + ".vyu";
                    std::ifstream in(p, std::ios::binary);
                    if (in) { pathOut = p; return true; }
                }
                std::string p = name + ".vyu";
                std::ifstream in(p, std::ios::binary);
                if (in) { pathOut = p; return true; }
                return false;
            }

            void loadImports(const Block& program) {
                for (auto& s : program.stmts) {
                    if (s->kind == StmtKind::Import) {
                        auto* n = static_cast<const ImportStmt*>(s.get());
                        if (!modules_.count(n->moduleName))
                            loadModule(n->moduleName, s->loc);
                    }
                    else if (s->kind == StmtKind::FromImport) {
                        auto* n = static_cast<const FromImportStmt*>(s.get());
                        if (!modules_.count(n->moduleName))
                            loadModule(n->moduleName, s->loc);
                        for (auto& item : n->items) {
                            const std::string& local = item.alias.empty() ? item.name : item.alias;
                            fromImports_[local] = n->moduleName;
                        }
                    }
                }
            }

            void loadModule(const std::string& name, SourceLocation loc) {
                (void)loc;
                std::string path;
                if (!findModuleFile(name, path))
                    throw std::runtime_error("native: cannot find module '" +
                        name + ".vyu'");
                std::ifstream in(path, std::ios::binary);
                if (!in) throw std::runtime_error("native: cannot open " + path);
                std::stringstream ss; ss << in.rdbuf();

                Block mod;
                try {
                    Lexer lexer(ss.str());
                    auto tokens = lexer.tokenize();
                    Parser parser(std::move(tokens));
                    mod = parser.parseProgram();
                }
                catch (const ParseError& e) {
                    throw std::runtime_error("native: parse error in module '" + name +
                        "' at line " + std::to_string(e.loc.line) +
                        ": " + e.what());
                }
                modules_[name] = std::move(mod);
                loadImports(modules_[name]);
            }

            void emitModuleTopLevel(const std::string& modName, const Block& modBlock) {
                std::string prefix = mangle(modName) + "_";

                std::unordered_set<std::string> modVars;
                for (auto& s : modBlock.stmts) {
                    if (s->kind == StmtKind::Def) continue;
                    collectVarsStmt(s.get(), modVars);
                }
                for (auto& n : modVars) {
                    std::string fullName = prefix + n;
                    std::string slot = "%" + fullName + "_slot";
                    slots_[fullName] = slot;
                    line(slot + " =l alloc8 8");
                    line("storel 0, " + slot);
                }

                currentModulePrefix_ = prefix;
                for (auto& s : modBlock.stmts) {
                    if (s->kind == StmtKind::Def) continue;
                    if (s->kind == StmtKind::Struct || s->kind == StmtKind::Class) continue;
                    if (s->kind == StmtKind::Import || s->kind == StmtKind::FromImport) continue;
                    emitStmt(s.get());
                }
                currentModulePrefix_.clear();
            }

            // =========================================================================
            // String literals
            // =========================================================================

            std::string internString(const std::string& s) {
                auto it = strLitLabels_.find(s);
                if (it != strLitLabels_.end()) return it->second;

                std::string label = "$vayu_strlit_" + std::to_string(nextStrLitId_++);
                strLitLabels_[s] = label;

                std::ostringstream d;
                d << "data " << label << " = { l " << s.size() << ", ";
                std::string run;
                auto flushRun = [&]() {
                    if (run.empty()) return;
                    d << "b \"" << run << "\", ";
                    run.clear();
                    };
                for (unsigned char c : s) {
                    if (c >= 32 && c < 127 && c != '"' && c != '\\') run += (char)c;
                    else { flushRun(); d << "b " << (int)c << ", "; }
                }
                flushRun();
                d << "b 0 }";
                strLitData_ += d.str() + "\n";
                return label;
            }

            void collectStringLiterals(const Block& b) {
                for (auto& s : b.stmts) collectStringLiteralsStmt(s.get());
            }
            void collectStringLiteralsStmt(const Stmt* s) {
                if (!s) return;
                switch (s->kind) {
                case StmtKind::Expr:
                    collectStringLiteralsExpr(static_cast<const ExprStmt*>(s)->expr.get());
                    break;
                case StmtKind::Assign: {
                    auto* n = static_cast<const AssignStmt*>(s);
                    collectStringLiteralsExpr(n->target.get());
                    collectStringLiteralsExpr(n->value.get());
                    break;
                }
                case StmtKind::AnnotAssign: {
                    auto* n = static_cast<const AnnotAssignStmt*>(s);
                    if (n->type)  collectStringLiteralsExpr(n->type.get());
                    if (n->value) collectStringLiteralsExpr(n->value.get());
                    break;
                }
                case StmtKind::If: {
                    auto* n = static_cast<const IfStmt*>(s);
                    collectStringLiteralsExpr(n->cond.get());
                    collectStringLiterals(n->thenBody);
                    for (auto& e : n->elifs) {
                        collectStringLiteralsExpr(e.cond.get());
                        collectStringLiterals(e.body);
                    }
                    if (n->elseBody) collectStringLiterals(*n->elseBody);
                    break;
                }
                case StmtKind::While: {
                    auto* n = static_cast<const WhileStmt*>(s);
                    collectStringLiteralsExpr(n->cond.get());
                    collectStringLiterals(n->body);
                    break;
                }
                case StmtKind::For: {
                    auto* n = static_cast<const ForStmt*>(s);
                    collectStringLiteralsExpr(n->iterable.get());
                    collectStringLiterals(n->body);
                    break;
                }
                case StmtKind::Def: {
                    auto* n = static_cast<const DefStmt*>(s);
                    collectStringLiterals(n->body);
                    break;
                }
                case StmtKind::Return: {
                    auto* n = static_cast<const ReturnStmt*>(s);
                    if (n->value) collectStringLiteralsExpr(n->value.get());
                    break;
                }
                case StmtKind::Class: {
                    auto* n = static_cast<const ClassStmt*>(s);
                    for (auto& m : n->methods) collectStringLiterals(m->body);
                    break;
                }
                case StmtKind::Try: {
                    auto* n = static_cast<const TryStmt*>(s);
                    collectStringLiterals(n->tryBody);
                    for (auto& h : n->handlers) collectStringLiterals(h.body);
                    if (n->finallyBody) collectStringLiterals(*n->finallyBody);
                    break;
                }
                case StmtKind::Raise: {
                    auto* n = static_cast<const RaiseStmt*>(s);
                    if (n->exception) collectStringLiteralsExpr(n->exception.get());
                    break;
                }
                default: break;
                }
            }
            void collectStringLiteralsExpr(const Expr* e) {
                if (!e) return;
                switch (e->kind) {
                case ExprKind::StringLit: {
                    auto* n = static_cast<const StringLitExpr*>(e);
                    internString(n->value);
                    break;
                }
                case ExprKind::Binary: {
                    auto* n = static_cast<const BinaryExpr*>(e);
                    collectStringLiteralsExpr(n->lhs.get());
                    collectStringLiteralsExpr(n->rhs.get());
                    break;
                }
                case ExprKind::Unary: {
                    auto* n = static_cast<const UnaryExpr*>(e);
                    collectStringLiteralsExpr(n->operand.get());
                    break;
                }
                case ExprKind::Grouping: {
                    auto* n = static_cast<const GroupingExpr*>(e);
                    collectStringLiteralsExpr(n->inner.get());
                    break;
                }
                case ExprKind::Call: {
                    auto* n = static_cast<const CallExpr*>(e);
                    collectStringLiteralsExpr(n->callee.get());
                    for (auto& a : n->args) collectStringLiteralsExpr(a.value.get());
                    break;
                }
                case ExprKind::Index: {
                    auto* n = static_cast<const IndexExpr*>(e);
                    collectStringLiteralsExpr(n->target.get());
                    collectStringLiteralsExpr(n->index.get());
                    break;
                }
                case ExprKind::Attr: {
                    auto* n = static_cast<const AttrExpr*>(e);
                    collectStringLiteralsExpr(n->target.get());
                    break;
                }
                case ExprKind::Lambda: {
                    auto* n = static_cast<const LambdaExpr*>(e);
                    collectStringLiteralsExpr(n->body.get());
                    break;
                }
                case ExprKind::ListLit: {
                    auto* n = static_cast<const ListLitExpr*>(e);
                    for (auto& el : n->elements) collectStringLiteralsExpr(el.get());
                    break;
                }
                case ExprKind::MapLit: {
                    auto* n = static_cast<const MapLitExpr*>(e);
                    for (auto& en : n->entries) {
                        collectStringLiteralsExpr(en.key.get());
                        collectStringLiteralsExpr(en.value.get());
                    }
                    break;
                }
                default: break;
                }
            }

            void collectExceptionLiterals() {
                internString("Exception");
                internString("ValueError");
                internString("TypeError");
                internString("RuntimeError");
                internString("ZeroDivisionError");
                internString("IndexError");
                internString("KeyError");
                internString("NameError");
                internString("AttributeError");
            }

            // =========================================================================
            // Classes
            // =========================================================================

            void collectClasses(const Block& program) {
                // Pass 1: field offsets.
                for (auto& s : program.stmts) {
                    if (s->kind == StmtKind::Class) {
                        auto* n = static_cast<const ClassStmt*>(s.get());
                        for (auto& f : n->fields) allocFieldOffset(f.name);
                        allocFieldOffset("typeName");
                        allocFieldOffset("message");
                    }
                    else if (s->kind == StmtKind::Struct) {
                        auto* n = static_cast<const StructStmt*>(s.get());
                        for (auto& f : n->fields) allocFieldOffset(f.name);
                    }
                }
                // Pass 2: ClassInfo with own fields.
                for (auto& stmt : program.stmts) {
                    if (stmt->kind != StmtKind::Class) continue;
                    auto* n = static_cast<const ClassStmt*>(stmt.get());
                    ClassInfo ci;
                    ci.name = n->name;
                    ci.parentName = n->parentName;
                    ci.decl = n;
                    for (auto& f : n->fields) ci.fields.push_back(f.name);
                    for (auto& m : n->methods) {
                        if (m->name == "__init__") {
                            ci.hasInit = true;
                            ci.initDecl = m.get();
                            continue;
                        }
                        ci.methods[m->name] = m.get();
                    }
                    classes_[n->name] = std::move(ci);
                }
                // Pass 3: parent pointers.
                for (auto it = classes_.begin(); it != classes_.end(); ++it) {
                    ClassInfo& ci = it->second;
                    if (ci.parentName.empty()) continue;
                    auto pit = classes_.find(ci.parentName);
                    if (pit != classes_.end()) ci.parent = &pit->second;
                }
                // Pass 4: inherited fields (fixed point).
                std::unordered_map<std::string, std::vector<std::string>> ownFields;
                for (auto it = classes_.begin(); it != classes_.end(); ++it)
                    ownFields[it->first] = it->second.fields;

                for (int pass = 0; pass < 8; ++pass) {
                    bool changed = false;
                    for (auto it = classes_.begin(); it != classes_.end(); ++it) {
                        ClassInfo& ci = it->second;
                        std::vector<std::string> rebuilt;
                        if (ci.parent)
                            for (auto& f : ci.parent->fields) rebuilt.push_back(f);
                        for (auto& f : ownFields[ci.name]) {
                            bool dup = false;
                            for (auto& r : rebuilt) if (r == f) { dup = true; break; }
                            if (!dup) rebuilt.push_back(f);
                        }
                        if (rebuilt != ci.fields) {
                            ci.fields = std::move(rebuilt);
                            changed = true;
                        }
                    }
                    if (!changed) break;
                }
                // Pass 5a: mark init owner.
                for (auto it = classes_.begin(); it != classes_.end(); ++it) {
                    ClassInfo& ci = it->second;
                    if (ci.hasInit && !ci.initOwner) ci.initOwner = &ci;
                }
                // Pass 5b: propagate __init__ signature.
                for (int pass = 0; pass < 8; ++pass) {
                    bool changed = false;
                    for (auto it = classes_.begin(); it != classes_.end(); ++it) {
                        ClassInfo& ci = it->second;
                        if (ci.hasInit || !ci.parent || !ci.parent->hasInit) continue;
                        ci.hasInit = true;
                        ci.initDecl = ci.parent->initDecl;
                        ci.initOwner = ci.parent->initOwner;
                        changed = true;
                    }
                    if (!changed) break;
                }
                // Pass 6: offsets + size.
                for (auto it = classes_.begin(); it != classes_.end(); ++it) {
                    ClassInfo& ci = it->second;
                    for (auto& f : ci.fields) ci.fieldOffsets[f] = fieldGlobals_.at(f);
                    int maxOff = -8;
                    for (auto& kv : ci.fieldOffsets)
                        if (kv.second > maxOff) maxOff = kv.second;
                    ci.totalSize = maxOff + 8;
                }
            }

            int allocFieldOffset(const std::string& name) {
                auto it = fieldGlobals_.find(name);
                if (it != fieldGlobals_.end()) return it->second;
                int off = nextFieldOffset_;
                fieldGlobals_[name] = off;
                nextFieldOffset_ += 8;
                return off;
            }

            const ClassInfo* findClass(const std::string& n) const {
                auto it = classes_.find(n);
                return it == classes_.end() ? nullptr : &it->second;
            }

            const ClassInfo* findClassDefiningMethod(const ClassInfo* ci,
                const std::string& name) const {
                for (auto c = ci; c; c = c->parent) {
                    if (name == "__init__") {
                        if (c->hasInit && c->initOwner == c) return c;
                    }
                    else {
                        if (c->methods.count(name)) return c;
                    }
                }
                return nullptr;
            }

            static void collectVarsStmt(const Stmt* s, std::unordered_set<std::string>& out);
            static void collectVarsBlock(const Block& b, std::unordered_set<std::string>& out);

            // =========================================================================
            // Expressions
            // =========================================================================

            struct Val {
                std::string ssa;
                VType       type = VType::Unknown;
                std::string cls;
                VType       elemType = VType::Unknown;
                std::string elemCls;
                VType       valType = VType::Unknown;
            };

            static int kindOf(VType t) {
                switch (t) {
                case VType::Int:  return 0;
                case VType::Bool: return 1;
                case VType::Str:  return 2;
                default:          return 0;
                }
            }

            Val emitExpr(const Expr* e) {
                Val r;
                if (!e) { r.ssa = "0"; r.type = VType::Int; return r; }

                switch (e->kind) {
                case ExprKind::IntLit: {
                    auto* n = static_cast<const IntLitExpr*>(e);
                    r.ssa = std::to_string(n->value);
                    r.type = VType::Int;
                    return r;
                }
                case ExprKind::BoolLit: {
                    auto* n = static_cast<const BoolLitExpr*>(e);
                    r.ssa = n->value ? "1" : "0";
                    r.type = VType::Bool;
                    return r;
                }
                case ExprKind::NoneLit:
                    r.ssa = "0"; r.type = VType::Int; return r;

                case ExprKind::StringLit: {
                    auto* n = static_cast<const StringLitExpr*>(e);
                    r.ssa = internString(n->value);
                    r.type = VType::Str;
                    return r;
                }

                case ExprKind::NameRef: {
                    auto* n = static_cast<const NameRefExpr*>(e);
                    const std::string& name = n->name;

                    std::string lookup = name;
                    auto fi = fromImports_.find(name);
                    if (fi != fromImports_.end()) {
                        lookup = mangle(fi->second) + "_" + name;
                    }
                    else if (!currentModulePrefix_.empty()) {
                        lookup = currentModulePrefix_ + name;
                    }

                    auto sit = slots_.find(lookup);
                    if (sit == slots_.end() && lookup != name) sit = slots_.find(name);
                    if (sit == slots_.end())
                        throw std::runtime_error(
                            "native: variable '" + name + "' not declared");
                    std::string t = newTemp();
                    line(t + " =l loadl " + sit->second);
                    r.ssa = t;
                    auto vi = varInfo_.find(lookup);
                    if (vi == varInfo_.end()) vi = varInfo_.find(name);
                    if (vi != varInfo_.end()) {
                        r.type = vi->second.type;
                        r.cls = vi->second.clsName;
                        r.elemType = vi->second.elemType;
                        r.elemCls = vi->second.elemClsName;
                        r.valType = vi->second.valType;
                    }
                    return r;
                }

                case ExprKind::Grouping:
                    return emitExpr(static_cast<const GroupingExpr*>(e)->inner.get());

                case ExprKind::Unary: {
                    auto* n = static_cast<const UnaryExpr*>(e);
                    Val v = emitExpr(n->operand.get());
                    switch (n->op) {
                    case UnOp::Pos: return v;
                    case UnOp::Neg: {
                        std::string t = newTemp();
                        line(t + " =l sub 0, " + v.ssa);
                        r.ssa = t; r.type = VType::Int; return r;
                    }
                    case UnOp::Not: {
                        std::string c = newTemp();
                        line(c + " =w ceql " + v.ssa + ", 0");
                        std::string ext = newTemp();
                        line(ext + " =l extsw " + c);
                        r.ssa = ext; r.type = VType::Bool; return r;
                    }
                    }
                    return v;
                }

                case ExprKind::Binary: {
                    auto* n = static_cast<const BinaryExpr*>(e);

                    if (n->op == BinOp::And || n->op == BinOp::Or) {
                        Val a = emitExpr(n->lhs.get());
                        Val b = emitExpr(n->rhs.get());
                        std::string ab = newTemp(); line(ab + " =w cnel " + a.ssa + ", 0");
                        std::string bb = newTemp(); line(bb + " =w cnel " + b.ssa + ", 0");
                        std::string w = newTemp();
                        line(w + " =w " + (n->op == BinOp::And ? "and" : "or") +
                            " " + ab + ", " + bb);
                        std::string ext = newTemp();
                        line(ext + " =l extsw " + w);
                        r.ssa = ext; r.type = VType::Bool; return r;
                    }

                    Val a = emitExpr(n->lhs.get());
                    Val b = emitExpr(n->rhs.get());

                    bool strAdd = (n->op == BinOp::Add) &&
                        (a.type == VType::Str || b.type == VType::Str);
                    if (strAdd) {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_concat(l " + a.ssa + ", l " + b.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return r;
                    }

                    bool strCmp = (n->op == BinOp::Eq || n->op == BinOp::NotEq) &&
                        (a.type == VType::Str || b.type == VType::Str);
                    if (strCmp) {
                        const char* fn = (n->op == BinOp::Eq) ? "$vayu_str_eq" : "$vayu_str_ne";
                        std::string t = newTemp();
                        line(t + " =l call " + fn + "(l " + a.ssa + ", l " + b.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return r;
                    }

                    switch (n->op) {
                    case BinOp::Add: {
                        std::string t = newTemp();
                        line(t + " =l add " + a.ssa + ", " + b.ssa);
                        r.ssa = t; r.type = VType::Int; return r;
                    }
                    case BinOp::Sub: {
                        std::string t = newTemp();
                        line(t + " =l sub " + a.ssa + ", " + b.ssa); r.ssa = t; r.type = VType::Int; return r;
                    }
                    case BinOp::Mul: {
                        std::string t = newTemp();
                        line(t + " =l mul " + a.ssa + ", " + b.ssa); r.ssa = t; r.type = VType::Int; return r;
                    }
                    case BinOp::Div: {
                        std::string t = newTemp();
                        line(t + " =l div " + a.ssa + ", " + b.ssa); r.ssa = t; r.type = VType::Int; return r;
                    }
                    case BinOp::FloorDiv: {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_floordiv(l " + a.ssa + ", l " + b.ssa + ")");
                        r.ssa = t; r.type = VType::Int; return r;
                    }
                    case BinOp::Mod: {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_mod(l " + a.ssa + ", l " + b.ssa + ")");
                        r.ssa = t; r.type = VType::Int; return r;
                    }

                    case BinOp::Eq: case BinOp::NotEq:
                    case BinOp::Lt: case BinOp::Gt:
                    case BinOp::LtEq: case BinOp::GtEq: {
                        const char* opName = nullptr;
                        switch (n->op) {
                        case BinOp::Eq:    opName = "ceql";  break;
                        case BinOp::NotEq: opName = "cnel";  break;
                        case BinOp::Lt:    opName = "csltl"; break;
                        case BinOp::Gt:    opName = "csgtl"; break;
                        case BinOp::LtEq:  opName = "cslel"; break;
                        case BinOp::GtEq:  opName = "csgel"; break;
                        default: break;
                        }
                        std::string c = newTemp();
                        line(c + " =w " + opName + " " + a.ssa + ", " + b.ssa);
                        std::string ext = newTemp();
                        line(ext + " =l extsw " + c);
                        r.ssa = ext; r.type = VType::Bool; return r;
                    }

                    case BinOp::In: {
                        if (b.type == VType::List) {
                            std::string t = newTemp();
                            line(t + " =l call $vayu_list_contains(l " + b.ssa +
                                ", l " + a.ssa + ")");
                            r.ssa = t; r.type = VType::Bool; return r;
                        }
                        if (b.type == VType::Map) {
                            std::string t = newTemp();
                            line(t + " =l call $vayu_map_has(l " + b.ssa +
                                ", l " + a.ssa + ")");
                            r.ssa = t; r.type = VType::Bool; return r;
                        }
                        if (b.type == VType::Str && a.type == VType::Str) {
                            std::string t = newTemp();
                            line(t + " =l call $vayu_str_contains(l " + b.ssa +
                                ", l " + a.ssa + ")");
                            r.ssa = t; r.type = VType::Bool; return r;
                        }
                        throw std::runtime_error("native: 'in' unsupported on these types");
                    }
                    case BinOp::Is:
                        throw std::runtime_error("native: 'is' not supported");
                    case BinOp::Pow:
                        throw std::runtime_error("native: '**' not yet lowered");
                    default: break;
                    }
                    throw std::runtime_error("native: unsupported binary op");
                }

                case ExprKind::Call:
                    return emitCall(static_cast<const CallExpr*>(e));

                case ExprKind::Index: {
                    auto* n = static_cast<const IndexExpr*>(e);
                    Val tgt = emitExpr(n->target.get());
                    Val idx = emitExpr(n->index.get());
                    if (tgt.type == VType::List) {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_list_get(l " + tgt.ssa + ", l " + idx.ssa + ")");
                        r.ssa = t;
                        if (tgt.elemType == VType::Unknown) {
                            r.type = VType::Int;   // best-effort fallback
                        }
                        else {
                            r.type = tgt.elemType;
                            r.cls = tgt.elemCls;
                        }
                        return r;
                    }
                    if (tgt.type == VType::Map) {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_map_get(l " + tgt.ssa + ", l " + idx.ssa + ")");
                        r.ssa = t;
                        r.type = tgt.valType == VType::Unknown ? VType::Int : tgt.valType;
                        return r;
                    }
                    if (tgt.type == VType::Str) {
                        // String indexing: return a length-1 VayuStr.
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_char_at(l " + tgt.ssa +
                            ", l " + idx.ssa + ")");
                        r.ssa = t; r.type = VType::Str;
                        return r;
                    }
                    throw std::runtime_error("native: index on unsupported type");
                }

                case ExprKind::Attr: {
                    auto* n = static_cast<const AttrExpr*>(e);

                    if (n->target->kind == ExprKind::NameRef) {
                        const auto* tn = static_cast<const NameRefExpr*>(n->target.get());
                        if (modules_.count(tn->name)) {
                            std::string prefixed = mangle(tn->name) + "_" + n->name;
                            auto sit = slots_.find(prefixed);
                            if (sit != slots_.end()) {
                                std::string t = newTemp();
                                line(t + " =l loadl " + sit->second);
                                r.ssa = t;
                                return r;
                            }
                            r.ssa = prefixed;
                            r.cls = "__fn__";
                            return r;
                        }
                    }

                    Val base = emitExpr(n->target.get());
                    if (base.type == VType::Exc) {
                        int off = (n->name == "message") ? 8 : 0;
                        std::string addr = newTemp();
                        line(addr + " =l add " + base.ssa + ", " + std::to_string(off));
                        std::string t = newTemp();
                        line(t + " =l loadl " + addr);
                        r.ssa = t;
                        r.type = VType::Str;
                        return r;
                    }
                    if (base.type != VType::Obj)
                        throw std::runtime_error(
                            "native: attribute access on non-object (type " +
                            std::to_string((int)base.type) + ") at line " +
                            std::to_string(e->loc.line) + " in " + sourceDir_);
                    auto ci = findClass(base.cls);
                    if (!ci) throw std::runtime_error("native: unknown class '" +
                        base.cls + "' at line " +
                        std::to_string(e->loc.line));
                    auto fit = ci->fieldOffsets.find(n->name);
                    if (fit == ci->fieldOffsets.end())
                        throw std::runtime_error(
                            "native: '" + n->name + "' is not a field of class '" +
                            base.cls + "'");
                    std::string addr = newTemp();
                    line(addr + " =l add " + base.ssa + ", " + std::to_string(fit->second));
                    std::string t = newTemp();
                    line(t + " =l loadl " + addr);
                    r.ssa = t;
                    for (auto c = ci; c; c = c->parent) {
                        if (!c->decl) continue;
                        bool found = false;
                        for (auto& f : c->decl->fields) {
                            if (f.name == n->name) {
                                if (f.type) {
                                    inferFromAnnotation(f.type.get(), r);
                                }
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                    }
                    return r;
                }

                case ExprKind::ListLit: {
                    auto* n = static_cast<const ListLitExpr*>(e);
                    std::string lst = newTemp();
                    line(lst + " =l call $vayu_list_new()");
                    VType elemT = VType::Unknown;
                    std::string elemC;
                    for (auto& el : n->elements) {
                        Val v = emitExpr(el.get());
                        if (elemT == VType::Unknown) {
                            elemT = v.type;
                            elemC = v.cls;
                        }
                        line("call $vayu_list_push(l " + lst + ", l " + v.ssa + ")");
                    }
                    r.ssa = lst; r.type = VType::List;
                    r.elemType = elemT; r.elemCls = elemC;
                    return r;
                }

                case ExprKind::MapLit: {
                    auto* n = static_cast<const MapLitExpr*>(e);
                    std::string m = newTemp();
                    line(m + " =l call $vayu_map_new()");
                    VType valT = VType::Unknown;
                    for (auto& en : n->entries) {
                        Val k = emitExpr(en.key.get());
                        if (k.type != VType::Str)
                            throw std::runtime_error("native: map keys must be strings");
                        Val v = emitExpr(en.value.get());
                        if (valT == VType::Unknown) valT = v.type;
                        line("call $vayu_map_put(l " + m + ", l " + k.ssa +
                            ", l " + v.ssa + ")");
                    }
                    r.ssa = m; r.type = VType::Map; r.valType = valT;
                    return r;
                }

                case ExprKind::FloatLit:
                    throw std::runtime_error("native: floats not yet supported");
                case ExprKind::CharLit:
                    throw std::runtime_error("native: char literals not yet supported");
                case ExprKind::Lambda:
                    throw std::runtime_error("native: lambdas not yet supported");
                case ExprKind::GenericType:
                    throw std::runtime_error("native: generic type used as value");
                }
                return r;
            }

            VType typeOfAnnotation(const Expr* e) {
                if (!e) return VType::Unknown;
                if (e->kind == ExprKind::NameRef) {
                    const auto* n = static_cast<const NameRefExpr*>(e);
                    const std::string& s = n->name;
                    if (s == "int")   return VType::Int;
                    if (s == "bool")  return VType::Bool;
                    if (s == "str")   return VType::Str;
                    if (s == "float") return VType::Int;
                    if (s == "list")  return VType::List;
                    if (s == "map")   return VType::Map;
                    if (classes_.count(s)) return VType::Obj;
                }
                if (e->kind == ExprKind::GenericType) {
                    const auto* g = static_cast<const GenericTypeExpr*>(e);
                    if (g->name == "list") return VType::List;
                    if (g->name == "map")  return VType::Map;
                }
                return VType::Unknown;
            }

            /// Same as typeOfAnnotation, but also fills in `cls`, `elemType`,
            /// `elemCls`, and `valType` for object / generic annotations.
            void inferFromAnnotation(const Expr* e, Val& v) {
                if (!e) return;
                if (e->kind == ExprKind::NameRef) {
                    const auto* n = static_cast<const NameRefExpr*>(e);
                    const std::string& s = n->name;
                    if (s == "int")   v.type = VType::Int;
                    else if (s == "bool")  v.type = VType::Bool;
                    else if (s == "str")   v.type = VType::Str;
                    else if (s == "float") v.type = VType::Int;
                    else if (s == "list")  v.type = VType::List;
                    else if (s == "map")   v.type = VType::Map;
                    else if (classes_.count(s)) {
                        v.type = VType::Obj;
                        v.cls = s;
                    }
                    return;
                }
                if (e->kind == ExprKind::GenericType) {
                    const auto* g = static_cast<const GenericTypeExpr*>(e);
                    if (g->name == "list") {
                        v.type = VType::List;
                        if (!g->typeArgs.empty()) {
                            Val inner;
                            inferFromAnnotation(g->typeArgs[0].get(), inner);
                            v.elemType = inner.type;
                            v.elemCls = inner.cls;
                        }
                        return;
                    }
                    if (g->name == "map") {
                        v.type = VType::Map;
                        if (g->typeArgs.size() >= 2) {
                            Val kk, vv;
                            inferFromAnnotation(g->typeArgs[0].get(), kk);
                            inferFromAnnotation(g->typeArgs[1].get(), vv);
                            v.elemType = kk.type;
                            v.elemCls = kk.cls;
                            v.valType = vv.type;
                        }
                        return;
                    }
                }
            }

            std::string classNameOfAnnotation(const Expr* e) {
                if (e && e->kind == ExprKind::NameRef) {
                    const auto* n = static_cast<const NameRefExpr*>(e);
                    if (classes_.count(n->name)) return n->name;
                }
                return {};
            }

            // =========================================================================
            // Builtin methods on list / map / str
            // =========================================================================

            bool tryBuiltinMethod(const std::string& recvName, const Val& recv,
                const CallExpr* call, Val& r) {
                auto argV = [&](size_t i) { return emitExpr(call->args[i].value.get()); };

                if (recv.type == VType::List) {
                    if (recvName == "append") {
                        Val v = argV(0);
                        line("call $vayu_list_push(l " + recv.ssa + ", l " + v.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                    if (recvName == "pop") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_list_pop(l " + recv.ssa + ")");
                        r.ssa = t; r.type = recv.elemType; return true;
                    }
                    if (recvName == "clear") {
                        line("call $vayu_list_clear(l " + recv.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                    if (recvName == "contains") {
                        Val v = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_list_contains(l " + recv.ssa +
                            ", l " + v.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    if (recvName == "insert") {
                        Val i = argV(0), v = argV(1);
                        line("call $vayu_list_insert(l " + recv.ssa + ", l " +
                            i.ssa + ", l " + v.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                    if (recvName == "remove") {
                        Val v = argV(0);
                        line("call $vayu_list_remove(l " + recv.ssa + ", l " + v.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                }
                if (recv.type == VType::Map) {
                    if (recvName == "put") {
                        Val k = argV(0), v = argV(1);
                        line("call $vayu_map_put(l " + recv.ssa + ", l " + k.ssa +
                            ", l " + v.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                    if (recvName == "get") {
                        Val k = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_map_get(l " + recv.ssa + ", l " + k.ssa + ")");
                        r.ssa = t; r.type = recv.valType; return true;
                    }
                    if (recvName == "contains") {
                        Val k = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_map_has(l " + recv.ssa + ", l " + k.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    if (recvName == "remove") {
                        Val k = argV(0);
                        line("call $vayu_map_remove(l " + recv.ssa + ", l " + k.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                    if (recvName == "clear") {
                        line("call $vayu_map_clear(l " + recv.ssa + ")");
                        r.ssa = "0"; r.type = VType::Void; return true;
                    }
                    if (recvName == "keys") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_map_keys(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::List; r.elemType = VType::Str; return true;
                    }
                }
                if (recv.type == VType::Str) {
                    if (recvName == "upper" || recvName == "lower") {
                        const char* fn = (recvName == "upper") ? "$vayu_str_upper" : "$vayu_str_lower";
                        std::string t = newTemp();
                        line(t + " =l call " + fn + "(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return true;
                    }
                    if (recvName == "contains") {
                        Val sub = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_contains(l " + recv.ssa +
                            ", l " + sub.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    if (recvName == "find") {
                        Val sub = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_find(l " + recv.ssa +
                            ", l " + sub.ssa + ")");
                        r.ssa = t; r.type = VType::Int; return true;
                    }
                    if (recvName == "starts_with" || recvName == "ends_with") {
                        Val sub = argV(0);
                        const char* fn = (recvName == "starts_with")
                            ? "$vayu_str_starts_with" : "$vayu_str_ends_with";
                        std::string t = newTemp();
                        line(t + " =l call " + fn + "(l " + recv.ssa + ", l " + sub.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    // ---- Phase 7A: new string methods ----
                    if (recvName == "char_at") {
                        Val i = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_char_at(l " + recv.ssa +
                            ", l " + i.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return true;
                    }
                    if (recvName == "split") {
                        Val sep = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_split(l " + recv.ssa +
                            ", l " + sep.ssa + ")");
                        r.ssa = t; r.type = VType::List; r.elemType = VType::Str;
                        return true;
                    }
                    if (recvName == "replace") {
                        Val o = argV(0), nw = argV(1);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_replace(l " + recv.ssa +
                            ", l " + o.ssa + ", l " + nw.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return true;
                    }
                    if (recvName == "substr") {
                        Val a = argV(0), b = argV(1);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_substr(l " + recv.ssa +
                            ", l " + a.ssa + ", l " + b.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return true;
                    }
                    if (recvName == "strip") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_strip(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return true;
                    }
                    if (recvName == "is_digit") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_is_digit(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    if (recvName == "is_alpha") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_is_alpha(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    if (recvName == "is_space") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_is_space(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::Bool; return true;
                    }
                    if (recvName == "to_int") {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_to_int(l " + recv.ssa + ")");
                        r.ssa = t; r.type = VType::Int; return true;
                    }
                    if (recvName == "join") {
                        Val lst = argV(0);
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_join(l " + recv.ssa +
                            ", l " + lst.ssa + ")");
                        r.ssa = t; r.type = VType::Str; return true;
                    }
                }
                return false;
            }

            // =========================================================================
            // Calls
            // =========================================================================

            Val emitCall(const CallExpr* n) {
                Val r;

                if (n->callee->kind == ExprKind::Attr) {
                    auto* attr = static_cast<const AttrExpr*>(n->callee.get());

                    // ---- super().method(args) ----
                    if (attr->target->kind == ExprKind::Call) {
                        auto* inner = static_cast<const CallExpr*>(attr->target.get());
                        if (inner->callee->kind == ExprKind::NameRef &&
                            inner->args.empty()) {
                            const auto* inm = static_cast<const NameRefExpr*>(
                                inner->callee.get());
                            if (inm->name == "super") {
                                if (currentClass_.empty())
                                    throw std::runtime_error(
                                        "native: super() outside method");
                                const ClassInfo* current = findClass(currentClass_);
                                if (!current)
                                    throw std::runtime_error(
                                        "native: unknown current class '" +
                                        currentClass_ + "'");
                                if (!current->parent)
                                    throw std::runtime_error(
                                        "native: super() called in class '" +
                                        currentClass_ + "' which has no parent");

                                const ClassInfo* defining =
                                    findClassDefiningMethod(current->parent, attr->name);
                                if (!defining)
                                    throw std::runtime_error(
                                        "native: parent class '" +
                                        current->parent->name + "' has no method '" +
                                        attr->name + "'");

                                auto selfIt = slots_.find("self");
                                if (selfIt == slots_.end())
                                    throw std::runtime_error(
                                        "native: super() requires 'self' in scope");

                                std::string selfVal = newTemp();
                                line(selfVal + " =l loadl " + selfIt->second);

                                std::vector<std::string> args;
                                args.push_back(selfVal);
                                for (auto& a : n->args) {
                                    if (!a.name.empty())
                                        throw std::runtime_error(
                                            "native: kwargs not supported");
                                    args.push_back(emitExpr(a.value.get()).ssa);
                                }
                                std::string argsStr;
                                for (size_t i = 0; i < args.size(); ++i) {
                                    if (i) argsStr += ", ";
                                    argsStr += "l " + args[i];
                                }
                                std::string sym = "$vayu_mth_" + mangle(defining->name) +
                                    "_" + mangle(attr->name);
                                std::string t = newTemp();
                                line(t + " =l call " + sym + "(" + argsStr + ")");
                                r.ssa = t;

                                if (attr->name == "__init__") {
                                    r.type = VType::Void;
                                }
                                else {
                                    auto mit = defining->methods.find(attr->name);
                                    if (mit != defining->methods.end() &&
                                        mit->second->returnType) {
                                        Val tmp;
                                        inferFromAnnotation(
                                            mit->second->returnType.get(), tmp);
                                        r.type = tmp.type;
                                        r.cls = tmp.cls;
                                        r.elemType = tmp.elemType;
                                        r.elemCls = tmp.elemCls;
                                        r.valType = tmp.valType;
                                    }
                                    else {
                                        r.type = VType::Unknown;
                                    }
                                }
                                return r;
                            }
                        }
                    }

                    Val recv = emitExpr(attr->target.get());

                    if (tryBuiltinMethod(attr->name, recv, n, r)) return r;

                    if (recv.type == VType::Obj) {
                        auto ci = findClass(recv.cls);
                        if (!ci) throw std::runtime_error("native: unknown class");
                        const ClassInfo* defining =
                            findClassDefiningMethod(ci, attr->name);
                        if (!defining)
                            throw std::runtime_error(
                                "native: class '" + recv.cls + "' has no method '" +
                                attr->name + "'");

                        std::vector<std::string> args;
                        args.push_back(recv.ssa);
                        for (auto& a : n->args) {
                            if (!a.name.empty())
                                throw std::runtime_error(
                                    "native: kwargs not supported");
                            args.push_back(emitExpr(a.value.get()).ssa);
                        }
                        std::string argsStr;
                        for (size_t i = 0; i < args.size(); ++i) {
                            if (i) argsStr += ", ";
                            argsStr += "l " + args[i];
                        }
                        std::string sym = "$vayu_mth_" + mangle(defining->name) +
                            "_" + mangle(attr->name);
                        std::string t = newTemp();
                        line(t + " =l call " + sym + "(" + argsStr + ")");
                        r.ssa = t;
                        if (attr->name == "__init__") {
                            r.type = VType::Void;
                        }
                        else {
                            auto mit = defining->methods.find(attr->name);
                            if (mit != defining->methods.end() &&
                                mit->second->returnType) {
                                Val tmp;
                                inferFromAnnotation(mit->second->returnType.get(), tmp);
                                r.type = tmp.type;
                                r.cls = tmp.cls;
                                r.elemType = tmp.elemType;
                                r.elemCls = tmp.elemCls;
                                r.valType = tmp.valType;
                            }
                            else {
                                r.type = VType::Unknown;
                            }
                        }
                        return r;
                    }

                    if (attr->target->kind == ExprKind::NameRef) {
                        const auto* tn = static_cast<const NameRefExpr*>(
                            attr->target.get());
                        if (modules_.count(tn->name)) {
                            std::vector<std::string> args;
                            for (auto& a : n->args)
                                args.push_back(emitExpr(a.value.get()).ssa);
                            std::string argsStr;
                            for (size_t i = 0; i < args.size(); ++i) {
                                if (i) argsStr += ", ";
                                argsStr += "l " + args[i];
                            }
                            std::string sym = "$vayu_fn_" + mangle(tn->name) + "_" +
                                mangle(attr->name);
                            std::string t = newTemp();
                            line(t + " =l call " + sym + "(" + argsStr + ")");
                            r.ssa = t; r.type = VType::Unknown;
                            return r;
                        }
                    }

                    throw std::runtime_error(
                        "native: method call on unsupported value");
                }

                if (n->callee->kind != ExprKind::NameRef)
                    throw std::runtime_error("native: indirect calls not supported");

                const auto* nm = static_cast<const NameRefExpr*>(n->callee.get());
                std::string name = nm->name;

                if (name == "print") {
                    if (n->args.empty()) {
                        line("call $vayu_print_ln()");
                        r.ssa = "0"; r.type = VType::Void; return r;
                    }
                    for (size_t i = 0; i < n->args.size(); ++i) {
                        Val v = emitExpr(n->args[i].value.get());
                        if (i > 0) line("call $vayu_print_space()");
                        switch (v.type) {
                        case VType::Bool:
                            line("call $vayu_print_bool_noln(l " + v.ssa + ")");
                            break;
                        case VType::Str:
                            line("call $vayu_print_str_noln(l " + v.ssa + ")");
                            break;
                        case VType::List:
                            line("call $vayu_print_list_noln(l " + v.ssa + ", l " +
                                std::to_string(kindOf(v.elemType)) + ")");
                            break;
                        case VType::Map:
                            line("call $vayu_print_map_noln(l " + v.ssa + ", l " +
                                std::to_string(kindOf(v.valType)) + ")");
                            break;
                        default:
                            line("call $vayu_print_int_noln(l " + v.ssa + ")");
                            break;
                        }
                    }
                    line("call $vayu_print_ln()");
                    r.ssa = "0"; r.type = VType::Void; return r;
                }

                if (name == "len") {
                    Val v = emitExpr(n->args[0].value.get());
                    int kind = -1;
                    switch (v.type) {
                    case VType::Str:  kind = 0; break;
                    case VType::List: kind = 1; break;
                    case VType::Map:  kind = 2; break;
                    default: throw std::runtime_error(
                        "native: len() of unsupported type");
                    }
                    std::string t = newTemp();
                    line(t + " =l call $vayu_len(l " + v.ssa + ", l " +
                        std::to_string(kind) + ")");
                    r.ssa = t; r.type = VType::Int;
                    return r;
                }

                if (name == "str") {
                    Val v = emitExpr(n->args[0].value.get());
                    if (v.type == VType::Str) return v;
                    if (v.type == VType::Int || v.type == VType::Bool) {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_int_to_str(l " + v.ssa + ", l " +
                            std::to_string(kindOf(v.type)) + ")");
                        r.ssa = t; r.type = VType::Str;
                        return r;
                    }
                    throw std::runtime_error(
                        "native: str() unsupported on this type");
                }
                if (name == "int") {
                    Val v = emitExpr(n->args[0].value.get());
                    if (v.type == VType::Int)  return v;
                    if (v.type == VType::Bool) return v;   // bools are 0/1 as i64
                    if (v.type == VType::Str) {
                        std::string t = newTemp();
                        line(t + " =l call $vayu_str_to_int(l " + v.ssa + ")");
                        r.ssa = t; r.type = VType::Int;
                        return r;
                    }
                    throw std::runtime_error(
                        "native: int() unsupported on this type");
                }

                if (name == "float") {
                    // No floats in the native backend yet; pass numeric values through.
                    Val v = emitExpr(n->args[0].value.get());
                    return v;
                }

                if (name == "bool") {
                    Val v = emitExpr(n->args[0].value.get());
                    if (v.type == VType::Bool) return v;
                    std::string w = newTemp();
                    line(w + " =w cnel " + v.ssa + ", 0");
                    std::string ext = newTemp();
                    line(ext + " =l extsw " + w);
                    r.ssa = ext; r.type = VType::Bool;
                    return r;
                }

                if (name == "ord") {
                    Val v = emitExpr(n->args[0].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_ord(l " + v.ssa + ")");
                    r.ssa = t; r.type = VType::Int;
                    return r;
                }

                if (name == "chr") {
                    Val v = emitExpr(n->args[0].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_chr(l " + v.ssa + ")");
                    r.ssa = t; r.type = VType::Str;
                    return r;
                }

                if (name == "read_file") {
                    Val p = emitExpr(n->args[0].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_read_file(l " + p.ssa + ")");
                    r.ssa = t; r.type = VType::Str;
                    return r;
                }
                if (name == "read_line") {
                    std::string t = newTemp();
                    line(t + " =l call $vayu_read_line()");
                    r.ssa = t; r.type = VType::Str;
                    return r;
                }

                if (name == "read_all") {
                    std::string t = newTemp();
                    line(t + " =l call $vayu_read_all()");
                    r.ssa = t; r.type = VType::Str;
                    return r;
                }

                if (name == "read_int") {
                    std::string t = newTemp();
                    line(t + " =l call $vayu_read_int()");
                    r.ssa = t; r.type = VType::Int;
                    return r;
                }
                if (name == "input") {
                    if (n->args.size() > 1)
                        throw std::runtime_error(
                            "native: input() takes 0 or 1 argument");
                    std::string t = newTemp();
                    if (n->args.empty()) {
                        line(t + " =l call $vayu_input_plain()");
                    }
                    else {
                        Val p = emitExpr(n->args[0].value.get());
                        if (p.type != VType::Str)
                            throw std::runtime_error(
                                "native: input() prompt must be a string");
                        line(t + " =l call $vayu_input_prompt(l " + p.ssa + ")");
                    }
                    r.ssa = t; r.type = VType::Str;
                    return r;
                }

                if (name == "print_raw") {
                    Val v = emitExpr(n->args[0].value.get());
                    if (v.type == VType::Str) {
                        line("call $vayu_print_raw(l " + v.ssa + ")");
                    }
                    else {
                        // Fall back: format to a temp string then print raw.
                        // For simplicity, require a str argument.
                        throw std::runtime_error(
                            "native: print_raw() requires a str argument");
                    }
                    r.ssa = "0"; r.type = VType::Void;
                    return r;
                }

                if (name == "write_file") {
                    Val p = emitExpr(n->args[0].value.get());
                    Val c = emitExpr(n->args[1].value.get());
                    line("call $vayu_write_file(l " + p.ssa + ", l " + c.ssa + ")");
                    r.ssa = "0"; r.type = VType::Void;
                    return r;
                }

                if (name == "file_exists") {
                    Val p = emitExpr(n->args[0].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_file_exists(l " + p.ssa + ")");
                    r.ssa = t; r.type = VType::Bool;
                    return r;
                }

                if (name == "args") {
                    std::string t = newTemp();
                    line(t + " =l call $vayu_get_args()");
                    r.ssa = t; r.type = VType::List; r.elemType = VType::Str;
                    return r;
                }

                if (name == "run_command") {
                    Val c = emitExpr(n->args[0].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_run_command(l " + c.ssa + ")");
                    r.ssa = t; r.type = VType::Int;
                    return r;
                }

                if (name == "exit") {
                    Val c = emitExpr(n->args[0].value.get());
                    line("call $vayu_exit(l " + c.ssa + ")");
                    r.ssa = "0"; r.type = VType::Void;
                    return r;
                }

                if (name == "join") {
                    // join(sep, list) -> str
                    Val sep = emitExpr(n->args[0].value.get());
                    Val lst = emitExpr(n->args[1].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_str_join(l " + sep.ssa +
                        ", l " + lst.ssa + ")");
                    r.ssa = t; r.type = VType::Str;
                    return r;
                }

                if (name == "range")
                    throw std::runtime_error("native: range() only in `for` loops");

                if (isExceptionName(name)) {
                    std::string typeLbl = internString(name);
                    Val msg = emitExpr(n->args[0].value.get());
                    std::string t = newTemp();
                    line(t + " =l call $vayu_mkexc(l " + typeLbl + ", l " + msg.ssa + ")");
                    r.ssa = t; r.type = VType::Exc;
                    return r;
                }

                if (classes_.count(name)) {
                    std::vector<std::string> args;
                    for (auto& a : n->args) {
                        if (!a.name.empty())
                            throw std::runtime_error("native: kwargs not supported");
                        args.push_back(emitExpr(a.value.get()).ssa);
                    }
                    std::string argsStr;
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i) argsStr += ", ";
                        argsStr += "l " + args[i];
                    }
                    std::string sym = "$vayu_ctor_" + mangle(name);
                    std::string t = newTemp();
                    line(t + " =l call " + sym + "(" + argsStr + ")");
                    r.ssa = t; r.type = VType::Obj; r.cls = name;
                    return r;
                }

                auto fi = fromImports_.find(name);
                std::string fnName = name;
                if (fi != fromImports_.end())
                    fnName = mangle(fi->second) + "_" + name;

                std::vector<std::string> args;
                for (auto& a : n->args) {
                    if (!a.name.empty())
                        throw std::runtime_error("native: kwargs not supported");
                    args.push_back(emitExpr(a.value.get()).ssa);
                }
                std::string argsStr;
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i) argsStr += ", ";
                    argsStr += "l " + args[i];
                }
                std::string t = newTemp();
                line(t + " =l call $vayu_fn_" + mangle(fnName) + "(" + argsStr + ")");
                r.ssa = t; r.type = VType::Unknown;
                return r;
            }

            // =========================================================================
            // Statements
            // =========================================================================

            void emitStmt(const Stmt* s) {
                if (!s) return;
                switch (s->kind) {
                case StmtKind::Expr: {
                    auto* n = static_cast<const ExprStmt*>(s);
                    emitExpr(n->expr.get());
                    return;
                }
                case StmtKind::Assign: {
                    auto* n = static_cast<const AssignStmt*>(s);

                    if (n->target->kind == ExprKind::Attr) {
                        auto* attr = static_cast<const AttrExpr*>(n->target.get());
                        Val recv = emitExpr(attr->target.get());
                        Val v = emitExpr(n->value.get());
                        if (recv.type != VType::Obj)
                            throw std::runtime_error(
                                "native: field assign on non-object");
                        auto ci = findClass(recv.cls);
                        if (!ci) throw std::runtime_error(
                            "native: unknown class");
                        auto fit = ci->fieldOffsets.find(attr->name);
                        if (fit == ci->fieldOffsets.end())
                            throw std::runtime_error(
                                "native: '" + attr->name + "' is not a field");
                        std::string addr = newTemp();
                        line(addr + " =l add " + recv.ssa + ", " +
                            std::to_string(fit->second));
                        line("storel " + v.ssa + ", " + addr);
                        return;
                    }
                    if (n->target->kind == ExprKind::Index) {
                        auto* ix = static_cast<const IndexExpr*>(n->target.get());
                        Val tgt = emitExpr(ix->target.get());
                        Val idx = emitExpr(ix->index.get());
                        Val v = emitExpr(n->value.get());
                        if (tgt.type == VType::List) {
                            line("call $vayu_list_set(l " + tgt.ssa + ", l " +
                                idx.ssa + ", l " + v.ssa + ")");
                            return;
                        }
                        if (tgt.type == VType::Map) {
                            line("call $vayu_map_put(l " + tgt.ssa + ", l " +
                                idx.ssa + ", l " + v.ssa + ")");
                            return;
                        }
                        throw std::runtime_error(
                            "native: index-assign on unsupported type");
                    }
                    if (n->target->kind != ExprKind::NameRef)
                        throw std::runtime_error(
                            "native: unsupported assignment target");

                    auto* nm = static_cast<const NameRefExpr*>(n->target.get());
                    std::string slotName = nm->name;
                    auto fi = fromImports_.find(slotName);
                    if (fi != fromImports_.end())
                        slotName = mangle(fi->second) + "_" + nm->name;
                    else if (!currentModulePrefix_.empty())
                        slotName = currentModulePrefix_ + nm->name;

                    Val v = emitExpr(n->value.get());
                    std::string slot = slots_.count(slotName)
                        ? slots_[slotName] : "";
                    if (slot.empty())
                        slot = slots_.count(nm->name) ? slots_[nm->name] : "";
                    if (slot.empty())
                        throw std::runtime_error(
                            "native: variable '" + nm->name + "' not declared");
                    line("storel " + v.ssa + ", " + slot);

                    VarInfo vi;
                    vi.type = v.type;
                    vi.clsName = v.cls;
                    vi.elemType = v.elemType;
                    vi.elemClsName = v.elemCls;
                    vi.valType = v.valType;
                    varInfo_[slotName] = vi;
                    return;
                }
                case StmtKind::AnnotAssign: {
                    auto* n = static_cast<const AnnotAssignStmt*>(s);
                    std::string slotName = n->name;
                    if (!currentModulePrefix_.empty())
                        slotName = currentModulePrefix_ + n->name;
                    std::string slot = slots_.count(slotName)
                        ? slots_[slotName] : "";
                    if (slot.empty())
                        slot = slots_.count(n->name) ? slots_[n->name] : "";
                    if (slot.empty())
                        throw std::runtime_error(
                            "native: variable '" + n->name + "' not declared");
                    Val v; v.ssa = "0"; v.type = VType::Int;
                    if (n->value) v = emitExpr(n->value.get());
                    line("storel " + v.ssa + ", " + slot);
                    VarInfo vi;
                    {
                        Val tmp;
                        inferFromAnnotation(n->type.get(), tmp);
                        vi.type = tmp.type;
                        vi.clsName = tmp.cls;
                        vi.elemType = tmp.elemType;
                        vi.elemClsName = tmp.elemCls;
                        vi.valType = tmp.valType;
                    }
                    if (vi.type == VType::Unknown) {
                        vi.type = v.type; vi.clsName = v.cls;
                        vi.elemType = v.elemType; vi.elemClsName = v.elemCls;
                        vi.valType = v.valType;
                    }
                    varInfo_[slotName] = vi;
                    return;
                }

                case StmtKind::If:     emitIf(static_cast<const IfStmt*>(s));       return;
                case StmtKind::While:  emitWhile(static_cast<const WhileStmt*>(s)); return;
                case StmtKind::For:    emitFor(static_cast<const ForStmt*>(s));     return;
                case StmtKind::Return: emitReturn(static_cast<const ReturnStmt*>(s)); return;
                case StmtKind::Try:    emitTry(static_cast<const TryStmt*>(s));     return;
                case StmtKind::Raise:  emitRaise(static_cast<const RaiseStmt*>(s)); return;

                case StmtKind::Break:
                    if (loopStack_.empty())
                        throw std::runtime_error("'break' outside loop");
                    line("jmp " + loopStack_.back().second);
                    return;
                case StmtKind::Continue:
                    if (loopStack_.empty())
                        throw std::runtime_error("'continue' outside loop");
                    line("jmp " + loopStack_.back().first);
                    return;

                case StmtKind::Pass:   return;
                case StmtKind::Struct:
                case StmtKind::Class:  return;
                case StmtKind::Import:
                case StmtKind::FromImport:
                    return;

                case StmtKind::Def:
                    throw std::runtime_error("native: nested 'def' not supported");
                }
            }

            void emitBlock(const Block& b) {
                for (auto& s : b.stmts) {
                    emitStmt(s.get());
                    if (terminated_) return;
                }
            }

            void emitIf(const IfStmt* n) {
                std::string cond = emitCond(n->cond.get());
                std::string lThen = newLabel("if_then_");
                std::string lElse = newLabel("if_else_");
                std::string lEnd = newLabel("if_end_");
                bool hasElse = n->elseBody.has_value() || !n->elifs.empty();

                if (hasElse) line("jnz " + cond + ", " + lThen + ", " + lElse);
                else         line("jnz " + cond + ", " + lThen + ", " + lEnd);

                raw(lThen);
                emitBlock(n->thenBody);
                if (!terminated_) line("jmp " + lEnd);

                if (hasElse) {
                    raw(lElse);
                    for (size_t i = 0; i < n->elifs.size(); ++i) {
                        auto& ec = n->elifs[i];
                        std::string ecCond = emitCond(ec.cond.get());
                        std::string ecThen = newLabel("elif_then_");
                        std::string ecElse = newLabel("elif_else_");
                        bool lastElif = (i + 1 == n->elifs.size());
                        std::string elseTarget =
                            (lastElif && !n->elseBody) ? lEnd : ecElse;
                        line("jnz " + ecCond + ", " + ecThen + ", " + elseTarget);
                        raw(ecThen);
                        emitBlock(ec.body);
                        if (!terminated_) line("jmp " + lEnd);
                        if (elseTarget == lEnd) break;
                        raw(ecElse);
                    }
                    if (n->elseBody) {
                        emitBlock(*n->elseBody);
                        if (!terminated_) line("jmp " + lEnd);
                    }
                }
                raw(lEnd);
            }

            void emitWhile(const WhileStmt* n) {
                std::string lCond = newLabel("while_cond_");
                std::string lBody = newLabel("while_body_");
                std::string lEnd = newLabel("while_end_");
                line("jmp " + lCond);
                raw(lCond);
                std::string cond = emitCond(n->cond.get());
                line("jnz " + cond + ", " + lBody + ", " + lEnd);
                raw(lBody);
                loopStack_.push_back({ lCond, lEnd });
                emitBlock(n->body);
                loopStack_.pop_back();
                if (!terminated_) line("jmp " + lCond);
                raw(lEnd);
            }

            void emitFor(const ForStmt* n) {
                if (n->iterable->kind == ExprKind::Call) {
                    auto* call = static_cast<const CallExpr*>(n->iterable.get());
                    if (call->callee->kind == ExprKind::NameRef) {
                        const auto* rn = static_cast<const NameRefExpr*>(
                            call->callee.get());
                        if (rn->name == "range") { emitForRange(n, call); return; }
                    }
                }
                emitForList(n);
            }

            void emitForRange(const ForStmt* n, const CallExpr* call) {
                if (call->args.size() < 1 || call->args.size() > 2)
                    throw std::runtime_error("native: range() supports 1 or 2 args");

                std::string savedSlot; bool hadSaved = false;
                {
                    auto it = slots_.find(n->targetName);
                    if (it != slots_.end()) { savedSlot = it->second; hadSaved = true; }
                }
                std::string uniq = std::to_string(nextLabel_++);
                std::string varSlot = "%" + mangle(n->targetName) + "_slot_" + uniq;
                std::string stopSlot = "%" + mangle(n->targetName) + "_stop_" + uniq;
                slots_[n->targetName] = varSlot;
                varInfo_[n->targetName] = VarInfo{ VType::Int };
                line(varSlot + " =l alloc8 8");
                line(stopSlot + " =l alloc8 8");

                if (call->args.size() == 2) {
                    Val s = emitExpr(call->args[0].value.get());
                    line("storel " + s.ssa + ", " + varSlot);
                }
                else line("storel 0, " + varSlot);
                Val stopV = emitExpr(
                    call->args[call->args.size() == 2 ? 1 : 0].value.get());
                line("storel " + stopV.ssa + ", " + stopSlot);

                std::string lBody = newLabel("for_body_");
                std::string lNext = newLabel("for_next_");
                std::string lCond = newLabel("for_cond_");
                std::string lEnd = newLabel("for_end_");
                line("jmp " + lCond);

                raw(lBody);
                loopStack_.push_back({ lNext, lEnd });
                emitBlock(n->body);
                loopStack_.pop_back();

                raw(lNext);
                {
                    std::string i = newTemp(); line(i + " =l loadl " + varSlot);
                    std::string iNew = newTemp(); line(iNew + " =l add " + i + ", 1");
                    line("storel " + iNew + ", " + varSlot);
                }
                raw(lCond);
                {
                    std::string i = newTemp(); line(i + " =l loadl " + varSlot);
                    std::string stop = newTemp(); line(stop + " =l loadl " + stopSlot);
                    std::string c = newTemp();
                    line(c + " =w csltl " + i + ", " + stop);
                    line("jnz " + c + ", " + lBody + ", " + lEnd);
                }
                raw(lEnd);

                if (hadSaved) slots_[n->targetName] = savedSlot;
                else          slots_.erase(n->targetName);
                varInfo_.erase(n->targetName);
            }

            void emitForList(const ForStmt* n) {
                Val iter = emitExpr(n->iterable.get());

                if (iter.type == VType::Map) {
                    std::string keysList = newTemp();
                    line(keysList + " =l call $vayu_map_keys(l " + iter.ssa + ")");
                    iter.ssa = keysList;
                    iter.type = VType::List;
                    iter.elemType = VType::Str;
                    iter.valType = VType::Unknown;
                }

                if (iter.type != VType::List)
                    throw std::runtime_error(
                        "native: `for` requires range(...), a list, or a map");

                std::string savedSlot; bool hadSaved = false;
                {
                    auto it = slots_.find(n->targetName);
                    if (it != slots_.end()) { savedSlot = it->second; hadSaved = true; }
                }
                std::string uniq = std::to_string(nextLabel_++);
                std::string varSlot = "%" + mangle(n->targetName) + "_slot_" + uniq;
                std::string listSlot = "%" + mangle(n->targetName) + "_lst_" + uniq;
                std::string idxSlot = "%" + mangle(n->targetName) + "_idx_" + uniq;
                std::string lenSlot = "%" + mangle(n->targetName) + "_len_" + uniq;
                slots_[n->targetName] = varSlot;

                VarInfo lv;
                lv.type = iter.elemType;
                varInfo_[n->targetName] = lv;

                line(varSlot + " =l alloc8 8");
                line(listSlot + " =l alloc8 8");
                line(idxSlot + " =l alloc8 8");
                line(lenSlot + " =l alloc8 8");
                line("storel " + iter.ssa + ", " + listSlot);
                line("storel 0, " + idxSlot);
                {
                    std::string len = newTemp();
                    line(len + " =l call $vayu_list_len(l " + iter.ssa + ")");
                    line("storel " + len + ", " + lenSlot);
                }

                std::string lBody = newLabel("for_body_");
                std::string lNext = newLabel("for_next_");
                std::string lCond = newLabel("for_cond_");
                std::string lEnd = newLabel("for_end_");
                line("jmp " + lCond);

                raw(lBody);
                loopStack_.push_back({ lNext, lEnd });
                {
                    std::string lst = newTemp(); line(lst + " =l loadl " + listSlot);
                    std::string idx = newTemp(); line(idx + " =l loadl " + idxSlot);
                    std::string el = newTemp();
                    line(el + " =l call $vayu_list_get(l " + lst + ", l " + idx + ")");
                    line("storel " + el + ", " + varSlot);
                }
                emitBlock(n->body);
                loopStack_.pop_back();

                raw(lNext);
                {
                    std::string idx = newTemp(); line(idx + " =l loadl " + idxSlot);
                    std::string nxt = newTemp(); line(nxt + " =l add " + idx + ", 1");
                    line("storel " + nxt + ", " + idxSlot);
                }
                raw(lCond);
                {
                    std::string idx = newTemp(); line(idx + " =l loadl " + idxSlot);
                    std::string len = newTemp(); line(len + " =l loadl " + lenSlot);
                    std::string c = newTemp();
                    line(c + " =w csltl " + idx + ", " + len);
                    line("jnz " + c + ", " + lBody + ", " + lEnd);
                }
                raw(lEnd);

                if (hadSaved) slots_[n->targetName] = savedSlot;
                else          slots_.erase(n->targetName);
                varInfo_.erase(n->targetName);
            }

            void emitReturn(const ReturnStmt* n) {
                if (n->value) {
                    Val v = emitExpr(n->value.get());
                    line("ret " + v.ssa);
                }
                else line("ret 0");
            }

            void emitRaise(const RaiseStmt* n) {
                if (n->exception) {
                    Val v = emitExpr(n->exception.get());
                    if (v.type == VType::Str) {
                        std::string typeLbl = internString("Exception");
                        std::string t = newTemp();
                        line(t + " =l call $vayu_mkexc(l " + typeLbl +
                            ", l " + v.ssa + ")");
                        v.ssa = t;
                    }
                    line("call $vayu_raise(l " + v.ssa + ")");
                }
                else {
                    line("call $vayu_reraise()");
                }
            }

            void emitTry(const TryStmt* n) {
                std::string lTry = newLabel("try_body_");
                std::string lExc = newLabel("try_exc_");
                std::string lEnd = newLabel("try_end_");

                std::string fid = newTemp();
                line(fid + " =l call $vayu_try_push()");
                std::string buf = newTemp();
                line(buf + " =l call $vayu_try_buf(l " + fid + ")");
                std::string rv = newTemp();
                line(rv + " =w call $setjmp(l " + buf + ")");
                line("jnz " + rv + ", " + lExc + ", " + lTry);

                raw(lTry);
                emitBlock(n->tryBody);
                if (!terminated_) {
                    line("call $vayu_try_pop()");
                    line("jmp " + lEnd);
                }

                raw(lExc);
                {
                    std::string et = newTemp();
                    line(et + " =l call $vayu_get_exc_type()");

                    std::vector<std::string> matchLabels;
                    std::vector<std::string> nextLabels;
                    for (size_t hi = 0; hi < n->handlers.size(); ++hi) {
                        matchLabels.push_back(newLabel("catch_match_"));
                        nextLabels.push_back(newLabel("catch_next_"));
                    }
                    std::string lReraise = newLabel("try_reraise_");

                    for (size_t i = 0; i < n->handlers.size(); ++i) {
                        auto& h = n->handlers[i];
                        if (h.exceptionType) {
                            std::string typeName;
                            if (h.exceptionType->kind == ExprKind::NameRef) {
                                typeName = static_cast<const NameRefExpr*>(
                                    h.exceptionType.get())->name;
                            }
                            else {
                                throw std::runtime_error(
                                    "native: exception type must be a class name");
                            }
                            std::string typeLbl = internString(typeName);

                            if (typeName == "Exception") {
                                line("jmp " + matchLabels[i]);
                            }
                            else {
                                std::string cond = newTemp();
                                line(cond + " =w call $vayu_str_eq(l " + et +
                                    ", l " + typeLbl + ")");
                                std::string nextTarget =
                                    (i + 1 < n->handlers.size())
                                    ? nextLabels[i + 1] : lReraise;
                                line("jnz " + cond + ", " + matchLabels[i] +
                                    ", " + nextTarget);
                            }
                        }
                        else {
                            line("jmp " + matchLabels[i]);
                        }
                    }

                    raw(lReraise);
                    line("call $vayu_reraise()");
                    line("jmp " + lEnd);

                    for (size_t i = 0; i < n->handlers.size(); ++i) {
                        auto& h = n->handlers[i];
                        raw(matchLabels[i]);

                        if (!h.varName.empty()) {
                            std::string excVal = newTemp();
                            line(excVal + " =l call $vayu_get_exc_value()");
                            std::string slotName = h.varName;
                            if (!slots_.count(slotName)) {
                                std::string slot = "%" + mangle(slotName) + "_slot";
                                slots_[slotName] = slot;
                                line(slot + " =l alloc8 8");
                            }
                            line("storel " + excVal + ", " + slots_[slotName]);
                            varInfo_[slotName] = VarInfo{ VType::Exc };
                        }

                        emitBlock(h.body);
                        if (!terminated_) {
                            line("call $vayu_try_pop()");
                            line("jmp " + lEnd);
                        }
                    }
                }

                raw(lEnd);
                if (n->finallyBody) emitBlock(*n->finallyBody);
            }

            void emitFunction(const DefStmt* def, const ClassInfo* cls,
                const std::string& prefix) {
                std::string sym;
                if (cls)
                    sym = "$vayu_mth_" + mangle(cls->name) + "_" + mangle(def->name);
                else
                    sym = "$vayu_fn_" + prefix + mangle(def->name);

                std::string params;
                for (size_t i = 0; i < def->params.size(); ++i) {
                    if (i) params += ", ";
                    params += "l %p_" + mangle(def->params[i].name);
                }
                raw("function l " + sym + "(" + params + ") {");
                raw("@start");

                auto savedSlots = slots_;
                auto savedVarInfo = varInfo_;
                auto savedClass = currentClass_;
                auto savedLoop = loopStack_;
                int  savedTemp = nextTemp_;
                int  savedLabel = nextLabel_;
                bool savedTerm = terminated_;
                std::string savedModPrefix = currentModulePrefix_;

                resetFunctionState();
                if (cls) currentClass_ = cls->name;
                if (!prefix.empty()) currentModulePrefix_ = prefix;

                for (size_t pi = 0; pi < def->params.size(); ++pi) {
                    auto& p = def->params[pi];
                    std::string slot = "%" + mangle(p.name) + "_slot";
                    slots_[p.name] = slot;
                    line(slot + " =l alloc8 8");
                    line("storel %p_" + mangle(p.name) + ", " + slot);

                    VarInfo vi;
                    if (cls && pi == 0) {
                        vi.type = VType::Obj;
                        vi.clsName = cls->name;
                    }
                    else if (p.type) {
                        Val tmp;
                        inferFromAnnotation(p.type.get(), tmp);
                        vi.type = tmp.type;
                        vi.clsName = tmp.cls;
                        vi.elemType = tmp.elemType;
                        vi.elemClsName = tmp.elemCls;
                        vi.valType = tmp.valType;
                    }
                    varInfo_[p.name] = vi;
                }

                std::unordered_set<std::string> names;
                collectVarsBlock(def->body, names);
                for (auto& n : names) {
                    if (slots_.count(n)) continue;
                    std::string slot = "%" + mangle(n) + "_slot";
                    slots_[n] = slot;
                    line(slot + " =l alloc8 8");
                    line("storel 0, " + slot);
                }

                emitBlock(def->body);
                if (!terminated_) line("ret 0");
                raw("}");

                slots_ = std::move(savedSlots);
                varInfo_ = std::move(savedVarInfo);
                currentClass_ = std::move(savedClass);
                loopStack_ = std::move(savedLoop);
                nextTemp_ = savedTemp;
                nextLabel_ = savedLabel;
                terminated_ = savedTerm;
                currentModulePrefix_ = std::move(savedModPrefix);
            }

            void emitClassCtor(const ClassInfo& ci) {
                std::string sym = "$vayu_ctor_" + mangle(ci.name);
                const DefStmt* userInit = ci.initDecl;

                std::vector<std::string> paramNames;
                if (userInit) {
                    for (size_t i = 1; i < userInit->params.size(); ++i)
                        paramNames.push_back(userInit->params[i].name);
                }
                else {
                    for (auto& f : ci.fields) paramNames.push_back(f);
                }

                std::string params;
                for (size_t i = 0; i < paramNames.size(); ++i) {
                    if (i) params += ", ";
                    params += "l %p_" + mangle(paramNames[i]);
                }
                raw("function l " + sym + "(" + params + ") {");
                raw("@start");

                auto savedSlots = slots_;
                auto savedVarInfo = varInfo_;
                auto savedClass = currentClass_;
                auto savedLoop = loopStack_;
                int  savedTemp = nextTemp_;
                int  savedLabel = nextLabel_;
                bool savedTerm = terminated_;
                std::string savedModPrefix = currentModulePrefix_;

                resetFunctionState();
                currentClass_ = ci.name;

                std::string self = newTemp();
                line(self + " =l call $vayu_alloc(l " +
                    std::to_string(ci.totalSize) + ")");

                if (userInit && ci.initOwner) {
                    std::string argList = "l " + self;
                    for (auto& p : paramNames)
                        argList += ", l %p_" + mangle(p);
                    std::string initSym = "$vayu_mth_" + mangle(ci.initOwner->name) +
                        "_" + mangle("__init__");
                    line("call " + initSym + "(" + argList + ")");
                }
                else {
                    for (size_t i = 0; i < ci.fields.size(); ++i) {
                        const std::string& f = ci.fields[i];
                        int off = ci.fieldOffsets.at(f);
                        std::string addr = newTemp();
                        line(addr + " =l add " + self + ", " +
                            std::to_string(off));
                        line("storel %p_" + mangle(f) + ", " + addr);
                    }
                }
                line("ret " + self);
                raw("}");

                slots_ = std::move(savedSlots);
                varInfo_ = std::move(savedVarInfo);
                currentClass_ = std::move(savedClass);
                loopStack_ = std::move(savedLoop);
                nextTemp_ = savedTemp;
                nextLabel_ = savedLabel;
                terminated_ = savedTerm;
                currentModulePrefix_ = std::move(savedModPrefix);
            }

            // Emit a branch-condition expression as a `w` SSA value suitable for
            // `jnz`.  When the expression is itself a comparison, we bypass the
            // usual i64 round-trip and emit the compare directly at width `w`.
            std::string emitCond(const Expr* e) {
                if (!e) return "0";

                if (e->kind == ExprKind::Binary) {
                    auto* b = static_cast<const BinaryExpr*>(e);

                    switch (b->op) {
                    case BinOp::Eq: case BinOp::NotEq:
                    case BinOp::Lt: case BinOp::Gt:
                    case BinOp::LtEq: case BinOp::GtEq: {
                        Val a = emitExpr(b->lhs.get());
                        Val c = emitExpr(b->rhs.get());

                        bool strCmp =
                            (a.type == VType::Str || c.type == VType::Str) &&
                            (b->op == BinOp::Eq || b->op == BinOp::NotEq);
                        if (strCmp) {
                            const char* fn = (b->op == BinOp::Eq)
                                ? "$vayu_str_eq" : "$vayu_str_ne";
                            std::string t = newTemp();
                            line(t + " =l call " + fn +
                                "(l " + a.ssa + ", l " + c.ssa + ")");
                            std::string w = newTemp();
                            line(w + " =w cnel " + t + ", 0");
                            return w;
                        }

                        const char* opName = nullptr;
                        switch (b->op) {
                        case BinOp::Eq:    opName = "ceql";  break;
                        case BinOp::NotEq: opName = "cnel";  break;
                        case BinOp::Lt:    opName = "csltl"; break;
                        case BinOp::Gt:    opName = "csgtl"; break;
                        case BinOp::LtEq:  opName = "cslel"; break;
                        case BinOp::GtEq:  opName = "csgel"; break;
                        default: break;
                        }
                        std::string w = newTemp();
                        line(w + " =w " + opName + " " + a.ssa + ", " + c.ssa);
                        return w;
                    }

                    case BinOp::And: {
                        std::string w1 = emitCond(b->lhs.get());
                        std::string w2 = emitCond(b->rhs.get());
                        std::string w = newTemp();
                        line(w + " =w and " + w1 + ", " + w2);
                        return w;
                    }
                    case BinOp::Or: {
                        std::string w1 = emitCond(b->lhs.get());
                        std::string w2 = emitCond(b->rhs.get());
                        std::string w = newTemp();
                        line(w + " =w or " + w1 + ", " + w2);
                        return w;
                    }
                    default: break;
                    }
                }

                if (e->kind == ExprKind::Unary) {
                    auto* u = static_cast<const UnaryExpr*>(e);
                    if (u->op == UnOp::Not) {
                        std::string w = emitCond(u->operand.get());
                        std::string inv = newTemp();
                        line(inv + " =w xor " + w + ", 1");
                        return inv;
                    }
                }

                if (e->kind == ExprKind::BoolLit) {
                    auto* bl = static_cast<const BoolLitExpr*>(e);
                    return bl->value ? "1" : "0";
                }

                Val v = emitExpr(e);
                std::string t = newTemp();
                line(t + " =w cnel " + v.ssa + ", 0");
                return t;
            }
        };

        void QbeEmitter::collectVarsStmt(const Stmt* s,
            std::unordered_set<std::string>& out) {
            if (!s) return;
            switch (s->kind) {
            case StmtKind::Assign: {
                auto* n = static_cast<const AssignStmt*>(s);
                if (n->target->kind == ExprKind::NameRef)
                    out.insert(static_cast<const NameRefExpr*>(
                        n->target.get())->name);
                break;
            }
            case StmtKind::AnnotAssign: {
                auto* n = static_cast<const AnnotAssignStmt*>(s);
                out.insert(n->name);
                break;
            }
            case StmtKind::If: {
                auto* n = static_cast<const IfStmt*>(s);
                collectVarsBlock(n->thenBody, out);
                for (auto& ec : n->elifs) collectVarsBlock(ec.body, out);
                if (n->elseBody) collectVarsBlock(*n->elseBody, out);
                break;
            }
            case StmtKind::While: {
                auto* n = static_cast<const WhileStmt*>(s);
                collectVarsBlock(n->body, out);
                break;
            }
            case StmtKind::For: {
                auto* n = static_cast<const ForStmt*>(s);
                collectVarsBlock(n->body, out);
                break;
            }
            case StmtKind::Try: {
                auto* n = static_cast<const TryStmt*>(s);
                collectVarsBlock(n->tryBody, out);
                for (auto& h : n->handlers) {
                    if (!h.varName.empty()) out.insert(h.varName);
                    collectVarsBlock(h.body, out);
                }
                if (n->finallyBody) collectVarsBlock(*n->finallyBody, out);
                break;
            }
            default: break;
            }
        }
        void QbeEmitter::collectVarsBlock(const Block& b,
            std::unordered_set<std::string>& out) {
            for (auto& s : b.stmts) collectVarsStmt(s.get(), out);
        }

    } // anonymous namespace

    // ===========================================================================
    // Runtime C — Phase 7A adds file I/O, process, args, and string helpers.
    // ===========================================================================

    static const char* kRuntimeC = R"C(
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <ctype.h>

typedef struct { int64_t len; char data[]; } VayuStr;
typedef struct { int64_t len; int64_t cap; int64_t* items; } VayuList;
typedef struct { VayuStr* key; int64_t value; uint8_t used; } VayuMapEntry;
typedef struct { int64_t len; int64_t cap; VayuMapEntry* entries; } VayuMap;
typedef struct { VayuStr* typeName; VayuStr* message; } VayuExc;

void vayu_raise_str(VayuStr* typeName, VayuStr* msg);
int64_t vayu_str_to_int(VayuStr* s);

// ---- process-global argv (set once at startup) ----------------------------
static int    g_argc = 0;
static char** g_argv = NULL;

// ---- allocation -----------------------------------------------------------
void* vayu_alloc(int64_t size) {
    void* p = malloc((size_t)size);
    if (!p) { fprintf(stderr, "vayu: oom\n"); exit(1); }
    return p;
}

static VayuStr* vayu_mkstr(const char* cstr, int64_t n) {
    VayuStr* s = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)n + 1);
    s->len = n;
    if (n > 0) memcpy(s->data, cstr, (size_t)n);
    s->data[n] = 0;
    return s;
}
static VayuStr* vayu_mkstr_c(const char* cstr) {
    return vayu_mkstr(cstr, (int64_t)strlen(cstr));
}

// ---- printing -------------------------------------------------------------
void vayu_print_int(long long v) { printf("%lld\n", v); }
void vayu_print_bool(long long v) { printf("%s\n", v ? "true" : "false"); }
void vayu_print_int_noln(long long v) { printf("%lld", v); }
void vayu_print_bool_noln(long long v) { printf("%s", v ? "true" : "false"); }
void vayu_print_space(void) { putchar(' '); }
void vayu_print_ln(void) { putchar('\n'); }
void vayu_print_str(VayuStr* s) {
    fwrite(s->data, 1, (size_t)s->len, stdout); putchar('\n');
}
void vayu_print_str_noln(VayuStr* s) {
    fwrite(s->data, 1, (size_t)s->len, stdout);
}


// ---- input primitives -----------------------------------------------------
void vayu_print_raw(VayuStr* s) {
    fwrite(s->data, 1, (size_t)s->len, stdout);
    fflush(stdout);
}

VayuStr* vayu_read_line(void) {
    size_t cap = 256, len = 0;
    char* buf = (char*)malloc(cap);
    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (len + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) {
        free(buf);
        return vayu_mkstr("", 0);
    }
    // Strip a trailing \r if present (Windows line endings).
    if (len > 0 && buf[len - 1] == '\r') --len;
    VayuStr* s = (VayuStr*)malloc(sizeof(VayuStr) + len + 1);
    s->len = (int64_t)len;
    memcpy(s->data, buf, len);
    s->data[len] = 0;
    free(buf);
    return s;
}

VayuStr* vayu_read_all(void) {
    size_t cap = 4096, len = 0;
    char* buf = (char*)malloc(cap);
    size_t n;
    while ((n = fread(buf + len, 1, cap - len, stdin)) > 0) {
        len += n;
        if (len == cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
    }
    VayuStr* s = (VayuStr*)malloc(sizeof(VayuStr) + len + 1);
    s->len = (int64_t)len;
    memcpy(s->data, buf, len);
    s->data[len] = 0;
    free(buf);
    return s;
}

int64_t vayu_read_int(void) {
    VayuStr* s = vayu_read_line();
    return vayu_str_to_int(s);
}

// ---- Python-style input ------------------------------------------------
VayuStr* vayu_input_plain(void) {
    return vayu_read_line();
}

VayuStr* vayu_input_prompt(VayuStr* prompt) {
    if (prompt->len) fwrite(prompt->data, 1, (size_t)prompt->len, stdout);
    fflush(stdout);
    return vayu_read_line();
}

// ---- string concat helper for building diagnostic messages ---------------
static VayuStr* vayu_concat_c(const char* prefix, VayuStr* s) {
    int64_t plen = (int64_t)strlen(prefix);
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)(plen + s->len) + 1);
    r->len = plen + s->len;
    if (plen)   memcpy(r->data, prefix, (size_t)plen);
    if (s->len) memcpy(r->data + plen, s->data, (size_t)s->len);
    r->data[r->len] = 0;
    return r;
}

// ---- strings --------------------------------------------------------------
VayuStr* vayu_str_concat(VayuStr* a, VayuStr* b) {
    int64_t n = a->len + b->len;
    VayuStr* s = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)n + 1);
    s->len = n;
    if (a->len) memcpy(s->data, a->data, (size_t)a->len);
    if (b->len) memcpy(s->data + a->len, b->data, (size_t)b->len);
    s->data[n] = 0;
    return s;
}
int64_t vayu_str_eq(VayuStr* a, VayuStr* b) {
    if (a == b) return 1;
    if (a->len != b->len) return 0;
    return memcmp(a->data, b->data, (size_t)a->len) == 0;
}
int64_t vayu_str_ne(VayuStr* a, VayuStr* b) { return !vayu_str_eq(a, b); }
int64_t vayu_str_len(VayuStr* s) { return s->len; }

VayuStr* vayu_int_to_str(long long v, long long kind) {
    char buf[64]; int n;
    if (kind == 1) n = snprintf(buf, sizeof(buf), "%s", v ? "true" : "false");
    else           n = snprintf(buf, sizeof(buf), "%lld", v);
    return vayu_mkstr(buf, n);
}
VayuStr* vayu_str_upper(VayuStr* s) {
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)s->len + 1);
    r->len = s->len;
    for (int64_t i = 0; i < s->len; ++i) {
        char c = s->data[i];
        r->data[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }
    r->data[s->len] = 0;
    return r;
}
VayuStr* vayu_str_lower(VayuStr* s) {
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)s->len + 1);
    r->len = s->len;
    for (int64_t i = 0; i < s->len; ++i) {
        char c = s->data[i];
        r->data[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    r->data[s->len] = 0;
    return r;
}
int64_t vayu_str_contains(VayuStr* s, VayuStr* sub) {
    if (sub->len == 0) return 1;
    if (sub->len > s->len) return 0;
    for (int64_t i = 0; i + sub->len <= s->len; ++i)
        if (memcmp(s->data + i, sub->data, (size_t)sub->len) == 0) return 1;
    return 0;
}
int64_t vayu_str_find(VayuStr* s, VayuStr* sub) {
    if (sub->len == 0) return 0;
    if (sub->len > s->len) return -1;
    for (int64_t i = 0; i + sub->len <= s->len; ++i)
        if (memcmp(s->data + i, sub->data, (size_t)sub->len) == 0) return i;
    return -1;
}
int64_t vayu_str_starts_with(VayuStr* s, VayuStr* p) {
    if (p->len > s->len) return 0;
    return memcmp(s->data, p->data, (size_t)p->len) == 0;
}
int64_t vayu_str_ends_with(VayuStr* s, VayuStr* p) {
    if (p->len > s->len) return 0;
    return memcmp(s->data + (s->len - p->len), p->data, (size_t)p->len) == 0;
}

// ---- NEW: string indexing, slicing, split/join/replace --------------------
VayuStr* vayu_str_char_at(VayuStr* s, int64_t i) {
    if (i < 0) i += s->len;
    if (i < 0 || i >= s->len) {
        vayu_raise_str(vayu_mkstr_c("IndexError"),
                       vayu_mkstr_c("string index out of range"));
    }
    return vayu_mkstr(s->data + i, 1);
}
VayuStr* vayu_str_substr(VayuStr* s, int64_t start, int64_t end) {
    if (start < 0) start += s->len;
    if (end   < 0) end   += s->len;
    if (start < 0) start = 0;
    if (end   > s->len) end = s->len;
    if (end < start) end = start;
    return vayu_mkstr(s->data + start, end - start);
}
VayuStr* vayu_str_replace(VayuStr* s, VayuStr* from, VayuStr* to) {
    if (from->len == 0) {
        // Python returns the original if `from` is empty.
        return vayu_mkstr(s->data, s->len);
    }
    // Count occurrences.
    int64_t count = 0;
    for (int64_t i = 0; i + from->len <= s->len; ) {
        if (memcmp(s->data + i, from->data, (size_t)from->len) == 0) {
            ++count; i += from->len;
        } else ++i;
    }
    int64_t newLen = s->len + count * (to->len - from->len);
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)newLen + 1);
    r->len = newLen;
    int64_t op = 0, ip = 0;
    while (ip < s->len) {
        if (ip + from->len <= s->len &&
            memcmp(s->data + ip, from->data, (size_t)from->len) == 0) {
            if (to->len) memcpy(r->data + op, to->data, (size_t)to->len);
            op += to->len;
            ip += from->len;
        } else {
            r->data[op++] = s->data[ip++];
        }
    }
    r->data[newLen] = 0;
    return r;
}
VayuStr* vayu_str_strip(VayuStr* s) {
    int64_t a = 0, b = s->len;
    while (a < b && (s->data[a]==' '||s->data[a]=='\t'||s->data[a]=='\n'||
                     s->data[a]=='\r'||s->data[a]=='\f'||s->data[a]=='\v')) ++a;
    while (b > a && (s->data[b-1]==' '||s->data[b-1]=='\t'||s->data[b-1]=='\n'||
                     s->data[b-1]=='\r'||s->data[b-1]=='\f'||s->data[b-1]=='\v')) --b;
    return vayu_mkstr(s->data + a, b - a);
}
int64_t vayu_str_is_digit(VayuStr* s) {
    if (s->len == 0) return 0;
    for (int64_t i = 0; i < s->len; ++i) {
        unsigned char c = (unsigned char)s->data[i];
        if (!isdigit(c)) return 0;
    }
    return 1;
}
int64_t vayu_str_is_alpha(VayuStr* s) {
    if (s->len == 0) return 0;
    for (int64_t i = 0; i < s->len; ++i) {
        unsigned char c = (unsigned char)s->data[i];
        if (!isalpha(c)) return 0;
    }
    return 1;
}
int64_t vayu_str_is_space(VayuStr* s) {
    if (s->len == 0) return 0;
    for (int64_t i = 0; i < s->len; ++i) {
        unsigned char c = (unsigned char)s->data[i];
        if (!isspace(c)) return 0;
    }
    return 1;
}
int64_t vayu_str_to_int(VayuStr* s) {
    // Simple decimal parse; leading whitespace allowed.
    int64_t n = 0;
    int64_t i = 0;
    int     neg = 0;
    while (i < s->len && (s->data[i]==' '||s->data[i]=='\t')) ++i;
    if (i < s->len && (s->data[i]=='-'||s->data[i]=='+')) {
        neg = (s->data[i]=='-'); ++i;
    }
    for (; i < s->len; ++i) {
        char c = s->data[i];
        if (c < '0' || c > '9') break;
        n = n * 10 + (c - '0');
    }
    return neg ? -n : n;
}

// ---- NEW: ord/chr --------------------------------------------------------
int64_t vayu_ord(VayuStr* s) {
    if (s->len != 1) {
        vayu_raise_str(vayu_mkstr_c("ValueError"),
                       vayu_mkstr_c("ord() requires a length-1 string"));
    }
    return (int64_t)(unsigned char)s->data[0];
}
VayuStr* vayu_chr(int64_t n) {
    if (n < 0 || n > 255) {
        vayu_raise_str(vayu_mkstr_c("ValueError"),
                       vayu_mkstr_c("chr() argument out of range"));
    }
    char c = (char)n;
    return vayu_mkstr(&c, 1);
}

// ---- lists ----------------------------------------------------------------
VayuList* vayu_list_new() {
    VayuList* l = (VayuList*)malloc(sizeof(VayuList));
    l->len = 0; l->cap = 4;
    l->items = (int64_t*)malloc(sizeof(int64_t) * 4);
    return l;
}
static void vayu_list_grow(VayuList* l) {
    if (l->len < l->cap) return;
    l->cap *= 2;
    l->items = (int64_t*)realloc(l->items, sizeof(int64_t) * (size_t)l->cap);
}
void vayu_list_push(VayuList* l, int64_t v) {
    vayu_list_grow(l);
    l->items[l->len++] = v;
}
int64_t vayu_list_get(VayuList* l, int64_t i) {
    if (i < 0) i += l->len;
    if (i < 0 || i >= l->len) {
        vayu_raise_str(vayu_mkstr_c("IndexError"),
                       vayu_mkstr_c("list index out of range"));
    }
    return l->items[i];
}
void vayu_list_set(VayuList* l, int64_t i, int64_t v) {
    if (i < 0) i += l->len;
    if (i < 0 || i >= l->len) {
        vayu_raise_str(vayu_mkstr_c("IndexError"),
                       vayu_mkstr_c("list index out of range"));
    }
    l->items[i] = v;
}
int64_t vayu_list_pop(VayuList* l) {
    if (l->len == 0) {
        vayu_raise_str(vayu_mkstr_c("IndexError"),
                       vayu_mkstr_c("pop from empty list"));
    }
    return l->items[--l->len];
}
int64_t vayu_list_len(VayuList* l) { return l->len; }
void vayu_list_clear(VayuList* l) { l->len = 0; }
int64_t vayu_list_contains(VayuList* l, int64_t v) {
    for (int64_t i = 0; i < l->len; ++i) if (l->items[i] == v) return 1;
    return 0;
}
void vayu_list_insert(VayuList* l, int64_t i, int64_t v) {
    if (i < 0) i = 0;
    if (i > l->len) i = l->len;
    vayu_list_grow(l);
    memmove(l->items + i + 1, l->items + i,
            sizeof(int64_t) * (size_t)(l->len - i));
    l->items[i] = v;
    l->len++;
}
void vayu_list_remove(VayuList* l, int64_t v) {
    for (int64_t i = 0; i < l->len; ++i) {
        if (l->items[i] == v) {
            memmove(l->items + i, l->items + i + 1,
                    sizeof(int64_t) * (size_t)(l->len - i - 1));
            l->len--;
            return;
        }
    }
}

// ---- NEW: string split/join ----------------------------------------------
VayuList* vayu_str_split(VayuStr* s, VayuStr* sep) {
    VayuList* out = vayu_list_new();
    if (sep->len == 0) {
        // Split into individual characters.
        for (int64_t i = 0; i < s->len; ++i)
            vayu_list_push(out, (int64_t)vayu_mkstr(s->data + i, 1));
        return out;
    }
    int64_t pos = 0;
    while (pos <= s->len) {
        int64_t next = -1;
        for (int64_t i = pos; i + sep->len <= s->len; ++i) {
            if (memcmp(s->data + i, sep->data, (size_t)sep->len) == 0) {
                next = i; break;
            }
        }
        if (next < 0) {
            vayu_list_push(out, (int64_t)vayu_mkstr(s->data + pos, s->len - pos));
            break;
        }
        vayu_list_push(out, (int64_t)vayu_mkstr(s->data + pos, next - pos));
        pos = next + sep->len;
    }
    return out;
}
VayuStr* vayu_str_join(VayuStr* sep, VayuList* parts) {
    // Compute total size.
    int64_t total = 0;
    for (int64_t i = 0; i < parts->len; ++i) {
        VayuStr* p = (VayuStr*)parts->items[i];
        total += p->len;
        if (i + 1 < parts->len) total += sep->len;
    }
    VayuStr* r = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)total + 1);
    r->len = total;
    int64_t op = 0;
    for (int64_t i = 0; i < parts->len; ++i) {
        VayuStr* p = (VayuStr*)parts->items[i];
        if (p->len) memcpy(r->data + op, p->data, (size_t)p->len);
        op += p->len;
        if (i + 1 < parts->len) {
            if (sep->len) memcpy(r->data + op, sep->data, (size_t)sep->len);
            op += sep->len;
        }
    }
    r->data[total] = 0;
    return r;
}

// ---- maps -----------------------------------------------------------------
static uint64_t hash_str(VayuStr* s) {
    uint64_t h = 1469598103934665603ULL;
    for (int64_t i = 0; i < s->len; ++i) { h ^= (uint8_t)s->data[i]; h *= 1099511628211ULL; }
    return h;
}
VayuMap* vayu_map_new() {
    VayuMap* m = (VayuMap*)malloc(sizeof(VayuMap));
    m->len = 0; m->cap = 8;
    m->entries = (VayuMapEntry*)calloc((size_t)m->cap, sizeof(VayuMapEntry));
    return m;
}
static VayuMapEntry* map_find(VayuMap* m, VayuStr* k) {
    uint64_t h = hash_str(k) & (uint64_t)(m->cap - 1);
    for (int64_t p = 0; p < m->cap; ++p) {
        VayuMapEntry* e = &m->entries[h];
        if (!e->used) return NULL;
        if (vayu_str_eq(e->key, k)) return e;
        h = (h + 1) & (uint64_t)(m->cap - 1);
    }
    return NULL;
}
static void map_grow(VayuMap* m) {
    if (m->len * 2 < m->cap) return;
    int64_t newCap = m->cap * 2;
    VayuMapEntry* ne = (VayuMapEntry*)calloc((size_t)newCap, sizeof(VayuMapEntry));
    for (int64_t i = 0; i < m->cap; ++i) {
        if (!m->entries[i].used) continue;
        uint64_t h = hash_str(m->entries[i].key) & (uint64_t)(newCap - 1);
        while (ne[h].used) h = (h + 1) & (uint64_t)(newCap - 1);
        ne[h] = m->entries[i];
    }
    free(m->entries);
    m->entries = ne;
    m->cap = newCap;
}
void vayu_map_put(VayuMap* m, VayuStr* k, int64_t v) {
    VayuMapEntry* e = map_find(m, k);
    if (e) { e->value = v; return; }
    map_grow(m);
    uint64_t h = hash_str(k) & (uint64_t)(m->cap - 1);
    while (m->entries[h].used) h = (h + 1) & (uint64_t)(m->cap - 1);
    m->entries[h].used = 1;
    m->entries[h].key = k;
    m->entries[h].value = v;
    m->len++;
}
int64_t vayu_map_get(VayuMap* m, VayuStr* k) {
    VayuMapEntry* e = map_find(m, k);
    if (!e) vayu_raise_str(vayu_mkstr_c("KeyError"), k);
    return e->value;
}
int64_t vayu_map_has(VayuMap* m, VayuStr* k) { return map_find(m, k) != NULL; }
void vayu_map_remove(VayuMap* m, VayuStr* k) {
    VayuMapEntry* e = map_find(m, k); if (e) { e->used = 0; m->len--; }
}
int64_t vayu_map_len(VayuMap* m) { return m->len; }
void vayu_map_clear(VayuMap* m) {
    memset(m->entries, 0, sizeof(VayuMapEntry) * (size_t)m->cap);
    m->len = 0;
}
VayuList* vayu_map_keys(VayuMap* m) {
    VayuList* l = vayu_list_new();
    for (int64_t i = 0; i < m->cap; ++i) {
        if (!m->entries[i].used) continue;
        vayu_list_push(l, (int64_t)m->entries[i].key);
    }
    return l;
}

// ---- generic helpers ------------------------------------------------------
int64_t vayu_len(int64_t v, int64_t kind) {
    switch (kind) {
        case 0: return vayu_str_len((VayuStr*)v);
        case 1: return vayu_list_len((VayuList*)v);
        case 2: return vayu_map_len((VayuMap*)v);
    }
    return 0;
}
void vayu_print_value(int64_t v, int64_t kind) {
    switch (kind) {
        case 0: printf("%lld", (long long)v); break;
        case 1: printf("%s", v ? "true" : "false"); break;
        case 2: { VayuStr* s = (VayuStr*)v; fwrite(s->data, 1, (size_t)s->len, stdout); break; }
    }
}
void vayu_print_list_noln(VayuList* l, int64_t ek) {
    putchar('[');
    for (int64_t i = 0; i < l->len; ++i) {
        if (i) printf(", ");
        if (ek == 2) putchar('"');
        vayu_print_value(l->items[i], ek);
        if (ek == 2) putchar('"');
    }
    putchar(']');
}
void vayu_print_list(VayuList* l, int64_t ek) {
    vayu_print_list_noln(l, ek); putchar('\n');
}
void vayu_print_map_noln(VayuMap* m, int64_t vk) {
    putchar('{');
    int64_t printed = 0;
    for (int64_t i = 0; i < m->cap; ++i) {
        if (!m->entries[i].used) continue;
        if (printed) printf(", ");
        putchar('"');
        fwrite(m->entries[i].key->data, 1, (size_t)m->entries[i].key->len, stdout);
        printf("\": ");
        if (vk == 2) putchar('"');
        vayu_print_value(m->entries[i].value, vk);
        if (vk == 2) putchar('"');
        printed++;
    }
    putchar('}');
}
void vayu_print_map(VayuMap* m, int64_t vk) {
    vayu_print_map_noln(m, vk); putchar('\n');
}

// ---- arithmetic -----------------------------------------------------------
long long vayu_floordiv(long long a, long long b) {
    if (b == 0) {
        vayu_raise_str(vayu_mkstr_c("ZeroDivisionError"),
                       vayu_mkstr_c("division by zero"));
    }
    long long q = a / b;
    if ((a ^ b) < 0 && q * b != a) q--;
    return q;
}
long long vayu_mod(long long a, long long b) {
    if (b == 0) {
        vayu_raise_str(vayu_mkstr_c("ZeroDivisionError"),
                       vayu_mkstr_c("modulo by zero"));
    }
    long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

// ---- NEW: file I/O -------------------------------------------------------
VayuStr* vayu_read_file(VayuStr* path) {
    // NUL-terminate the path into a stack buffer.
    char buf[4096];
    int64_t n = path->len < 4095 ? path->len : 4095;
    memcpy(buf, path->data, (size_t)n);
    buf[n] = 0;

    FILE* f = fopen(buf, "rb");
    if (!f) {
        vayu_raise_str(vayu_mkstr_c("RuntimeError"),
                       vayu_concat_c("cannot open file: ", path));
    }
    fseek(f, 0, SEEK_END);
    long long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    VayuStr* s = (VayuStr*)malloc(sizeof(VayuStr) + (size_t)sz + 1);
    s->len = sz;
    if (sz > 0) fread(s->data, 1, (size_t)sz, f);
    s->data[sz] = 0;
    fclose(f);
    return s;
}
void vayu_write_file(VayuStr* path, VayuStr* content) {
    char buf[4096];
    int64_t n = path->len < 4095 ? path->len : 4095;
    memcpy(buf, path->data, (size_t)n);
    buf[n] = 0;

    FILE* f = fopen(buf, "wb");
    if (!f) {
        vayu_raise_str(vayu_mkstr_c("RuntimeError"),
                       vayu_concat_c("cannot write file: ", path));
    }
    fwrite(content->data, 1, (size_t)content->len, f);
    fclose(f);
}
int64_t vayu_file_exists(VayuStr* path) {
    char buf[4096];
    int64_t n = path->len < 4095 ? path->len : 4095;
    memcpy(buf, path->data, (size_t)n);
    buf[n] = 0;
    FILE* f = fopen(buf, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

// ---- NEW: process + args + exit ------------------------------------------
VayuList* vayu_get_args(void) {
    VayuList* l = vayu_list_new();
    for (int i = 1; i < g_argc; ++i) {
        vayu_list_push(l, (int64_t)vayu_mkstr_c(g_argv[i]));
    }
    return l;
}
int64_t vayu_run_command(VayuStr* cmd) {
    char buf[8192];
    int64_t n = cmd->len < 8191 ? cmd->len : 8191;
    memcpy(buf, cmd->data, (size_t)n);
    buf[n] = 0;
    int rc = system(buf);
    return (int64_t)rc;
}
void vayu_exit(int64_t code) {
    exit((int)code);
}

// ---- exceptions -----------------------------------------------------------
#define VAYU_MAX_TRY 64

static jmp_buf  g_jmpBufs[VAYU_MAX_TRY];
static int      g_trySp = 0;
static VayuExc* g_excValue = NULL;

int vayu_try_push(void) {
    if (g_trySp >= VAYU_MAX_TRY) {
        fprintf(stderr, "vayu: try stack overflow\n"); exit(1);
    }
    return g_trySp++;
}
void* vayu_try_buf(int id) { return &g_jmpBufs[id]; }
void vayu_try_pop(void) { if (g_trySp > 0) g_trySp--; }

VayuStr* vayu_get_exc_type(void) {
    return g_excValue ? g_excValue->typeName : NULL;
}
VayuExc* vayu_get_exc_value(void) { return g_excValue; }

VayuExc* vayu_mkexc(VayuStr* typeName, VayuStr* msg) {
    VayuExc* e = (VayuExc*)malloc(sizeof(VayuExc));
    e->typeName = typeName;
    e->message  = msg;
    return e;
}

void vayu_raise(VayuExc* e) {
    if (g_trySp == 0) {
        fwrite(e->typeName->data, 1, (size_t)e->typeName->len, stderr);
        fprintf(stderr, ": ");
        fwrite(e->message->data, 1, (size_t)e->message->len, stderr);
        fprintf(stderr, "\n");
        exit(1);
    }
    g_excValue = e;
    longjmp(g_jmpBufs[g_trySp - 1], 1);
}

void vayu_raise_str(VayuStr* typeName, VayuStr* msg) {
    VayuExc* e = vayu_mkexc(typeName, msg);
    vayu_raise(e);
}

void vayu_reraise(void) {
    if (g_trySp == 0) { fprintf(stderr, "vayu: uncaught\n"); exit(1); }
    longjmp(g_jmpBufs[g_trySp - 1], 1);
}

extern void vayu_main(void);

int main(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
    vayu_main();
    return 0;
}
)C";

    bool NativeCompiler::writeRuntimeC(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out << kRuntimeC;
        return true;
    }

    namespace {
        void tryRemove(const std::string& p) { std::remove(p.c_str()); }
        void dumpFileHead(const std::string& path, int maxLines) {
            std::ifstream in(path);
            if (!in) return;
            std::string line; int n = 0;
            std::fprintf(stderr, "--- first %d lines of %s ---\n", maxLines, path.c_str());
            while (std::getline(in, line) && n < maxLines)
                std::fprintf(stderr, "%3d | %s\n", ++n, line.c_str());
        }
    }

    std::string NativeCompiler::buildQBE(const Block& program,
        const std::string& sourceDir) {
        QbeEmitter e;
        return e.emit(program, sourceDir);
    }

    void NativeCompiler::dumpIR(const Block& program, const std::string& sourceDir) {
        lastError_.clear();
        try { std::printf("%s\n", buildQBE(program, sourceDir).c_str()); }
        catch (const std::exception& e) {
            lastError_ = e.what();
            std::fprintf(stderr, "native: %s\n", e.what());
        }
    }

    int NativeCompiler::compileAndRun(const Block& program,
        const std::string& sourceDir) {
        lastError_.clear();

        std::string il;
        try { il = buildQBE(program, sourceDir); }
        catch (const std::exception& e) {
            lastError_ = e.what();
            std::fprintf(stderr, "native: %s\n", e.what());
            return 1;
        }

        std::string base = "_vayu_" + std::to_string(VAYU_GETPID());
        std::string ssaPath = base + ".ssa";
        std::string asmPath = base + ".s";
        std::string objPath = base + ".o";
        std::string rtPath = base + "_rt.c";
        std::string exePath = outputExe_.empty() ? (base + ".exe") : outputExe_;
        const bool  compileOnly = !outputExe_.empty();

        {
            std::ofstream out(ssaPath, std::ios::binary);
            if (!out) { lastError_ = "cannot write " + ssaPath; return 1; }
            out << il;
        }

        if (!writeRuntimeC(rtPath)) {
            lastError_ = "cannot write " + rtPath;
            tryRemove(ssaPath); return 1;
        }

        {
            std::string cmd = qbePath_ + " -t " + qbeTarget_ +
                " -o \"" + asmPath + "\" \"" + ssaPath + "\"";
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
                lastError_ = "qbe failed. IL at " + ssaPath;
                std::fprintf(stderr, "native: %s\n", lastError_.c_str());
                tryRemove(rtPath); return 1;
            }
        }

        {
            std::ifstream in(asmPath, std::ios::binary);
            if (!in) { lastError_ = "cannot reopen .s"; return 1; }
            std::string filtered; std::string line;
            while (std::getline(in, line)) {
                if (line.find(".note.GNU-stack") != std::string::npos) continue;
                filtered += line; filtered += '\n';
            }
            in.close();
            std::ofstream out(asmPath, std::ios::binary);
            out << ".att_syntax prefix\n" << filtered;
        }

        {
            std::string cmd = ccPath_ + " -c -O2 \"" + asmPath +
                "\" -o \"" + objPath + "\"";
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
                lastError_ = "assembler failed. .s at " + asmPath;
                std::fprintf(stderr, "native: %s\n", lastError_.c_str());
                dumpFileHead(asmPath, 60);
                tryRemove(ssaPath); tryRemove(rtPath); return 1;
            }
        }

        {
            std::string cmd = ccPath_ + " -O2 \"" + objPath + "\" \"" + rtPath +
                "\" -o \"" + exePath + "\"";
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
                lastError_ = "linker failed";
                std::fprintf(stderr, "native: %s\n", lastError_.c_str());
                tryRemove(ssaPath); tryRemove(asmPath);
                tryRemove(objPath); tryRemove(rtPath);
                return 1;
            }
        }

        tryRemove(ssaPath);
        tryRemove(asmPath);
        tryRemove(objPath);
        tryRemove(rtPath);

        if (compileOnly) return 0;

        int runRc = std::system(("\"" + exePath + "\"").c_str());

        bool keep = (std::getenv("VAYU_KEEP_TEMP") != nullptr);
        if (!keep) tryRemove(exePath);

        if (runRc != 0) {
            lastError_ = "program exited " + std::to_string(runRc);
            return 1;
        }
        return 0;
    }

} // namespace vayu