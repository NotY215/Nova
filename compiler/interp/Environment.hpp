#pragma once
#include "Value.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace vayu {

    class Environment : public std::enable_shared_from_this<Environment> {
    public:
        explicit Environment(std::shared_ptr<Environment> parent = nullptr)
            : parent_(std::move(parent)) {
        }

        void define(const std::string& name, Value v) {
            vars_[name] = std::move(v);
        }

        Value* lookup(const std::string& name) {
            auto it = vars_.find(name);
            if (it != vars_.end()) return &it->second;
            if (parent_) return parent_->lookup(name);
            return nullptr;
        }

        bool assign(const std::string& name, Value v) {
            auto it = vars_.find(name);
            if (it != vars_.end()) { it->second = std::move(v); return true; }
            if (parent_) return parent_->assign(name, std::move(v));
            return false;
        }

        std::shared_ptr<Environment> parent() const { return parent_; }

        /// Names bound directly in this scope (not parents).
        const std::unordered_map<std::string, Value>& localVars() const {
            return vars_;
        }

    private:
        std::unordered_map<std::string, Value> vars_;
        std::shared_ptr<Environment>           parent_;
    };

} // namespace vayu