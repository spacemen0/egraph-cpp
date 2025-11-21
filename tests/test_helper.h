#include <e_node.h>
static ENode make_symbol(const std::string &name)
{
    return ENode(Op::Symbol, Children{}, std::string(name));
}

static ENode make_leaf(Op op)
{
    return ENode(op, Children{}, std::monostate{});
}

static ENode make_op(Op op, const Children &children)
{
    return ENode(op, children, std::monostate{});
}