#include "e_graph.h"
#include "analysis.h"
#include "e_graph_visualization.h"
#include "e_node.h"
#include "errors.h"
#include "matcher.h"
#include "pattern.h"
#include "utils.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <optional>

namespace {
std::string make_dot_output_path(const std::string &filename) {
    std::filesystem::path output_path(filename);
    std::filesystem::path dot_dir = std::filesystem::path("dot_files");
    std::filesystem::path destination =
        dot_dir / (output_path.has_filename() ? output_path.filename() : std::filesystem::path("output"));
    std::filesystem::create_directories(destination.parent_path());
    return destination.string();
}
} // namespace

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
    auto it = classes.find(root);
    if (it != classes.end()) {
        return it->second->get_analysis_data();
    }
    throw std::runtime_error("EClass ID not found in classes when getting analysis data.");
}

const PropertyTable &EGraph::get_property_table() const noexcept { return property_table; }

PropertyTable &EGraph::get_property_table() noexcept { return property_table; }

/// @brief  Find an e-class with the given matrix property.
/// @param prop
/// @return
std::optional<Id> EGraph::find_class_with_property(const MatrixProperty &prop) const {
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

std::optional<Id> EGraph::find_expression_id(const Expression &expr) const {
    ENode temp_node({}, expr.atom);
    for (const Expression &child : expr.children) {
        auto child_id_opt = find_expression_id(child);
        if (!child_id_opt.has_value()) {
            return std::nullopt;
        }
        temp_node.get_children_mut().push_back(child_id_opt.value());
    }
    return find_node_id(temp_node);
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
    if (std::holds_alternative<uint32_t>(expr.atom)) {
        const uint32_t id = std::get<uint32_t>(expr.atom);
        const std::string &name = get_string_from_lookup(id);
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
    if (!classes.contains(root1) || !classes.contains(root2)) {
        throw std::runtime_error("union_classes: One or both IDs not found in classes");
    }
    if (classes.at(root1)->get_parents().size() < classes.at(root2)->get_parents().size()) {
        std::swap(root1, root2);
    }

    auto &data1 = classes.at(root1)->get_analysis_data();
    const auto &data2 = classes.at(root2)->get_analysis_data();
    bool changed = merge_analysis_data(data1, data2);
    // takes the ownership of class2 to a local unique_ptr
    auto node_handle = classes.extract(root2);
    auto class2_ptr = std::move(node_handle.mapped());

    // find(root2) will be root1.
    uf.unite(root1, root2);

    const auto &class1_ref = classes.at(root1);

    pending.insert(pending.end(), class2_ptr->get_parents().begin(), class2_ptr->get_parents().end());
    if (changed) {
        for (auto parent : class1_ref->get_parents()) {
            analysis_pending.insert(parent);
        }
    }
    for (auto parent : class2_ptr->get_parents()) {
        analysis_pending.insert(parent);
    }
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
    while (!pending.empty()) {
        std::vector<Id> current_pending = std::move(pending);
        pending.clear();

        for (Id pending_id : current_pending) {
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

    while (!analysis_pending.empty()) {
        std::unordered_set<Id> current_pending = std::move(analysis_pending);
        analysis_pending.clear();

        for (Id pending_id : current_pending) {
            auto node = nodes[pending_id].get();
            Id class_id = uf.find_root(pending_id);
            auto &class_ref = classes.at(class_id);
            auto new_analysis = make_analysis(*node);
            bool changed = merge_analysis_data(class_ref->get_analysis_data(), new_analysis);
            if (changed) {
                for (auto parent : class_ref->get_parents()) {
                    analysis_pending.insert(parent);
                }
            }
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
bool EGraph::merge_analysis_data(AnalysisData &data1, const AnalysisData &data2) const {
    return MatrixAnalysis::merge(data1, data2);
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

PruneResult EGraph::prune_nodes_except(const std::unordered_map<Id, std::unordered_set<const ENode *>> &keep_choices) {
    PruneResult result;
    result.nodes_before = memo.size();

    std::unordered_map<const ENode *, Id, ENodePtrHash, ENodePtrEqual> new_memo;
    new_memo.reserve(memo.size());

    std::vector<Id> class_ids = get_all_class_ids();
    size_t removed_nodes = 0;
    size_t changed_classes = 0;

    for (Id class_id : class_ids) {
        Id root = uf.find_root(class_id);
        if (root != class_id) {
            continue;
        }

        auto class_it = classes.find(root);
        if (class_it == classes.end()) {
            continue; // This should not happen
        }

        auto &class_nodes = class_it->second->get_nodes();
        auto keep_it = keep_choices.find(root);

        // remove the entire class
        if (keep_it == keep_choices.end()) {
            removed_nodes += class_nodes.size();
            changed_classes++;
            classes.erase(class_it);
            continue;
        }

        // remove nodes that are not in the keep set
        const auto &keep_nodes = keep_it->second;
        size_t before = class_nodes.size();
        class_nodes.erase(
            std::remove_if(
                class_nodes.begin(), class_nodes.end(),
                [&](const ENode *node) {
            return !keep_nodes.contains(node);
        }),
            class_nodes.end());

        if (class_nodes.size() < before) {
            removed_nodes += (before - class_nodes.size());
            changed_classes++;
        }
    }

    // build new memo
    for (const auto &[class_id, eclass_ptr] : classes) {
        assert(uf.find_root(class_id) == class_id);

        for (const ENode *node : eclass_ptr->get_nodes()) {
            new_memo[node] = class_id;
        }
    }

    memo = std::move(new_memo);

    // remove dangling parents
    for (auto &[class_id, eclass_ptr] : classes) {
        auto &parents = eclass_ptr->get_parents();
        parents.erase(
            std::remove_if(
                parents.begin(), parents.end(),
                [&](Id parent_id) {
            return !memo.contains(nodes[parent_id].get());
        }),
            parents.end());
    }

    // remove pending ids that are no longer valid
    pending.erase(
        std::remove_if(
            pending.begin(), pending.end(),
            [&](Id pending_id) {
        return !memo.contains(nodes[pending_id].get());
    }),
        pending.end());

    result.nodes_after = memo.size();
    result.nodes_pruned = removed_nodes;
    result.classes_with_removed_nodes = changed_classes;
    result.changed = removed_nodes > 0;

    if (result.changed) {
        ++revision;
    }
    return result;
}

bool EGraph::update_class_analysis_data(Id class_id, const AnalysisData &data) {
    Id root = uf.find_and_compress(class_id);
    if (!classes.contains(root)) {
        return false;
    }
    auto &current_data = classes.at(root)->get_analysis_data();

    bool changed = false;
    if (auto *p1 = std::get_if<MatrixProperty>(&current_data.property)) {
        if (auto *p2 = std::get_if<MatrixProperty>(&data.property)) {
            changed = !p1->strict_equal(*p2);
        } else {
            changed = true;
        }
    } else if (auto *t1 = std::get_if<TupleProperty>(&current_data.property)) {
        if (auto *t2 = std::get_if<TupleProperty>(&data.property)) {
            if (t1->size() != t2->size()) {
                throw AnalysisError("Cannot merge TupleProperties of different sizes");
            } else {
                for (size_t i = 0; i < t1->size(); ++i) {
                    if (!(*t1)[i].strict_equal((*t2)[i])) {
                        changed = true;
                        break;
                    }
                }
            }
        } else {

            throw AnalysisError("Cannot merge TupleProperty with non-TupleProperty");
        }
    } else {

        // if current property is neither MatrixProperty nor TupleProperty, then it's double
        changed = (current_data != data);
    }

    if (changed) {
        current_data = data;
        register_analysis_pending_parents_for(root);
        return true;
    }
    return false;
}

bool EGraph::register_analysis_pending_parents_for(Id class_id) {
    Id root = uf.find_and_compress(class_id);
    if (!classes.contains(root)) {
        return false;
    }
    auto &parents = classes.at(root)->get_parents();
    for (auto parent : parents) {
        analysis_pending.insert(parent);
    }
    return true;
}

void EGraph::register_or_update_property(const std::string &name, const MatrixProperty &prop) {
    property_table.add_or_update_property_entry(name, prop);
}

std::string EGraph::to_dot() const { return EGraphVisualization::to_dot(*this); }

void EGraph::to_dot_file(const std::string &filename) const {
    EGraphVisualization::to_dot_file(*this, make_dot_output_path(filename));
}

void EGraph::to_img(const std::string &filename, const std::string &format) const {
    EGraphVisualization::to_img(*this, make_dot_output_path(filename), format);
}
