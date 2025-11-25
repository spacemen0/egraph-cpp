#include <e_node.h>
static ENode make_symbol(const std::string &name)
{
    return ENode(Children{}, std::string(name));
}

static ENode make_leaf(Op op)
{
    return ENode(Children{}, op);
}

static ENode make_op(Op op, const Children &children)
{
    return ENode(children, op);
}