#include "e_graph.h"
#include <iostream>

/// @brief Canonicalize the children of a node.
/// @param node
void EGraph::canonicalize_node(ENode &node)
{
    for (auto &i : node.get_children_mut())
    {
        i = uf.find_and_compress(i);
    }
}

/// @brief Look up a node in the e-graph.
/// @param node
/// @return EClass ID if found, std::nullopt otherwise
std::optional<Id> EGraph::find(const ENode &node)
{

    ENode temp(node);
    for (auto &i : temp.get_children_mut())
    {
        i = uf.find_and_compress(i);
    }
    return memo.contains(&temp) ? std::optional<Id>(uf.find_and_compress(memo.at(&temp))) : std::nullopt;
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

    auto new_id = uf.make_set();
    auto new_class = std::make_unique<EClass>(new_id);

    // Move the node
    auto new_node_owner = std::make_unique<ENode>(std::move(node));
    const ENode *stable_ptr = new_node_owner.get();
    nodes.push_back(std::move(new_node_owner));

    new_class->get_nodes().push_back(stable_ptr);

    for (auto child_id : stable_ptr->get_children())
    {
        classes.at(child_id)->get_parents().push_back(new_id);
    }

    memo[stable_ptr] = new_id;
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

/// @brief Union two e-classes given their IDs.
/// @param id1
/// @param id2
/// @return
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
    std::vector<Id> current_pendings = std::move(pendings);
    pendings.clear();

    for (Id pending_id : current_pendings)
    {
        auto node = nodes[pending_id].get();
        for (auto &child_id : node->get_children_mut())
        {
            child_id = uf.find_and_compress(child_id);
        }
        if (memo.contains(node))
        {
            Id existing_class_id = memo.at(node);
            union_classes(existing_class_id, pending_id);
        }
        memo.emplace(node, pending_id);
    }
}

const ENode &EGraph::at(Id id) const
{
    return *(nodes.at(id));
}
