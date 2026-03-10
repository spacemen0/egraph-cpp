#pragma once
#include <vector>
#include <optional>
#include <set>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include "union_find.h"
#include "e_node.h"
#include "e_class.h"
#include "expression.h"
#include "pattern.h"
#include "cost_storage.h"
#include "property_table.h"

class EGraph
{
public:
    EGraph() = delete;
    explicit EGraph(PropertyTable pt) : property_table(std::move(pt)), cost_storage(*this)
    {
    }
    void canonicalize_node(ENode &node);
    std::optional<Id> find_node_id(const ENode &node) const;
    Id find_class_id(Id node_id) const;
    Id add_node(ENode node);
    Id add_expression(const Expression &expr);
    Id add_expression(const Expression &expr, const Substitution &subst);
    void register_property(const std::string &name, const MatrixProperty &prop);
    bool union_classes(Id id1, Id id2);
    void rebuild();
    void print_egraph() const;
    void find_matches_in_eclass(Id eclass_id, const Pattern &pattern, std::set<Substitution> &out_substitutions);
    const ENode &at(Id id) const;
    std::vector<Id> get_all_class_ids() const;
    const std::vector<const ENode *> &get_class_nodes(Id class_id) const;
    std::vector<Id> get_class_parents(Id class_id) const;
    size_t num_nodes() const noexcept;
    const AnalysisData &get_class_analysis_data(Id class_id) const;
    const PropertyTable &get_property_table() const noexcept;
    std::optional<Id> find_class_with_property(const MatrixProperty &prop) const;
    void to_dot_file(const std::string &filename) const;
    void to_img(const std::string &filename, const std::string &format) const;
    CostStorage &get_cost_storage() { return cost_storage; }
    void update_cost_storage() { cost_storage.compute(); }
    uint64_t get_revision() const noexcept { return revision; }
    bool is_clean() const noexcept { return pendings.empty(); }

private:
    // stores the union-find structure for e-classes (which stores equivalences)
    UnionFind uf;
    // stores all e-nodes in the order they were added
    std::vector<std::unique_ptr<ENode>> nodes;
    // stores pending parent updates after unions
    std::vector<Id> pendings;
    // stores mapping from ENode to EClass id (canonicalized one after rebuild)
    std::unordered_map<const ENode *, Id, ENodePtrHash, ENodePtrEqual> memo;
    // stores mapping from EClass id to EClass, classes being merged will be removed
    std::unordered_map<Id, std::unique_ptr<EClass>> classes;
    PropertyTable property_table;
    CostStorage cost_storage;
    uint64_t revision = 0;
    std::string to_dot() const;
    AnalysisData make_analysis(const ENode &node) const;
    void merge_analysis_data(AnalysisData &data1, const AnalysisData &data2) const;
};