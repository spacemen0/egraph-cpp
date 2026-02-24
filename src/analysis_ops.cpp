#include "analysis_ops.h"
#include "e_graph.h"
#include "errors.h"
#include <variant>

void enforce_hierarchy(MatrixProperty &p)
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

static const MatrixProperty *get_matrix_data(const EGraph &egraph, Id id)
{
    return std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(id).property);
}

static AnalysisData matrix_property_data(MatrixProperty prop)
{
    enforce_hierarchy(prop);
    return AnalysisData{prop};
}

static AnalysisData analyze_add(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 2, "Add");
    if (auto data1 = get_matrix_data(egraph, children.at(0)))
    {
        if (auto data2 = get_matrix_data(egraph, children.at(1)))
        {
            if (data1->shape != data2->shape)
                throw ShapeMismatchError("Add size mismatch");

            MatrixProperty prop;
            prop.shape = data1->shape;
            prop.is_symmetric = data1->is_symmetric && data2->is_symmetric;
            prop.is_upper_triangular = data1->is_upper_triangular && data2->is_upper_triangular;
            prop.is_lower_triangular = data1->is_lower_triangular && data2->is_lower_triangular;
            prop.is_diagonal = data1->is_diagonal && data2->is_diagonal;
            prop.is_zero = data1->is_zero && data2->is_zero;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Add expects Matrix inputs");
}

static AnalysisData analyze_mul(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 2, "Mul");
    if (auto data1 = get_matrix_data(egraph, children.at(0)))
    {
        if (auto data2 = get_matrix_data(egraph, children.at(1)))
        {
            if (data1->shape.second != data2->shape.first)
            {
                throw ShapeMismatchError("Mul operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = std::make_pair(data1->shape.first, data2->shape.second);
            prop.is_identity = data1->is_identity && data2->is_identity;
            prop.is_permutation = data1->is_permutation && data2->is_permutation;
            prop.is_singular = data1->is_singular || data2->is_singular;
            prop.is_zero = data1->is_zero || data2->is_zero;
            prop.is_lower_triangular = data1->is_lower_triangular && data2->is_lower_triangular;
            prop.is_upper_triangular = data1->is_upper_triangular && data2->is_upper_triangular;
            prop.is_orthogonal = data1->is_orthogonal && data2->is_orthogonal;
            prop.is_diagonal = data1->is_diagonal && data2->is_diagonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Mul expects Matrix inputs");
}

static AnalysisData analyze_transpose(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 1, "Transpose");
    if (auto data = get_matrix_data(egraph, children.at(0)))
    {
        auto child_size = data->shape;
        MatrixProperty prop = *data;

        prop.shape = std::make_pair(child_size.second, child_size.first);
        prop.is_lower_triangular = data->is_upper_triangular;
        prop.is_upper_triangular = data->is_lower_triangular;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Transpose expects a Matrix input");
}

static AnalysisData analyze_invert(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 1, "Invert");
    if (auto data = get_matrix_data(egraph, children.at(0)))
    {
        if (data->shape.first != data->shape.second)
        {
            throw InvalidOperationError("Invert operation on non-square matrix");
        }
        if (data->is_singular)
        {
            throw InvalidOperationError("Invert operation on singular matrix");
        }

        MatrixProperty prop = *data;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Invert expects a Matrix input");
}

static AnalysisData analyze_negate(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 1, "Negate");
    if (auto data = get_matrix_data(egraph, children.at(0)))
    {
        MatrixProperty prop = *data;
        prop.is_identity = false;
        prop.is_permutation = false;
        prop.is_positive_definite = false;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Negate expects a Matrix input");
}

static AnalysisData analyze_qr(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 1, "QR");
    if (auto *data = std::get_if<MatrixProperty>(&egraph.get_class_analysis_data(children.at(0)).property))
    {
        MatrixProperty Q;
        MatrixProperty R;

        Q.is_orthonormal = true;
        R.is_upper_triangular = true;

        if (data->is_tall_matrix())
        {
            Q.shape = data->shape;
            Q.is_tall = true;
            R.shape = std::make_pair(data->shape.second, data->shape.second);
        }
        else if (data->is_wide_matrix())
        {
            Q.shape = std::make_pair(data->shape.first, data->shape.first);
            Q.is_orthogonal = true;
            R.shape = data->shape;
            R.is_wide = true;
        }
        else
        {
            Q.shape = data->shape;
            R.shape = data->shape;
            Q.is_orthogonal = true;
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

static AnalysisData analyze_get(const EGraph &egraph, const std::vector<Id> &children)
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

static AnalysisData analyze_solve(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 2, "Solve");
    if (auto data1 = get_matrix_data(egraph, children.at(0)))
    {
        if (auto data2 = get_matrix_data(egraph, children.at(1)))
        {
            if (data1->shape.second != data2->shape.first)
            {
                throw ShapeMismatchError("Solve operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = data2->shape;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Solve expects Matrix inputs");
}

static AnalysisData analyze_triangular_solve(const EGraph &egraph, const std::vector<Id> &children)
{
    check_arity(children, 2, "TriangularSolve");
    if (auto data1 = get_matrix_data(egraph, children.at(0)))
    {
        if (auto data2 = get_matrix_data(egraph, children.at(1)))
        {
            if (data1->shape.second != data2->shape.first)
            {
                throw ShapeMismatchError("TriangularSolve operation with incompatible sizes");
            }
            if (!data1->is_lower_triangular && !data1->is_upper_triangular)
            {
                throw InvalidOperationError("TriangularSolve operation on non-triangular matrix");
            }

            MatrixProperty prop;
            prop.shape = data2->shape;
            return matrix_property_data(prop);
        }
    }

    return AnalysisData{};
}

AnalysisData analyze_matrix_op(const EGraph &egraph, const ENode &node, Op op)
{
    const auto &children = node.get_children();

    switch (op)
    {
        using enum Op;
    case Add:
        return analyze_add(egraph, children);
    case Mul:
        return analyze_mul(egraph, children);
    case Transpose:
        return analyze_transpose(egraph, children);
    case Invert:
        return analyze_invert(egraph, children);
    case Negate:
        return analyze_negate(egraph, children);
    case QR:
        return analyze_qr(egraph, children);
    case LU:
        throw AnalysisError("LU analysis not implemented yet");
    case LLt:
        throw AnalysisError("LLt analysis not implemented yet");
    case Get:
        return analyze_get(egraph, children);
    case Solve:
        return analyze_solve(egraph, children);
    case TriangularSolve:
        return analyze_triangular_solve(egraph, children);
    case Determinant:
        return AnalysisData{};
    case Log:
        return AnalysisData{};
    default:
        throw AnalysisError("Unknown operation in analysis");
    }
}
