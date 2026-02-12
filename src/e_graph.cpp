#include "e_graph.h"
#include <iostream>
#include "pattern.h"
#include "errors.h"
#include "analysis.h"

/// @brief Canonicalize the children of a node.
/// @param node
void EGraph::canonicalize_node(ENode &node)
{
    for (auto &i : node.get_children_mut())
    {
        i = uf.find_and_compress(i);
    }
}

size_t EGraph::num_nodes() const noexcept
{
    return nodes.size();
}

const AnalysisData &EGraph::get_class_analysis_data(Id class_id) const
{
    Id root = uf.find_root(class_id);
    return classes.at(root)->get_analysis_data();
}

const PropertyTable &EGraph::get_property_table() const noexcept
{
    return property_table;
}

/// @brief  Find an e-class with the given matrix property.
/// @param prop
/// @return
std::optional<Id> EGraph::find_class_with_property(const MatrixProperty &prop) const // probabaly not working correctly
{
    for (const auto &[class_id, eclass_ptr] : classes)
    {
        // only check root classes
        if (uf.find_root(class_id) != class_id)
            continue;

        const auto &analysis_data = eclass_ptr->get_analysis_data();
        const auto &class_prop = analysis_data.property;
        if (const auto *p = std::get_if<MatrixProperty>(&class_prop))
        {
            if (p->strict_equal(prop))
            {
                std::cout << "Found class with property: " << class_id << "\n";
                return class_id;
            }
        }
    }
    return std::nullopt;
}

/// @brief Look up a node in the e-graph. The node does not need to be canonicalized beforehand.
/// @param node
/// @return EClass ID if found, std::nullopt otherwise
std::optional<Id> EGraph::find(const ENode &node) const
{

    ENode temp(node);
    for (auto &i : temp.get_children_mut())
    {
        i = uf.find_root(i);
    }
    return memo.contains(&temp) ? std::optional<Id>(uf.find_root(memo.at(&temp))) : std::nullopt;
}

/// @brief Find the representative e-class ID for a given node ID in the union-find structure.
/// @param node_id
/// @return
Id EGraph::find_class_id(Id node_id) const
{
    return uf.find_root(node_id);
}

/// @brief Add a node to the e-graph, returning its e-class ID.
/// @param node
/// @return
Id EGraph::add_node(ENode node)
{
    if (auto found = find(node); found.has_value())
    {
        return found.value();
    }
    canonicalize_node(node);

    auto new_id = uf.make_set();

    // Move the node
    auto new_node_owner = std::make_unique<ENode>(std::move(node));
    const ENode *node_ptr = new_node_owner.get();
    nodes.push_back(std::move(new_node_owner));

    auto new_class = std::make_unique<EClass>(new_id, node_ptr, make_analysis(*node_ptr));

    for (auto child_id : node_ptr->get_children())
    {
        classes.at(child_id)->get_parents().push_back(new_id);
    }

    memo[node_ptr] = new_id;
    classes.emplace(new_id, std::move(new_class));
    return new_id;
}

Id EGraph::add_expression(const Expression &expr)
{
    Children child_ids;
    child_ids.reserve(expr.children.size());
    for (const Expression &child_expr : expr.children)
    {
        child_ids.push_back(add_expression(child_expr));
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
/// @return true if a union was performed, false if they were already in the same class
bool EGraph::union_classes(Id id1, Id id2)
{
    Id root1 = uf.find_and_compress(id1);
    Id root2 = uf.find_and_compress(id2);
    if (root1 == root2)
    {
        return false;
    }
    if (classes.at(root1)->get_parents().size() < classes.at(root2)->get_parents().size())
    {
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
    nodes1.insert(nodes1.end(),
                  std::make_move_iterator(nodes2.begin()),
                  std::make_move_iterator(nodes2.end()));

    auto &parents1 = class1_ref->get_parents();
    auto &parents2 = class2_ptr->get_parents();
    parents1.insert(parents1.end(),
                    std::make_move_iterator(parents2.begin()),
                    std::make_move_iterator(parents2.end()));

    return true;
}

void EGraph::rebuild()
{
    while (!pendings.empty())
    {
        std::vector<Id> current_pendings = std::move(pendings);
        pendings.clear();

        for (Id pending_id : current_pendings)
        {
            auto node = nodes[pending_id].get();
            memo.erase(node);
            canonicalize_node(*node);
            if (memo.contains(node))
            {
                Id existing_class_id = memo.at(node);
                union_classes(existing_class_id, pending_id);
                classes.at(uf.find_root(existing_class_id))->clean_up_nodes();
            }
            else
                memo.emplace(node, pending_id);
        }
    }
}

void EGraph::print_egraph() const
{
    std::cout << "=== E-Graph State ===\n";

    for (const auto &[class_id, eclass_ptr] : classes)
    {
        // only print root classes
        if (uf.find_root(class_id) != class_id)
            continue;

        std::cout << "EClass " << class_id << ": { ";

        // Use a set to deduplicate node strings
        std::set<std::string, std::less<>> unique_nodes;
        const auto &nodes_in_class = eclass_ptr->get_nodes();

        for (const ENode *node : nodes_in_class)
        {
            unique_nodes.insert(node->format());
        }

        bool first = true;
        for (const auto &str : unique_nodes)
        {
            if (!first)
                std::cout << ", ";
            std::cout << str;
            first = false;
        }
        std::cout << " }\n";
    }
    std::cout << "=====================\n";
}

AnalysisData EGraph::make_analysis(const ENode &node) const
{
    return MatrixAnalysis::make(*this, node);
}

// @brief Merge two analysis data objects, throwing an error if they conflict.
// @param data1
// @param data2
void EGraph::merge_analysis_data(AnalysisData &data1, const AnalysisData &data2) const
{
    MatrixAnalysis::merge(data1, data2);
}

bool EGraph::atoms_match(const Atom &pat_atom, const Atom &enode_atom) const
{
    if (pat_atom.index() != enode_atom.index())
        return false;

    if (const auto op1 = std::get_if<Op>(&pat_atom))
        return *op1 == std::get<Op>(enode_atom);

    if (const auto s1 = std::get_if<std::string>(&pat_atom))
        return *s1 == std::get<std::string>(enode_atom);

    if (const auto i1 = std::get_if<int>(&pat_atom))
        return *i1 == std::get<int>(enode_atom);

    return false;
}

void EGraph::find_matches_in_eclass(Id eclass_id, const Pattern &pattern, std::set<Substitution> &out_substitutions) const
{
    Substitution initial_subst;
    auto new_matches = search_eclass_for_pattern(eclass_id, pattern, initial_subst);

    if (!new_matches.empty())
    {
        out_substitutions.insert(new_matches.begin(), new_matches.end());
    }
}

std::vector<Substitution> EGraph::search_eclass_for_pattern(Id eclass_id, const Pattern &pattern, const Substitution &initial_subst) const
{
    std::vector<Substitution> results;
    Id canonical_id = find_class_id(eclass_id);

    // handle pattern variable case
    if (const std::string *str_atom = std::get_if<std::string>(&pattern.atom))
    {
        if (str_atom->starts_with('?'))
        {
            auto var_name = str_atom->substr(1);
            auto it = initial_subst.find(var_name);

            if (it != initial_subst.end())
            {
                // variable already bound, check for consistency
                if (it->second == canonical_id)
                {
                    results.push_back(initial_subst);
                }
            }
            else
            {
                Substitution new_subst = initial_subst; // record existing bindings
                new_subst[var_name] = canonical_id;
                results.push_back(new_subst);
            }
            return results;
        }
    }

    const auto eclass = classes.at(canonical_id).get();
    for (const ENode *node : eclass->get_nodes())
    {
        if (!atoms_match(pattern.atom, node->get_atom()) || node->get_children().size() != pattern.children.size())
        {
            continue;
        }
        std::vector<Substitution> current_matches = {initial_subst};
        bool possible = true;

        for (size_t i = 0; i < pattern.children.size(); ++i)
        {
            std::vector<Substitution> next_matches;
            const Pattern &child_pattern = pattern.children[i];
            Id child_eclass_id = node->get_children()[i];

            for (const auto &subst : current_matches)
            {
                auto child_results = search_eclass_for_pattern(child_eclass_id, child_pattern, subst);
                next_matches.insert(next_matches.end(), child_results.begin(), child_results.end());
            }
            current_matches = std::move(next_matches);

            // have to match at least one for each child
            if (current_matches.empty())
            {
                possible = false;
                break;
            }
        }
        if (possible)
        {
            results.insert(results.end(), current_matches.begin(), current_matches.end());
        }
    }
    return results;
}

/// @brief Get the ENode corresponding to the given Id
/// @param id
/// @return
const ENode &EGraph::at(Id id) const
{
    return *(nodes.at(id));
}

std::vector<Id> EGraph::get_all_class_ids() const
{
    std::vector<Id> ids;
    ids.reserve(classes.size());
    for (const auto &[id, _] : classes)
    {
        ids.push_back(id);
    }
    return ids;
}

const std::vector<const ENode *> &EGraph::get_class_nodes(Id class_id) const
{
    Id root = uf.find_root(class_id);
    return classes.at(root)->get_nodes();
}

std::vector<Id> EGraph::get_class_parents(Id class_id) const
{
    Id root = uf.find_root(class_id);
    return classes.at(root)->get_parents();
}

void EGraph::register_property(const std::string &name, const MatrixProperty &prop)
{
    property_table.add_property_entry(name, prop);
}