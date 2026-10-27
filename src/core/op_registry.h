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
struct MatrixNode;

struct MatrixOperand {
    int rows = 0;
    int cols = 0;
    MatrixProperty property{};

    bool is_square() const { return rows > 0 ? (rows == cols) : property.is_square(); }
    bool is_tall_matrix() const { return property.is_tall_matrix(); }
    bool is_wide_matrix() const { return property.is_wide_matrix(); }
    bool is_vector() const { return property.is_vector(); }
    bool is_scalar() const { return property.is_scalar(); }

    bool is_zero() const { return property.flags.is_zero; }
    bool is_symmetric() const { return property.flags.is_symmetric; }
    bool is_upper_triangular() const { return property.flags.is_upper_triangular; }
    bool is_lower_triangular() const { return property.flags.is_lower_triangular; }
    bool is_diagonal() const { return property.flags.is_diagonal; }
    bool is_full_rank() const { return property.flags.is_full_rank; }
    bool is_non_singular() const { return property.flags.is_non_singular; }
    bool is_positive_definite() const { return property.flags.is_positive_definite; }
    bool is_orthogonal() const { return property.flags.is_orthogonal; }
};

using TupleOperand = std::vector<MatrixOperand>;

using IndexOperand = int;

using ScalarOperand = double;

using Operand = std::variant<MatrixOperand, TupleOperand, IndexOperand, ScalarOperand>;

inline const MatrixOperand *as_matrix(const Operand &op) { return std::get_if<MatrixOperand>(&op); }
inline MatrixOperand *as_matrix(Operand &op) { return std::get_if<MatrixOperand>(&op); }

inline const TupleOperand *as_tuple(const Operand &op) { return std::get_if<TupleOperand>(&op); }
inline TupleOperand *as_tuple(Operand &op) { return std::get_if<TupleOperand>(&op); }

inline const IndexOperand *as_index(const Operand &op) { return std::get_if<IndexOperand>(&op); }
inline IndexOperand *as_index(Operand &op) { return std::get_if<IndexOperand>(&op); }

inline const ScalarOperand *as_scalar(const Operand &op) { return std::get_if<ScalarOperand>(&op); }
inline ScalarOperand *as_scalar(Operand &op) { return std::get_if<ScalarOperand>(&op); }

using AnalyzeFn = std::function<AnalysisData(const std::vector<Operand> &operands)>;
using CostFn = std::function<Cost(Op op, const std::vector<Operand> &operands)>;
using EvalFn = std::function<void(const std::vector<const MatrixNode *> &inputs, MatrixNode &output)>;
struct OpDescriptor {
    Op op{};
    std::string name;
    bool is_kernel = false;
    AnalyzeFn analyze = nullptr;
    CostFn cost = nullptr;
    EvalFn eval = nullptr;
    std::optional<int> in_place_input = std::nullopt;
};

void init_builtin_ops(class OpRegistry &registry);
void init_operands(
    const EGraph &egraph, const Children &children, std::vector<Operand> &operands,
    const SizeBindings *bindings = nullptr);

class OpRegistry {
  public:
    static OpRegistry &instance();

    Op register_op(
        const std::string &name, bool is_kernel, AnalyzeFn analyze = nullptr, CostFn cost = nullptr,
        EvalFn eval = nullptr, std::optional<int> in_place_input = std::nullopt);

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
