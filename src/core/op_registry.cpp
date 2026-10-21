#include "op_registry.h"
#include "errors.h"

namespace egraph {

bool is_kernel_op(Op op) { return OpRegistry::instance().is_kernel(op); }

OpRegistry &OpRegistry::instance() {
    static OpRegistry registry;
    return registry;
}

OpRegistry::OpRegistry() { init_builtins(); }

void OpRegistry::init_builtins() { init_builtin_ops(*this); }

void OpRegistry::register_op(Op op, const std::string &name, bool is_kernel, AnalyzeFn analyze) {
    auto idx = static_cast<size_t>(op);
    if (idx >= ops.size()) {
        ops.resize(idx + 1);
    }
    ops[idx] = OpDescriptor{op, name, is_kernel, std::move(analyze)};
    name_to_op_[name] = op;
}

Op OpRegistry::register_op(const std::string &name, bool is_kernel, AnalyzeFn analyze) {
    if (auto it = name_to_op_.find(name); it != name_to_op_.end()) {
        std::string error_msg = "Operation already registered: " + name;
        throw InvalidOperationError(error_msg);
    }

    auto new_op = static_cast<Op>(ops.size());
    ops.push_back(OpDescriptor{new_op, name, is_kernel, std::move(analyze)});
    name_to_op_[name] = new_op;
    return new_op;
}

bool OpRegistry::is_kernel(Op op) const {
    auto idx = static_cast<size_t>(op);
    if (idx < ops.size()) {
        return ops[idx].is_kernel;
    }
    return false;
}

std::string_view OpRegistry::name(Op op) const {
    auto idx = static_cast<size_t>(op);
    if (idx < ops.size()) {
        return ops[idx].name;
    }
    return "UnknownOp";
}

std::optional<Op> OpRegistry::find_by_name(std::string_view name) const {
    if (auto it = name_to_op_.find(std::string(name)); it != name_to_op_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const OpDescriptor &OpRegistry::get(Op op) const {
    auto idx = static_cast<size_t>(op);
    if (idx < ops.size() && !ops[idx].name.empty()) {
        return ops[idx];
    }
    throw InvalidOperationError("Op not found in OpRegistry: " + std::to_string(static_cast<uint32_t>(op)));
}

bool OpRegistry::contains(Op op) const {
    auto idx = static_cast<size_t>(op);
    return idx < ops.size() && !ops[idx].name.empty();
}

bool OpRegistry::contains(std::string_view name) const { return name_to_op_.contains(std::string(name)); }
} // namespace egraph
