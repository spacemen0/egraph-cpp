#include <stdexcept>
#include <sstream>
#include "expression.h"
#include "utils.h"

/// @brief parse a expression from a string with the format: Op(child1, child2, ...)
/// @param string
Expression::Expression(std::string_view string)
{
    auto parsed = string_to_parsed_atom(string);
    atom = parsed.atom;
    for (const auto &child_str : parsed.children_strings)
    {
        children.emplace_back(child_str); // recursively parse child expressions
    }
}

std::string Expression::to_string() const
{
    std::stringstream ss;
    std::string atom_str = atom_to_string(this->atom);

    if (children.empty())
    {
        return atom_str;
    }

    ss << atom_str << "(";
    for (size_t i = 0; i < children.size(); ++i)
    {
        ss << children[i].to_string();
        if (i < children.size() - 1)
        {
            ss << ", ";
        }
    }
    ss << ")";
    return ss.str();
}