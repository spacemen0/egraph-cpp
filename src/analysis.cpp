#include "analysis.h"
#include "e_graph.h"
#include "errors.h"
#include <variant>

static void enforce_hierarchy(MatrixProperty &p)
{
    if (p.is_zero)
    {
        p.is_diagonal = true;
        p.is_identity = false;
        p.is_singular = true;
    }
    if (p.is_identity)
    {
        p.is_diagonal = true;
        p.is_orthogonal = true;
        p.is_singular = false;
    }
    if (p.is_diagonal)
    {
        p.is_upper_triangular = true;
        p.is_lower_triangular = true;
        p.is_symmetric = true;
    }
}

static void check_arity(const std::vector<Id> &children, size_t expected, const char *op_name)
{
    if (children.size() != expected)
    {
        throw AnalysisError(std::string(op_name) + " expects " + std::to_string(expected) + " children, got " + std::to_string(children.size()));
    }
}

AnalysisData MatrixAnalysis::make(const EGraph &egraph, const ENode &node)
{
    const auto atom = node.get_atom();
    MatrixProperty prop;
    const auto &children = node.get_children();
    auto get_data = [&](Id id) -> const MatrixProperty *
    {
        return std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id).property);
    };

    if (const auto op = std::get_if<Op>(&atom))
    {
        switch (*op)
        {
            using enum Op;
        case Add:
        {
            check_arity(children, 2, "Add");
            if (auto data1 = get_data(children.at(0)))
            {
                if (auto data2 = get_data(children.at(1)))
                {
                    if (data1->shape != data2->shape)
                        throw ShapeMismatchError("Add size mismatch");
                    prop.shape = data1->shape;
                    prop.is_symmetric = data1->is_symmetric && data2->is_symmetric;
                    prop.is_upper_triangular = data1->is_upper_triangular && data2->is_upper_triangular;
                    prop.is_lower_triangular = data1->is_lower_triangular && data2->is_lower_triangular;
                    prop.is_diagonal = data1->is_diagonal && data2->is_diagonal;
                    prop.is_zero = data1->is_zero && data2->is_zero;
                    break;
                }
            }
            throw AnalysisError("Add expects Matrix inputs");
        }
        case Mul:
        {
            check_arity(children, 2, "Mul");
            if (auto data1 = get_data(children.at(0)))
            {
                if (auto data2 = get_data(children.at(1)))
                {
                    if (data1->shape.second != data2->shape.first)
                    {
                        throw ShapeMismatchError("Mul operation with incompatible sizes");
                    }

                    prop.shape = std::make_pair(data1->shape.first, data2->shape.second);
                    prop.is_identity = data1->is_identity && data2->is_identity;
                    prop.is_permutation = data1->is_permutation && data2->is_permutation;
                    prop.is_singular = data1->is_singular || data2->is_singular;
                    prop.is_zero = data1->is_zero || data2->is_zero;
                    prop.is_lower_triangular = data1->is_lower_triangular && data2->is_lower_triangular;
                    prop.is_upper_triangular = data1->is_upper_triangular && data2->is_upper_triangular;
                    prop.is_orthogonal = data1->is_orthogonal && data2->is_orthogonal;
                    prop.is_diagonal = data1->is_diagonal && data2->is_diagonal;
                    break;
                }
            }
            throw AnalysisError("Mul expects Matrix inputs");
        }
        case Transpose:
        {
            check_arity(children, 1, "Transpose");
            if (auto data = get_data(children.at(0)))
            {
                auto child_size = data->shape;
                prop = *data;

                prop.shape = std::make_pair(child_size.second, child_size.first);
                prop.is_lower_triangular = data->is_upper_triangular;
                prop.is_upper_triangular = data->is_lower_triangular;
                break;
            }
            throw AnalysisError("Transpose expects a Matrix input");
        }
        case Invert:
        {
            check_arity(children, 1, "Invert");
            if (auto data = get_data(children.at(0)))
            {
                if (data->shape.first != data->shape.second)
                {
                    throw InvalidOperationError("Invert operation on non-square matrix");
                }
                if (data->is_singular)
                {
                    throw InvalidOperationError("Invert operation on singular matrix");
                }

                prop = *data;
                break;
            }
            throw AnalysisError("Invert expects a Matrix input");
        }
        case Negate:
        {
            check_arity(children, 1, "Negate");
            if (auto data = get_data(children.at(0)))
            {
                prop = *data;

                prop.is_identity = false;
                prop.is_permutation = false;
                prop.is_positive_definite = false;
                break;
            }
            throw AnalysisError("Negate expects a Matrix input");
        }
        case QR:
        {
            check_arity(children, 1, "QR");
            if (auto *data = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(children.at(0)).property))
            {
                MatrixProperty Q;
                MatrixProperty R;

                Q.shape = std::make_pair(data->shape.first, data->shape.first);
                Q.is_orthogonal = true;

                R.shape = data->shape;
                R.is_upper_triangular = true;
                if (data->is_wide)
                {
                    R.is_wide = true;
                }
                if (data->is_tall)
                {
                    R.is_tall = true;
                }
                if (data->is_identity)
                {
                    Q.is_identity = true;
                    R.is_identity = true;
                }
                if (data->is_zero)
                {
                    R.is_zero = true;
                }

                return AnalysisData{TupleProperty{Q, R}};
            }
            throw AnalysisError("QR expects a Matrix input");
        }
        case LU:
            throw AnalysisError("LU analysis not implemented yet");
        case LLt:
            throw AnalysisError("LLt analysis not implemented yet");
        case Get:
        {
            check_arity(children, 2, "Get");
            auto tuple_data = egraph.get_class_analysis_data(children.at(0));

            const auto &index_nodes = egraph.get_class_nodes(children.at(1));
            if (index_nodes.empty())
                throw AnalysisError("Get index has no nodes");

            const Atom &index_atom = index_nodes[0]->get_atom();
            if (const int *idx = std::get_if<int>(&index_atom))
            {
                if (auto *props = std::get_if<TupleProperty>(&tuple_data.property))
                {
                    if (*idx >= 0 && *idx < props->size())
                    {
                        return AnalysisData{(*props)[*idx]};
                    }
                    throw AnalysisError("Get index out of bounds");
                }
                throw AnalysisError("Get called on non-tuple");
            }
            throw AnalysisError("Get index must be a constant integer");
        }
        case Solve:
        {
            check_arity(children, 2, "Solve");
            if (auto data1 = get_data(children.at(0)))
            {
                if (auto data2 = get_data(children.at(1)))
                {
                    if (data1->shape.second != data2->shape.first)
                    {
                        throw ShapeMismatchError("Solve operation with incompatible sizes");
                    }
                    prop.shape = data2->shape;
                    break;
                }
            }
            throw AnalysisError("Solve expects Matrix inputs");
        }

        case TriangularSolve:
        {
            check_arity(children, 2, "TriangularSolve");
            if (auto data1 = get_data(children.at(0)))
            {
                if (auto data2 = get_data(children.at(1)))
                {
                    if (data1->shape.second != data2->shape.first)
                    {
                        throw ShapeMismatchError("TriangularSolve operation with incompatible sizes");
                    }
                    if (!data1->is_lower_triangular && !data1->is_upper_triangular)
                    {
                        throw InvalidOperationError("TriangularSolve operation on non-triangular matrix");
                    }
                    prop.shape = data2->shape;
                    break;
                }
            }
        }
        case Determinant:
            return AnalysisData{};
        case Log:
            return AnalysisData{};
        default:
            throw AnalysisError("Unknown operation in analysis");
        }
    }

    else if (const auto *s = std::get_if<std::string>(&atom))
    {
        if (egraph.get_property_table().has_property(*s))
        {
            auto property = egraph.get_property_table().get_property(*s).value();
            enforce_hierarchy(property);
            return AnalysisData{property};
        }
        throw AnalysisError("Variable has no property: " + *s);
    }

    enforce_hierarchy(prop);
    return AnalysisData{prop};
}

void MatrixAnalysis::merge(AnalysisData &data1, const AnalysisData &data2)
{
    if (data1 != data2)
    {
        throw ShapeMismatchError("Merging e-classes with conflicting size data");
    }

    auto merge_matrix_props = [](MatrixProperty &p1, const MatrixProperty &p2)
    {
        p1.is_symmetric = p1.is_symmetric || p2.is_symmetric;
        p1.is_orthogonal = p1.is_orthogonal || p2.is_orthogonal;
        p1.is_identity = p1.is_identity || p2.is_identity;
        p1.is_zero = p1.is_zero || p2.is_zero;
        p1.is_upper_triangular = p1.is_upper_triangular || p2.is_upper_triangular;
        p1.is_lower_triangular = p1.is_lower_triangular || p2.is_lower_triangular;
        p1.is_diagonal = p1.is_diagonal || p2.is_diagonal;
        p1.is_positive_definite = p1.is_positive_definite || p2.is_positive_definite;
        p1.is_singular = p1.is_singular || p2.is_singular;
        p1.is_permutation = p1.is_permutation || p2.is_permutation;
    };

    if (auto *p1 = std::get_if<MatrixProperty>(&data1.property))
    {
        const auto *p2 = std::get_if<MatrixProperty>(&data2.property);
        merge_matrix_props(*p1, *p2);
    }
    else if (auto *t1 = std::get_if<TupleProperty>(&data1.property))
    {
        const auto *t2 = std::get_if<TupleProperty>(&data2.property);
        for (size_t i = 0; i < t1->size(); ++i)
        {
            merge_matrix_props((*t1)[i], (*t2)[i]);
        }
    }
}