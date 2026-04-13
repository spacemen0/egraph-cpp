#include "analysis.h"
#include "e_graph.h"
#include "errors.h"
#include "utils.h"
#include <variant>

AnalysisData MatrixAnalysis::make(const EGraph &egraph, const ENode &node) {
    const auto atom = node.get_atom();

    if (const auto op = std::get_if<Op>(&atom)) {
        return analyze_matrix_op(egraph, node, *op);
    }

    else if (const auto *s = std::get_if<std::string>(&atom)) {
        if (egraph.get_property_table().has_property(*s)) {
            auto property = egraph.get_property_table().get_property(*s).value();
            enforce_hierarchy(property);
            return AnalysisData{property};
        }
        throw AnalysisError("Variable has no property: " + *s);
    } else {
        return AnalysisData{"Const"};
    }
}

void MatrixAnalysis::merge(AnalysisData &data1, const AnalysisData &data2) {
    if (data1 != data2) // has a custom operator ==
    {
        throw ShapeMismatchError("Merging e-classes with conflicting size data");
    }

    auto merge_matrix_props = [](MatrixProperty &p1, const MatrixProperty &p2) {
        for (const auto &flag : MatrixProperty::flag_descriptors) {
            p1.flags.*(flag.member) = p1.flags.*(flag.member) || p2.flags.*(flag.member);
        }
    };

    if (auto *p1 = std::get_if<MatrixProperty>(&data1.property)) {
        const auto *p2 = std::get_if<MatrixProperty>(&data2.property);
        if (!p2) {
            throw AnalysisError(
                "Cannot merge MatrixProperty with non-MatrixProperty "
                "(likely TupleProperty)");
        }
        merge_matrix_props(*p1, *p2);
    } else if (auto *t1 = std::get_if<TupleProperty>(&data1.property)) {
        const auto *t2 = std::get_if<TupleProperty>(&data2.property);
        if (!t2) {
            throw AnalysisError("Cannot merge TupleProperty with non-TupleProperty");
        }
        if (t1->size() != t2->size()) {
            throw AnalysisError("Cannot merge TupleProperties of different sizes");
        }
        for (size_t i = 0; i < t1->size(); ++i) {
            merge_matrix_props((*t1)[i], (*t2)[i]);
        }
    }
}

void MatrixAnalysis::enforce_hierarchy(MatrixProperty &p) {
    if (p.flags.is_zero) {
        p.flags.is_diagonal = true;
        p.flags.is_identity = false;
        p.flags.is_singular = true;
    }
    if (p.flags.is_identity) {
        p.flags.is_diagonal = true;
        p.flags.is_orthogonal = true;
        p.flags.is_singular = false;
    }
    if (p.flags.is_diagonal) {
        p.flags.is_upper_triangular = true;
        p.flags.is_lower_triangular = true;
        p.flags.is_symmetric = true;
    }
}

static void check_arity(const std::vector<Id> &children, size_t expected, const char *op_name) {
    if (children.size() != expected) {
        throw AnalysisError(
            std::string(op_name) + " expects " + std::to_string(expected) + " children, got " +
            std::to_string(children.size()));
    }
}

static AnalysisData matrix_property_data(MatrixProperty prop) {
    MatrixAnalysis::enforce_hierarchy(prop);
    return AnalysisData{prop};
}

static AnalysisData analyze_add(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Add");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape != data2->shape)
                throw ShapeMismatchError("Add size mismatch");

            MatrixProperty prop;
            prop.shape = data1->shape;
            prop.flags.is_symmetric = data1->flags.is_symmetric && data2->flags.is_symmetric;
            prop.flags.is_upper_triangular = data1->flags.is_upper_triangular && data2->flags.is_upper_triangular;
            prop.flags.is_lower_triangular = data1->flags.is_lower_triangular && data2->flags.is_lower_triangular;
            prop.flags.is_diagonal = data1->flags.is_diagonal && data2->flags.is_diagonal;
            prop.flags.is_zero = data1->flags.is_zero && data2->flags.is_zero;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Add expects Matrix inputs");
}

static AnalysisData analyze_mul(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Mul");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape.second != data2->shape.first) {
                throw ShapeMismatchError("Mul operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = std::make_pair(data1->shape.first, data2->shape.second);
            prop.flags.is_identity = data1->flags.is_identity && data2->flags.is_identity;
            prop.flags.is_permutation = data1->flags.is_permutation && data2->flags.is_permutation;
            prop.flags.is_singular = data1->flags.is_singular || data2->flags.is_singular;
            prop.flags.is_zero = data1->flags.is_zero || data2->flags.is_zero;
            prop.flags.is_lower_triangular = data1->flags.is_lower_triangular && data2->flags.is_lower_triangular;
            prop.flags.is_upper_triangular = data1->flags.is_upper_triangular && data2->flags.is_upper_triangular;
            prop.flags.is_orthogonal = data1->flags.is_orthogonal && data2->flags.is_orthogonal;
            prop.flags.is_diagonal = data1->flags.is_diagonal && data2->flags.is_diagonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Mul expects Matrix inputs");
}

static AnalysisData analyze_transpose(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "Tr");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        auto child_size = data->shape;
        MatrixProperty prop = *data;

        prop.shape = std::make_pair(child_size.second, child_size.first);
        prop.flags.is_lower_triangular = data->flags.is_upper_triangular;
        prop.flags.is_upper_triangular = data->flags.is_lower_triangular;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Tr expects a Matrix input");
}

static AnalysisData analyze_invert(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "Inv");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (data->shape.first != data->shape.second) {
            throw InvalidOperationError("Inv operation on non-square matrix");
        }
        if (data->flags.is_singular) {
            throw InvalidOperationError("Inv operation on singular matrix");
        }

        MatrixProperty prop = *data;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Inv expects a Matrix input");
}

static AnalysisData analyze_negate(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "Neg");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        MatrixProperty prop = *data;
        prop.flags.is_identity = false;
        prop.flags.is_permutation = false;
        prop.flags.is_positive_definite = false;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Neg expects a Matrix input");
}

static AnalysisData analyze_qr(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "QR");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        MatrixProperty Q;
        MatrixProperty R;

        Q.flags.is_orthonormal = true;
        R.flags.is_upper_triangular = true;

        if (data->is_square()) {
            Q.shape = data->shape;
            R.shape = data->shape;
            Q.flags.is_orthogonal = true;
        } else if (data->is_tall_matrix()) {
            Q.shape = data->shape;
            Q.flags.is_tall = true;
            R.shape = std::make_pair(data->shape.second, data->shape.second);
        } else if (data->is_wide_matrix()) {
            Q.shape = std::make_pair(data->shape.first, data->shape.first);
            Q.flags.is_orthogonal = true;
            R.shape = data->shape;
            R.flags.is_wide = true;
        } else {
            throw InvalidOperationError("QR operation on symbolic matrix with ambiguous shape");
        }

        if (data->flags.is_identity) {
            Q.flags.is_identity = true;
            R.flags.is_identity = true;
        }
        if (data->flags.is_zero) {
            R.flags.is_zero = true;
        }

        return AnalysisData{TupleProperty{Q, R}};
    }
    throw AnalysisError("QR expects a Matrix input");
}

static AnalysisData analyze_get(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Get");
    auto tuple_data = egraph.get_class_analysis_data(children.at(0));

    const auto &index_nodes = egraph.get_class_nodes(children.at(1));
    if (index_nodes.empty())
        throw AnalysisError("Get index has no nodes");

    const Atom &index_atom = index_nodes[0]->get_atom();
    if (const int *idx = std::get_if<int>(&index_atom)) {
        if (auto *props = std::get_if<TupleProperty>(&tuple_data.property)) {
            if (*idx >= 0 && *idx < props->size()) {
                return AnalysisData{(*props)[*idx]};
            }
            throw AnalysisError("Get index out of bounds");
        }
        throw AnalysisError("Get called on non-tuple");
    }
    throw AnalysisError("Get index must be a constant integer");
}

static AnalysisData analyze_solve(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Sol");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape.second != data2->shape.first) {
                throw ShapeMismatchError("Sol operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = data2->shape;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Sol expects Matrix inputs");
}

static AnalysisData analyze_triangular_solve(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "TriSol");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape.second != data2->shape.first) {
                throw ShapeMismatchError("TriSol operation with incompatible sizes");
            }
            if (!data1->flags.is_lower_triangular && !data1->flags.is_upper_triangular) {
                throw InvalidOperationError("TriSol operation on non-triangular matrix");
            }

            MatrixProperty prop;
            prop.shape = data2->shape;
            return matrix_property_data(prop);
        }
    }

    return AnalysisData{};
}

static AnalysisData analyze_lu(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "LU");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (data->shape.first != data->shape.second) {
            throw InvalidOperationError("LU operation on non-square matrix");
        }

        MatrixProperty L;
        MatrixProperty U;
        MatrixProperty P;

        L.shape = data->shape;
        L.flags.is_lower_triangular = true;

        U.shape = data->shape;
        U.flags.is_upper_triangular = true;

        P.shape = data->shape;
        P.flags.is_permutation = true;

        return AnalysisData{TupleProperty{L, U, P}};
    }
    throw AnalysisError("LU expects a Matrix input");
}

static AnalysisData analyze_llt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "LLt");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (data->shape.first != data->shape.second) {
            throw InvalidOperationError("LLt operation on non-square matrix");
        }
        if (!data->flags.is_positive_definite) {
            throw InvalidOperationError("LLt operation on non-positive-definite matrix");
        }
        if (!data->flags.is_symmetric) {
            throw InvalidOperationError("LLt operation on non-symmetric matrix");
        }
        MatrixProperty L;
        L.shape = data->shape;
        L.flags.is_lower_triangular = true;

        return AnalysisData{TupleProperty{L}};
    }
    throw AnalysisError("LLt expects a Matrix input");
}

AnalysisData MatrixAnalysis::analyze_matrix_op(const EGraph &egraph, const ENode &node, Op op) {
    const auto &children = node.get_children();

    switch (op) {
        using enum Op;
    case Add:
        return analyze_add(egraph, children);
    case Mul:
        return analyze_mul(egraph, children);
    case Tr:
        return analyze_transpose(egraph, children);
    case Inv:
        return analyze_invert(egraph, children);
    case Neg:
        return analyze_negate(egraph, children);
    case QR:
        return analyze_qr(egraph, children);
    case LU:
        return analyze_lu(egraph, children);
    case LLt:
        return analyze_llt(egraph, children);
    case Get:
        return analyze_get(egraph, children);
    case Sol:
        return analyze_solve(egraph, children);
    case TriSol:
        return analyze_triangular_solve(egraph, children);
    case Det:
        return AnalysisData{};
    case Log:
        return AnalysisData{};
    default:
        throw AnalysisError("Unknown operation in analysis");
    }
}