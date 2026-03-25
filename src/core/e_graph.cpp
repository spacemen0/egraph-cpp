#include "e_graph.h"
#include "analysis.h"
#include "e_graph_visualization.h"
#include "e_node.h"
#include "matcher.h"
#include "pattern.h"
#include <iostream>
#include <optional>

/// @brief Canonicalize the children of a node.
/// @param node
void EGraph::canonicalize_node(ENode &node) {
    for (auto &i : node.get_children_mut()) {
        i = uf.find_and_compress(i);
    }
}

size_t EGraph::num_nodes() const noexcept { return memo.size(); }

const AnalysisData &EGraph::get_class_analysis_data(Id class_id) const {
    Id root = uf.find_root(class_id);
    return classes.at(root)->get_analysis_data();
}

const PropertyTable &EGraph::get_property_table() const noexcept { return property_table; }

PropertyTable &EGraph::get_property_table() noexcept { return property_table; }

/// @brief  Find an e-class with the given matrix property.
/// @param prop
/// @return
std::optional<Id> EGraph::find_class_with_property(const MatrixProperty &prop) const // probabaly not working correctly
{
    for (const auto &[class_id, eclass_ptr] : classes) {
        // only check root classes
        if (uf.find_root(class_id) != class_id)
            continue;

        const auto &analysis_data = eclass_ptr->get_analysis_data();
        const auto &class_prop = analysis_data.property;
        if (const auto *p = std::get_if<MatrixProperty>(&class_prop)) {
            if (p->strict_equal(prop)) {
                return class_id;
            }
        }
    }
    return std::nullopt;
}

std::optional<ENode> EGraph::find_node(Id node_id) const {
    if (node_id >= nodes.size()) {
        return std::nullopt;
    }
    return *nodes[node_id];
}

/// @brief Look up a node in the e-graph. The node does not need to be
/// canonicalized beforehand.
/// @param node
/// @return EClass ID if found, std::nullopt otherwise
std::optional<Id> EGraph::find_node_id(const ENode &node) const {

    ENode temp(node);
    for (auto &i : temp.get_children_mut()) {
        i = uf.find_root(i);
    }
    return memo.contains(&temp) ? std::optional<Id>(uf.find_root(memo.at(&temp))) : std::nullopt;
}

/// @brief Find the representative e-class ID for a given node ID in the
/// union-find structure.
/// @param node_id
/// @return
Id EGraph::find_class_id(Id node_id) const { return uf.find_root(node_id); }

/// @brief Add a node to the e-graph, returning its e-class ID.
/// @param node
/// @return
Id EGraph::add_node(ENode node) {
    if (auto found = find_node_id(node); found.has_value()) {
        return found.value();
    }
    canonicalize_node(node);

    auto new_id = uf.make_set();

    // Move the node
    auto new_node_owner = std::make_unique<ENode>(std::move(node));
    const ENode *node_ptr = new_node_owner.get();
    nodes.push_back(std::move(new_node_owner));

    auto new_class = std::make_unique<EClass>(new_id, node_ptr, make_analysis(*node_ptr));

    for (auto child_id : node_ptr->get_children()) {
        classes.at(child_id)->get_parents().push_back(new_id);
    }

    memo[node_ptr] = new_id;
    classes.emplace(new_id, std::move(new_class));
    ++revision;
    return new_id;
}

Id EGraph::add_expression(const Expression &expr) {
    Children child_ids;
    child_ids.reserve(expr.children.size());
    for (const Expression &child_expr : expr.children) {
        child_ids.push_back(add_expression(child_expr));
    }
    ENode current_node{
        child_ids,
        expr.atom,
    };
    return add_node(current_node);
}

Id EGraph::add_expression(const Expression &expr, const Substitution &subst) {
    if (std::holds_alternative<std::string>(expr.atom)) {
        const std::string &name = std::get<std::string>(expr.atom);
        if (name.length() > 1 && name[0] == '?') {
            std::string var_name = name.substr(1);
            if (subst.count(var_name)) {
                return subst.at(var_name);
            }
        }
    }

    Children child_ids;
    child_ids.reserve(expr.children.size());
    for (const Expression &child_expr : expr.children) {
        child_ids.push_back(add_expression(child_expr, subst));
    }
    ENode current_node{
        child_ids,
        expr.atom,
    };
    return add_node(current_node);
}

/// @brief Union two e-classes given their IDs. The classes need not be roots.
/// @param id1
/// @param id2
/// @return true if a union was performed, false if they were already in the
/// same class
bool EGraph::union_classes(Id id1, Id id2) {
    Id root1 = uf.find_and_compress(id1);
    Id root2 = uf.find_and_compress(id2);
    if (root1 == root2) {
        return false;
    }
    if (classes.at(root1)->get_parents().size() < classes.at(root2)->get_parents().size()) {
        std::swap(root1, root2);
    }

    auto &data1 = classes.at(root1)->get_analysis_data();
    const auto &data2 = classes.at(root2)->get_analysis_data();
    merge_analysis_data(data1, data2);
    // takes the ownership of class2 to a local unique_ptr
    auto node_handle = classes.extract(root2);
    auto class2_ptr = std::move(node_handle.mapped());

    // find(root2) will be root1.
    uf.unite(root1, root2);

    const auto &class1_ref = classes.at(root1);

    pendings.insert(pendings.end(), class2_ptr->get_parents().begin(), class2_ptr->get_parents().end());

    // move the nodes and parents from class2 to class1
    auto &nodes1 = class1_ref->get_nodes();
    auto &nodes2 = class2_ptr->get_nodes();
    nodes1.insert(nodes1.end(), std::make_move_iterator(nodes2.begin()), std::make_move_iterator(nodes2.end()));

    auto &parents1 = class1_ref->get_parents();
    auto &parents2 = class2_ptr->get_parents();
    parents1.insert(parents1.end(), std::make_move_iterator(parents2.begin()), std::make_move_iterator(parents2.end()));

    ++revision;
    return true;
}

void EGraph::rebuild() {
    while (!pendings.empty()) {
        std::vector<Id> current_pendings = std::move(pendings);
        pendings.clear();

        for (Id pending_id : current_pendings) {
            auto node = nodes[pending_id].get();
            memo.erase(node);
            canonicalize_node(*node);
            if (memo.contains(node)) {
                Id existing_class_id = memo.at(node);
                union_classes(existing_class_id, pending_id);
                classes.at(uf.find_root(existing_class_id))->clean_up_nodes();
            } else
                memo.emplace(node, pending_id);
        }
    }
}

void EGraph::print_egraph() const {
    std::cout << "=== E-Graph State ===\n";

    for (const auto &[class_id, eclass_ptr] : classes) {
        // only print root classes
        if (uf.find_root(class_id) != class_id)
            continue;

        std::cout << "EClass " << class_id << ": { ";

        // Use a set to deduplicate node strings
        std::set<std::string, std::less<>> unique_nodes;
        const auto &nodes_in_class = eclass_ptr->get_nodes();

        for (const ENode *node : nodes_in_class) {
            unique_nodes.insert(node->format());
        }

        bool first = true;
        for (const auto &str : unique_nodes) {
            if (!first)
                std::cout << ", ";
            std::cout << str;
            first = false;
        }
        std::cout << " }\n";
    }
    std::cout << "=====================\n";
}

AnalysisData EGraph::make_analysis(const ENode &node) const { return MatrixAnalysis::make(*this, node); }

// @brief Merge two analysis data objects, throwing an error if they conflict.
// @param data1
// @param data2
void EGraph::merge_analysis_data(AnalysisData &data1, const AnalysisData &data2) const {
    MatrixAnalysis::merge(data1, data2);
}

void EGraph::find_matches_in_eclass(Id eclass_id, const Pattern &pattern, std::set<Substitution> &out_substitutions) {
    Matcher matcher(*this);
    auto matches = matcher.find_matches_in_eclass(eclass_id, pattern);
    out_substitutions.insert(matches.begin(), matches.end());
}

/// @brief Get the ENode corresponding to the given Id
/// @param id
/// @return
const ENode &EGraph::at(Id id) const { return *(nodes.at(id)); }

std::vector<Id> EGraph::get_all_class_ids() const {
    std::vector<Id> ids;
    ids.reserve(classes.size());
    for (const auto &[id, _] : classes) {
        ids.push_back(id);
    }
    return ids;
}

const std::vector<const ENode *> &EGraph::get_class_nodes(Id class_id) const {
    Id root = uf.find_root(class_id);
    return classes.at(root)->get_nodes();
}

std::vector<Id> EGraph::get_class_parents(Id class_id) const {
    Id root = uf.find_root(class_id);
    return classes.at(root)->get_parents();
}

void EGraph::register_or_update_property(const std::string &name, const MatrixProperty &prop) {
    property_table.add_or_update_property_entry(name, prop);
}

std::string EGraph::to_dot() const { return EGraphVisualization::to_dot(*this); }

void EGraph::to_dot_file(const std::string &filename) const { EGraphVisualization::to_dot_file(*this, filename); }

void EGraph::to_img(const std::string &filename, const std::string &format) const {
    EGraphVisualization::to_img(*this, filename, format);
}
