#pragma once
#include "basic_types.h"
#include "e_graph.h"
#include "extractor.h"
#include "storage_node.h"
#include <string_view>

namespace egraph {

class Evaluator {
  public:
    explicit Evaluator(
        EGraph &egraph, const ExtractionResult &result, const SizeBindings &size_bindings,
        const DataBindings &data_bindings, const std::vector<Id> &preserved_ids = {});
    std::vector<double> evaluate();
    std::vector<double> get_preserved(Id id) const;

  private:
    void prepare_in_place(Id child_id, EvalValue &output);
    void setup_in_place_output(Id child_id, MatrixNode &output);
    EGraph &egraph;
    ExtractionResult result;
    const SizeBindings &size_bindings;
    const DataBindings &data_bindings;
    std::vector<Id> preserved_ids;

    // simply to mark which slots are preserved, so that we don't overwrite them
    std::vector<bool> preserved_slots;

    // element 0 stores the data for the first node in execution_order, and so on.
    std::vector<EvalValue> data_storage;
    // index is class_id, value is the index in execution_order
    std::vector<int> slot_map;
    // index is the index of the slot in execution_order (stores how many times a node will be referenced in the
    // execution chain.)
    mutable std::vector<int> use_counts;
};

void eval_symm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_trmm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_trsm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_gemm_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_gemv_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_syrk_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_trtri(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_transpose(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_scale(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_axpy(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const MatrixNode *> &inputs,
    MatrixNode &output);
void eval_potrf_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output);
void eval_geqrf(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output);
void eval_get(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output);
void eval_orgqr(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output);
void eval_ormqr_group(
    std::string_view op_name, const std::vector<Operand> &operands, const std::vector<const EvalValue *> &inputs,
    EvalValue &output);
} // namespace egraph
