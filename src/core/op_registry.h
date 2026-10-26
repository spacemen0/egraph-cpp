#pragma once

#include "basic_types.h"
#include "property_table.h"
#include "types.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace egraph {

class EGraph;

struct Operand {
    int rows = 0;
    int cols = 0;
    MatrixProperty property{};
    std::vector<Operand> tuple{};

    bool is_tuple() const { return !tuple.empty(); }
    bool is_square() const { return rows > 0 && rows == cols; }
    bool is_zero() const { return property.flags.is_zero; }
    bool is_symmetric() const { return property.flags.is_symmetric; }
    bool is_upper_triangular() const { return property.flags.is_upper_triangular; }
    bool is_lower_triangular() const { return property.flags.is_lower_triangular; }
    bool is_diagonal() const { return property.flags.is_diagonal; }
};

using AnalyzeFn = std::function<AnalysisData(const EGraph &egraph, const std::vector<Id> &children)>;
using CostFn = std::function<Cost(Op op, const std::vector<Operand> &operands)>;
struct OpDescriptor {
    Op op{};
    std::string name;
    bool is_kernel = false;
    AnalyzeFn analyze = nullptr;
    CostFn cost = nullptr;
};

void init_builtin_ops(class OpRegistry &registry);

class OpRegistry {
  public:
    static OpRegistry &instance();

    Op register_op(const std::string &name, bool is_kernel, AnalyzeFn analyze = nullptr, CostFn cost = nullptr);

    // Queries
    bool is_kernel(Op op) const;
    static std::string_view name(Op op);
    std::optional<Op> find_by_name(std::string_view name) const;
    static Op op(std::string_view name);
    const OpDescriptor &get(Op op) const;
    bool contains(Op op) const;
    bool contains(std::string_view name) const;

  private:
    OpRegistry();
    void init_builtins();

    std::vector<OpDescriptor> ops;
    std::unordered_map<std::string, Op> name_to_op_;
};

} // namespace egraph
