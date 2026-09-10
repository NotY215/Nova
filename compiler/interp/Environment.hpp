#pragma once
#include "Value.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace nova {

    class Environment : public std::enable_shared_from_this<Environment> {
    public:
        explicit Environment(std::shared_ptr<Environment> parent = nullptr)
            : parent_(std::move(parent)) {
        }

        void define(const std::string& name, Value v) {
            vars_[name] = std::move(v);
        }

        /// Search this scope and all parents. Returns nullptr if not found.
        Value* lookup(const std::string& name) {
            auto it = vars_.find(name);
            if (it != vars_.end()) return &it->second;
            if (parent_) return parent_->lookup(name);
            return nullptr;
        }

        /// Assign to an existing name in this scope or any parent.
        /// Returns false if the name is not defined anywhere.
        bool assign(const std::string& name, Value v) {
            auto it = vars_.find(name);
            if (it != vars_.end()) { it->second = std::move(v); return true; }
            if (parent_) return parent_->assign(name, std::move(v));
            return false;
        }

        std::shared_ptr<Environment> parent() const { return parent_; }

    private:
        std::unordered_map<std::string, Value> vars_;
        std::shared_ptr<Environment>           parent_;
    };

} // namespace nova