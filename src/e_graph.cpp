#include "e_graph.h"

std::optional<Id> EGraph::look_up(ENode &node)
{
    for (auto i : node.get_children_mut())
    {
        i = uf.find_and_compress(i);
    }
    return memo.find(node) != memo.end() ? std::optional<Id>(memo.at(node)) : std::nullopt;
}

Id EGraph::add_node(ENode &node)
{
    if (look_up(node).has_value())
    {
        return look_up(node).value();
    }
    Id class_id = add_class(node, node);
    return class_id;
}

bool EGraph::union_classes(Id id1, Id id2)
{
    Id root1 = uf.find_and_compress(id1);
    Id root2 = uf.find_and_compress(id2);
    if (root1 == root2)
    {
        return false;
    }
    auto root1_parents = classes[root1].get_parents();
    auto root2_parents = classes[root2].get_parents();
    if (root1_parents.size() < root2_parents.size())
    {
        std::swap(root1, root2);
        std::swap(root1_parents, root2_parents);
    }
    uf.unite(root1, root2);
    auto class2 = classes.erase(root2);
    auto &class1 = classes[root1];
    pendings.insert(pendings.end(), root2_parents.begin(), root2_parents.end());
    class1.get_nodes().insert(class1.get_nodes().end(),
                              classes[root2].get_nodes().begin(),
                              classes[root2].get_nodes().end());
    class1.get_parents().insert(class1.get_parents().end(),
                                classes[root2].get_parents().begin(),
                                classes[root2].get_parents().end());
    return true;
}

void EGraph::rebuild()
{
}

Id EGraph::add_class(const ENode &node, const ENode &original)
{
    auto new_id = uf.make_set();
    EClass new_class{new_id};
    new_class.get_nodes().push_back(node);
    nodes.push_back(original);
    for (auto child_id : node.get_children())
    {
        classes[child_id].get_parents().push_back(new_id);
    }
    memo[node] = new_id;
    classes[new_id] = new_class;
    return new_id;
}
