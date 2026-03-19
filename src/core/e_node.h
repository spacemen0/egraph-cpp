#pragma once

#include "types.h"
#include <string>
#include <variant>

class EGraph;

class ENode {
  private:
    Children children;
    Atom atom;

  public:
    explicit ENode(const Children &children, Atom const &atom) : children(children), atom(atom) {}
    Cost compute_local_cost(const EGraph &egraph, const SizeBindings *size_bindings = nullptr) const;

    // access children (mutable/immutable)
    const Children &get_children() const;
    Children &get_children_mut();
    Atom get_atom() const;

    std::string to_string() const;
    std::string format() const;
    size_t hash() const;
    bool is_leaf() const;
    bool has_ancestor(std::string_view ancestor_op, const EGraph &egraph) const;

    // Declare operator== as a hidden friend
    friend bool operator==(const ENode &a, const ENode &b) noexcept {
        if (a.atom != b.atom)
            return false;
        if (a.children.size() != b.children.size())
            return false;
        for (size_t i = 0; i < a.children.size(); ++i) {
            if (a.children[i] != b.children[i])
                return false;
        }
        return true;
    };
};

struct ENodePtrHash {
    size_t operator()(const ENode *e) const noexcept { return e->hash(); }
};

struct ENodePtrEqual {
    bool operator()(const ENode *a, const ENode *b) const noexcept { return *a == *b; }
};