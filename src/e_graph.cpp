#include "e_graph.h"
#include <iostream>

std::optional<Id> EGraph::look_up(ENode &node)
{
    for (auto i : node.get_children_mut())
    {
        i = uf.find_and_compress(i);
    }
    auto temp_node_ptr = std::make_unique<ENode>(node);
    auto it = memo.find(temp_node_ptr.get());
    return it != memo.end() ? std::optional<Id>(it->second) : std::nullopt;
}

/// @brief Add a node to the e-graph, returning its e-class ID.
/// @param node
/// @return
Id EGraph::add_node(ENode &node)
{
    if (look_up(node).has_value())
    {
        return look_up(node).value();
    }
    Id class_id = add_class(node, node);
    return class_id;
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

    auto class2_ptr = std::move(classes.at(root2));
    classes.erase(root2);

    // Now unite the sets. After this, find(root2) will be root1.
    uf.unite(root1, root2);

    auto &class1_ref = classes.at(root1);

    pendings.insert(pendings.end(), class2_ptr->get_parents().begin(), class2_ptr->get_parents().end());

    // Efficiently move the nodes and parents from class2 to class1
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
        if (memo.find(node) != memo.end())
        {
            Id existing_class_id = memo.at(node);
            union_classes(existing_class_id, pending_id);
        }
        memo.emplace(node, pending_id);
    }
}

Id EGraph::add_class(const ENode &node, const ENode &original)
{
    auto new_id = uf.make_set();
    auto new_class = std::make_unique<EClass>(new_id);
    new_class->get_nodes().push_back(&node);
    nodes.push_back(std::make_unique<ENode>(original));
    for (auto child_id : node.get_children())
    {
        classes.at(child_id)->get_parents().push_back(new_id);
    }
    memo[&node] = new_id;
    classes.emplace(new_id, std::move(new_class));
    return new_id;
}
