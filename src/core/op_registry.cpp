#include "op_registry.h"
#include "errors.h"
#include "utils.h"

namespace egraph {

bool is_kernel_op(Op op) { return OpRegistry::instance().is_kernel(op); }

OpRegistry &OpRegistry::instance() {
    static OpRegistry registry;
    return registry;
}

OpRegistry::OpRegistry() { init_builtins(); }

void OpRegistry::init_builtins() { init_builtin_ops(*this); }

Op OpRegistry::register_op(const std::string &name, bool is_kernel, AnalyzeFn analyze, CostFn cost) {
    if (auto it = name_to_op_.find(name); it != name_to_op_.end()) {
        std::string error_msg = "Operation already registered: " + name;
        throw InvalidOperationError(error_msg);
    }

    auto new_op = static_cast<Op>(ops.size());
    ops.push_back(OpDescriptor{new_op, name, is_kernel, std::move(analyze), std::move(cost)});
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

std::string_view OpRegistry::name(Op op) {
    auto idx = static_cast<size_t>(op);
    if (idx < instance().ops.size()) {
        return instance().ops[idx].name;
    }
    return "UnknownOp";
}

std::optional<Op> OpRegistry::find_by_name(std::string_view name) const {
    if (auto it = name_to_op_.find(std::string(name)); it != name_to_op_.end()) {
        return it->second;
    }
    return std::nullopt;
}

Op OpRegistry::op(std::string_view name) {
    if (auto it = instance().find_by_name(name); it.has_value()) {
        return it.value();
    }
    throw InvalidOperationError("Operation not found in OpRegistry: " + std::string(name));
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

void init_operands(
    const EGraph &egraph, const Children &children, std::vector<Operand> &operands,
    const SizeBindings *bindings) {
    operands.reserve(children.size());

    for (Id child_id : children) {
        if (auto idx = get_int_from_eclass(egraph, child_id)) {
            operands.emplace_back(*idx);
        } else if (auto dbl = get_double_from_eclass(egraph, child_id)) {
            operands.emplace_back(*dbl);
        } else {
            bool is_symbolic_scalar = false;
            for (const auto &cnode : egraph.get_class_nodes(child_id)) {
                if (std::holds_alternative<ScalarExpr>(cnode->get_atom())) {
                    is_symbolic_scalar = true;
                    break;
                }
            }
            if (is_symbolic_scalar) {
                operands.emplace_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            const auto &class_data = egraph.get_class_analysis_data(child_id);
            if (auto prop = std::get_if<MatrixProperty>(&class_data.property)) {
                MatrixOperand mat;
                mat.property = *prop;
                if (bindings) {
                    Shape bound = bind_shape(prop->shape, *bindings);
                    if (!is_numeric(bound)) {
                        throw std::runtime_error("Cannot compute cost: missing size binding for symbolic dimension");
                    }
                    mat.rows = std::get<int>(bound.first);
                    mat.cols = std::get<int>(bound.second);
                    mat.property.shape = bound;
                } else {
                    if (auto r = std::get_if<int>(&prop->shape.first)) {
                        mat.rows = *r;
                    }
                    if (auto c = std::get_if<int>(&prop->shape.second)) {
                        mat.cols = *c;
                    }
                }
                operands.emplace_back(mat);
            } else if (auto tuple_prop = std::get_if<TupleProperty>(&class_data.property)) {
                TupleOperand tuple;
                tuple.reserve(tuple_prop->size());
                for (const auto &elem_prop : *tuple_prop) {
                    MatrixOperand elem;
                    elem.property = elem_prop;
                    if (bindings) {
                        Shape bound = bind_shape(elem_prop.shape, *bindings);
                        if (!is_numeric(bound)) {
                            throw std::runtime_error(
                                "Cannot compute cost: missing size binding for symbolic dimension");
                        }
                        elem.rows = std::get<int>(bound.first);
                        elem.cols = std::get<int>(bound.second);
                        elem.property.shape = bound;
                    } else {
                        if (auto r = std::get_if<int>(&elem_prop.shape.first)) {
                            elem.rows = *r;
                        }
                        if (auto c = std::get_if<int>(&elem_prop.shape.second)) {
                            elem.cols = *c;
                        }
                    }

                    tuple.push_back(elem);
                }
                operands.emplace_back(tuple);
            }
        }
    }
}

} // namespace egraph
