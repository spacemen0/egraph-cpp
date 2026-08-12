#include "analysis.h"
#include "e_graph.h"
#include "errors.h"
#include "expression.h"
#include "property_table.h"
#include "utils.h"
#include <variant>

AnalysisData MatrixAnalysis::make(const EGraph &egraph, const ENode &node) {
    const auto atom = node.get_atom();

    if (const auto op = std::get_if<Op>(&atom)) {
        return analyze_matrix_op(egraph, node, *op);
    }

    else if (const auto *a = std::get_if<uint32_t>(&atom)) {
        const auto s = get_string_from_lookup(*a);
        if (egraph.get_property_table().has_property(s)) {
            auto property = egraph.get_property_table().get_property(s).value();
            enforce_hierarchy(property);
            return AnalysisData{property};
        }
        throw AnalysisError("Variable has no property: " + s);
    } else {
        return {};
    }
}

bool MatrixAnalysis::merge(AnalysisData &data1, const AnalysisData &data2) {
    if (data1 != data2) // has a custom operator ==
    {
        throw ShapeMismatchError("Merging e-classes with conflicting size data");
    }

    auto merge_matrix_props = [](MatrixProperty &p1, const MatrixProperty &p2) {
        MatrixProperty old_p1 = p1;
        for (const auto &flag : MatrixProperty::flag_descriptors) {
            p1.flags.*(flag.member) = p1.flags.*(flag.member) || p2.flags.*(flag.member);
        }
        enforce_hierarchy(p1);
        return !p1.strict_equal(old_p1);
    };

    bool changed = false;
    if (auto *p1 = std::get_if<MatrixProperty>(&data1.property)) {
        const auto *p2 = std::get_if<MatrixProperty>(&data2.property);
        if (!p2) {
            throw AnalysisError("Cannot merge MatrixProperty with non-MatrixProperty (likely TupleProperty)");
        }
        return merge_matrix_props(*p1, *p2);
    } else if (auto *t1 = std::get_if<TupleProperty>(&data1.property)) {
        const auto *t2 = std::get_if<TupleProperty>(&data2.property);
        if (!t2) {
            throw AnalysisError("Cannot merge TupleProperty with non-TupleProperty");
        }
        if (t1->size() != t2->size()) {
            throw AnalysisError("Cannot merge TupleProperties of different sizes");
        }
        for (size_t i = 0; i < t1->size(); ++i) {
            if (merge_matrix_props((*t1)[i], (*t2)[i])) {
                changed = true;
            }
        }
    }
    return changed;
}

void MatrixAnalysis::enforce_hierarchy(MatrixProperty &p) {
    if (p.flags.is_permutation) {
        p.flags.is_orthogonal = true;
    }

    if (p.flags.is_orthogonal) {
        p.flags.has_orthonormal_columns = true;
        p.flags.is_non_singular = true;
        p.flags.is_full_rank = true;
    }

    if (p.is_square() && p.flags.has_orthonormal_columns) {
        p.flags.is_orthogonal = true;
        p.flags.is_non_singular = true;
    }

    if (p.flags.has_orthonormal_columns) {
        p.flags.is_full_rank = true;
    }

    if (p.flags.is_upper_triangular && p.flags.is_lower_triangular) {
        p.flags.is_diagonal = true;
    }

    if (p.flags.is_symmetric && (p.flags.is_upper_triangular || p.flags.is_lower_triangular)) {
        p.flags.is_diagonal = true;
    }

    if (p.flags.is_zero) {
        p.flags.is_diagonal = true;
        p.flags.is_non_singular = false;
    }

    if (p.flags.is_identity) {
        p.flags.is_diagonal = true;
        p.flags.is_orthogonal = true;
        p.flags.is_positive_definite = true;
        p.flags.is_positive_semi_definite = true;
        p.flags.is_non_singular = true;
        p.flags.is_full_rank = true;
    }

    if (p.flags.is_positive_definite) {
        p.flags.is_positive_semi_definite = true;
        p.flags.is_symmetric = true;
        p.flags.is_non_singular = true;
        p.flags.is_full_rank = true;
    }

    if (p.flags.is_diagonal) {
        p.flags.is_upper_triangular = true;
        p.flags.is_lower_triangular = true;
        p.flags.is_symmetric = true;
    }

    if (p.is_square() && p.flags.is_full_rank) {
        p.flags.is_non_singular = true;
    }

    if (p.flags.is_non_singular) {
        p.flags.is_full_rank = true;
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

static AnalysisData tuple_property_data(std::vector<MatrixProperty> &props) {
    for (auto &prop : props) {
        MatrixAnalysis::enforce_hierarchy(prop);
    }
    return AnalysisData{TupleProperty(props)};
}

static AnalysisData analyze_add(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "+");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape != data2->shape)
                throw ShapeMismatchError("+ size mismatch");

            MatrixProperty prop;
            prop.shape = data1->shape;
            prop.flags.is_symmetric = data1->flags.is_symmetric && data2->flags.is_symmetric;
            prop.flags.is_upper_triangular = data1->flags.is_upper_triangular && data2->flags.is_upper_triangular;
            prop.flags.is_lower_triangular = data1->flags.is_lower_triangular && data2->flags.is_lower_triangular;
            prop.flags.is_diagonal = data1->flags.is_diagonal && data2->flags.is_diagonal;
            prop.flags.is_zero = data1->flags.is_zero && data2->flags.is_zero;

            if (data1->flags.is_positive_definite && data2->flags.is_positive_definite) {
                prop.flags.is_positive_definite = true;
                prop.flags.is_non_singular = true;
                prop.flags.is_full_rank = true;
            }
            prop.flags.is_full_rank = data1->flags.is_full_rank && data2->flags.is_full_rank;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("+ expects Matrix inputs");
}

static AnalysisData analyze_mul(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "*");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape.second != data2->shape.first) {
                throw ShapeMismatchError("* operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = std::make_pair(data1->shape.first, data2->shape.second);
            prop.flags.is_identity = data1->flags.is_identity && data2->flags.is_identity;
            prop.flags.is_permutation = data1->flags.is_permutation && data2->flags.is_permutation;
            prop.flags.is_non_singular = data1->flags.is_non_singular && data2->flags.is_non_singular;
            prop.flags.is_full_rank = data1->flags.is_full_rank && data2->flags.is_full_rank;
            prop.flags.is_zero = data1->flags.is_zero || data2->flags.is_zero;
            prop.flags.is_lower_triangular = data1->flags.is_lower_triangular && data2->flags.is_lower_triangular;
            prop.flags.is_upper_triangular = data1->flags.is_upper_triangular && data2->flags.is_upper_triangular;
            prop.flags.is_orthogonal = data1->flags.is_orthogonal && data2->flags.is_orthogonal;
            prop.flags.is_diagonal = data1->flags.is_diagonal && data2->flags.is_diagonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("* expects Matrix inputs");
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
        if (!data->flags.is_non_singular) {
            throw InvalidOperationError("Inv operation on singular matrix");
        }

        MatrixProperty prop = *data;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Inv expects a Matrix input");
}

static AnalysisData analyze_minus(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "-");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape != data2->shape)
                throw ShapeMismatchError("- size mismatch");

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
    throw AnalysisError("- expects Matrix inputs");
}

// Doing thin QR
static AnalysisData analyze_qr(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "QR");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        MatrixProperty Q;
        MatrixProperty R;

        Q.flags.has_orthonormal_columns = true;
        R.flags.is_upper_triangular = true;

        if (data->is_square()) {
            Q.shape = data->shape;
            R.shape = data->shape;
            Q.flags.is_orthogonal = true;
            if (data->flags.is_non_singular) {
                R.flags.is_non_singular = true;
            }
        } else if (data->is_tall_matrix() && data->flags.is_full_rank) {
            Q.shape = data->shape;
            Q.flags.is_tall = true;
            R.shape = std::make_pair(data->shape.second, data->shape.second);
            R.flags.is_non_singular = true;
        } else if (data->is_wide_matrix()) {
            Q.shape = std::make_pair(data->shape.first, data->shape.first);
            Q.flags.is_orthogonal = true;
            R.shape = data->shape;
            R.flags.is_wide = true;
            R.flags.is_upper_triangular = false; // is_upper_trapezoidal
            if (data->flags.is_full_rank) {
                R.flags.is_full_rank = true;
            }
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

        auto props = std::vector{Q, R};
        return tuple_property_data(props);
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
    if (const double *idx_ptr = std::get_if<double>(&index_atom)) {
        int idx = static_cast<int>(*idx_ptr);
        if (auto *props = std::get_if<TupleProperty>(&tuple_data.property)) {
            if (idx >= 0 && idx < props->size()) {
                return AnalysisData{(*props)[idx]};
            }
            throw AnalysisError("Get index out of bounds");
        }
        throw AnalysisError("Get called on non-tuple");
    }
    throw AnalysisError("Get index must be a constant number");
}

static AnalysisData analyze_solve(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Sol");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (!data1->is_square())
            throw InvalidOperationError(
                "Sol operation on non-square matrix " +
                Expression(egraph.find_node(children.at(0)).value(), egraph).to_string());
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data1->shape.second != data2->shape.first) {
                throw ShapeMismatchError("Sol operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = {data1->shape.first, data2->shape.second};
            prop.flags.is_full_rank = data2->flags.is_full_rank;
            prop.flags.is_non_singular = data2->flags.is_non_singular;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Sol expects Matrix inputs");
}

static AnalysisData analyze_solve_right(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "SolR");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (!data1->is_square())
            throw InvalidOperationError(
                "SolR operation on non-square matrix " +
                Expression(egraph.find_node(children.at(0)).value(), egraph).to_string());
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data2->shape.second != data1->shape.first) {
                throw ShapeMismatchError("SolR operation with incompatible sizes");
            }

            MatrixProperty prop;
            prop.shape = {data2->shape.first, data1->shape.second};
            prop.flags.is_full_rank = data2->flags.is_full_rank;
            prop.flags.is_non_singular = data2->flags.is_non_singular;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("SolR expects Matrix inputs");
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
        L.flags.is_non_singular = true;

        U.shape = data->shape;
        U.flags.is_upper_triangular = true;
        if (data->flags.is_non_singular) {
            U.flags.is_non_singular = true;
        }

        P.shape = data->shape;
        P.flags.is_permutation = true;
        auto props = std::vector{L, U, P};

        return tuple_property_data(props);
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
        L.flags.is_non_singular = true;
        auto props = std::vector{L};
        return tuple_property_data(props);
    }
    throw AnalysisError("LLt expects a Matrix input");
}

static AnalysisData analyze_utu(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "UtU");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (data->shape.first != data->shape.second) {
            throw InvalidOperationError("UtU operation on non-square matrix");
        }
        if (!data->flags.is_positive_definite) {
            throw InvalidOperationError("UtU operation on non-positive-definite matrix");
        }
        if (!data->flags.is_symmetric) {
            throw InvalidOperationError("UtU operation on non-symmetric matrix");
        }
        MatrixProperty U;
        U.shape = data->shape;
        U.flags.is_upper_triangular = true;
        U.flags.is_non_singular = true;
        auto props = std::vector{U};
        return tuple_property_data(props);
    }
    throw AnalysisError("UtU expects a Matrix input");
}

static AnalysisData analyze_gemm_nn(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 3, "Gemm_NN");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (auto data3 = get_matrix_data(egraph, children.at(2))) {
                if (data->shape.second != data2->shape.first || data2->shape.second != data3->shape.second ||
                    data->shape.first != data3->shape.first) {
                    throw ShapeMismatchError("Gemm_NN operation with incompatible sizes");
                }

                MatrixProperty prop;
                prop.shape = std::make_pair(data->shape.first, data3->shape.second);
                return matrix_property_data(prop);
            }
        }
    }
    throw AnalysisError("Gemm_NN expects Matrix inputs");
}

static AnalysisData analyze_gemm_tn(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 3, "Gemm_TN");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (auto data3 = get_matrix_data(egraph, children.at(2))) {
                if (data->shape.first != data2->shape.first || data2->shape.second != data3->shape.second ||
                    data->shape.second != data3->shape.first) {
                    throw ShapeMismatchError("Gemm_TN operation with incompatible sizes");
                }
                MatrixProperty prop;
                prop.shape = std::make_pair(data->shape.second, data2->shape.second);
                return matrix_property_data(prop);
            }
        }
    }
    throw AnalysisError("Gemm_TN expects Matrix inputs");
}

static AnalysisData analyze_gemm_nt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 3, "Gemm_NT");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (auto data3 = get_matrix_data(egraph, children.at(2))) {
                if (data->shape.second != data2->shape.second || data2->shape.first != data3->shape.second ||
                    data->shape.first != data3->shape.first) {
                    throw ShapeMismatchError("Gemm_NT operation with incompatible sizes");
                }
                MatrixProperty prop;
                prop.shape = std::make_pair(data->shape.first, data2->shape.first);
                return matrix_property_data(prop);
            }
        }
    }
    throw AnalysisError("Gemm_NT expects Matrix inputs");
}

static AnalysisData analyze_gemm_tt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 3, "Gemm_TT");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (auto data3 = get_matrix_data(egraph, children.at(2))) {
                if (data->shape.first != data2->shape.second || data2->shape.first != data3->shape.second ||
                    data->shape.second != data3->shape.first) {
                    throw ShapeMismatchError("Gemm_TT operation with incompatible sizes");
                }
                MatrixProperty prop;
                prop.shape = std::make_pair(data->shape.second, data2->shape.first);
                return matrix_property_data(prop);
            }
        }
    }
    throw AnalysisError("Gemm_TT expects Matrix inputs");
}

static AnalysisData analyze_syrk_n(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Syrk_N");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data->shape.first != data2->shape.first || data->shape.first != data2->shape.second) {
                throw ShapeMismatchError("Syrk_N operation with incompatible sizes");
            }
            MatrixProperty prop;
            prop.shape = std::make_pair(data->shape.first, data->shape.first);
            prop.flags.is_symmetric = true;
            if (data->flags.is_full_rank || data->flags.is_positive_definite) {
                prop.flags.is_positive_definite = true;
                prop.flags.is_full_rank = true;
                prop.flags.is_non_singular = true;
            }
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Syrk_N expects Matrix inputs");
}

static AnalysisData analyze_syrk_t(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Syrk_T");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (data->shape.second != data2->shape.first || data->shape.second != data2->shape.second) {
                throw ShapeMismatchError("Syrk_T operation with incompatible sizes");
            }
            MatrixProperty prop;
            prop.shape = std::make_pair(data->shape.second, data->shape.second);
            prop.flags.is_symmetric = true;
            if (data->flags.is_full_rank || data->flags.is_positive_definite) {
                prop.flags.is_positive_definite = true;
                prop.flags.is_full_rank = true;
                prop.flags.is_non_singular = true;
            }
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Syrk_T expects Matrix inputs");
}

static AnalysisData analyze_trsm_ln(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Trsm_LN");
    if (auto dataA = get_matrix_data(egraph, children.at(0))) {
        if (auto dataB = get_matrix_data(egraph, children.at(1))) {
            if (!dataA->flags.is_lower_triangular && !dataA->flags.is_upper_triangular) {
                throw InvalidOperationError("Trsm_LN operation on non-triangular matrix A");
            }
            if (dataA->shape.second != dataB->shape.first || !dataA->flags.is_non_singular) {
                throw ShapeMismatchError("Trsm_LN operation with incompatible sizes");
            }
            MatrixProperty prop;
            prop.shape = std::make_pair(dataA->shape.first, dataB->shape.second);
            prop.flags.is_full_rank = dataB->flags.is_full_rank;
            prop.flags.is_non_singular = dataB->flags.is_non_singular;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Trsm_LN expects Matrix inputs");
}

static AnalysisData analyze_trsm_lt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Trsm_LT");
    if (auto dataA = get_matrix_data(egraph, children.at(0))) {
        if (auto dataB = get_matrix_data(egraph, children.at(1))) {
            if (!dataA->flags.is_lower_triangular && !dataA->flags.is_upper_triangular) {
                throw InvalidOperationError("Trsm_LT operation on non-triangular matrix A");
            }
            if (dataA->shape.first != dataB->shape.first || !dataA->flags.is_non_singular) {
                throw ShapeMismatchError("Trsm_LT operation with incompatible sizes");
            }
            MatrixProperty prop;
            prop.shape = std::make_pair(dataA->shape.second, dataB->shape.second);
            prop.flags.is_full_rank = dataB->flags.is_full_rank;
            prop.flags.is_non_singular = dataB->flags.is_non_singular;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Trsm_LT expects Matrix inputs");
}

static AnalysisData analyze_trsm_rn(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Trsm_RN");
    if (auto dataA = get_matrix_data(egraph, children.at(0))) {
        if (auto dataB = get_matrix_data(egraph, children.at(1))) {
            if (!dataA->flags.is_lower_triangular && !dataA->flags.is_upper_triangular) {
                throw InvalidOperationError("Trsm_RN operation on non-triangular matrix A");
            }
            if (dataA->shape.first != dataB->shape.second || !dataA->flags.is_non_singular) {
                throw ShapeMismatchError("Trsm_RN operation with incompatible sizes");
            }
            MatrixProperty prop;
            prop.shape = std::make_pair(dataB->shape.first, dataA->shape.second);
            prop.flags.is_full_rank = dataB->flags.is_full_rank;
            prop.flags.is_non_singular = dataB->flags.is_non_singular;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Trsm_RN expects Matrix inputs");
}

static AnalysisData analyze_trsm_rt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Trsm_RT");
    if (auto dataA = get_matrix_data(egraph, children.at(0))) {
        if (auto dataB = get_matrix_data(egraph, children.at(1))) {
            if (!dataA->flags.is_lower_triangular && !dataA->flags.is_upper_triangular) {
                throw InvalidOperationError("Trsm_RT operation on non-triangular matrix A");
            }
            if (dataA->shape.second != dataB->shape.second || !dataA->flags.is_non_singular) {
                throw ShapeMismatchError("Trsm_RT operation with incompatible sizes");
            }
            MatrixProperty prop;
            prop.shape = std::make_pair(dataB->shape.first, dataA->shape.first);
            prop.flags.is_full_rank = dataB->flags.is_full_rank;
            prop.flags.is_non_singular = dataB->flags.is_non_singular;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Trsm_RT expects Matrix inputs");
}

static AnalysisData analyze_potrf_l(const EGraph &egraph, const std::vector<Id> &children) {
    return analyze_llt(egraph, children);
}

static AnalysisData analyze_potrf_u(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "Potrf_U");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (data->shape.first != data->shape.second) {
            throw InvalidOperationError("Potrf_U operation on non-square matrix");
        }
        if (!data->flags.is_positive_definite) {
            throw InvalidOperationError("Potrf_U operation on non-positive-definite matrix");
        }
        if (!data->flags.is_symmetric) {
            throw InvalidOperationError("Potrf_U operation on non-symmetric matrix");
        }
        MatrixProperty U;
        U.shape = data->shape;
        U.flags.is_upper_triangular = true;
        U.flags.is_non_singular = true;
        auto props = std::vector{U};
        return tuple_property_data(props);
    }
    throw AnalysisError("Potrf_U expects a Matrix input");
}

static AnalysisData analyze_geqrf(const EGraph &egraph, const std::vector<Id> &children) {
    return analyze_qr(egraph, children);
}

static AnalysisData analyze_trtri(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "Trtri");
    if (auto data = get_matrix_data(egraph, children.at(0))) {
        if (data->shape.first != data->shape.second) {
            throw InvalidOperationError("Trtri operation on non-square matrix");
        }
        if (!data->flags.is_upper_triangular && !data->flags.is_lower_triangular) {
            throw InvalidOperationError("Trtri operation on non-triangular matrix");
        }
        if (!data->flags.is_non_singular) {
            throw InvalidOperationError("Trtri operation on singular matrix");
        }

        MatrixProperty prop;
        prop.shape = data->shape;
        prop.flags.is_upper_triangular = data->flags.is_upper_triangular;
        prop.flags.is_lower_triangular = data->flags.is_lower_triangular;
        prop.flags.is_non_singular = true;
        prop.flags.is_full_rank = true;
        return matrix_property_data(prop);
    }
    throw AnalysisError("Trtri expects a Matrix input");
}

static AnalysisData analyze_gemv_n(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 3, "Gemv_N");
    if (auto data1 = get_matrix_data(egraph, children.at(0))) {
        if (auto data2 = get_matrix_data(egraph, children.at(1))) {
            if (auto data3 = get_matrix_data(egraph, children.at(2))) {
                if (!data2->is_vector() || !data3->is_vector()) {
                    throw InvalidOperationError("Gemv_N operation requires vector inputs for x and y");
                }

                if (data1->shape.second != data2->shape.first) {
                    throw ShapeMismatchError("Gemv_N operation with incompatible sizes between A and x");
                }

                if (data1->shape.first != data3->shape.first) {
                    throw ShapeMismatchError("Gemv_N operation with incompatible sizes between A and y");
                }

                MatrixProperty prop;
                prop.shape = std::make_pair(data1->shape.first, 1);
                return matrix_property_data(prop);
            }
        }
    }
    throw AnalysisError("Gemv_N expects Matrix inputs");
}

static AnalysisData analyze_gemv_t(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 3, "Gemv_T");

    if (auto dataA = get_matrix_data(egraph, children.at(0))) {
        if (auto dataX = get_matrix_data(egraph, children.at(1))) {
            if (auto dataY = get_matrix_data(egraph, children.at(2))) {

                if (!dataX->is_vector() || !dataY->is_vector()) {
                    throw InvalidOperationError("Gemv_T operation requires vector inputs for x and y");
                }

                if (dataA->shape.first != dataX->shape.first) {
                    throw ShapeMismatchError("Gemv_T operation with incompatible sizes between A and x");
                }

                if (dataA->shape.second != dataY->shape.first) {
                    throw ShapeMismatchError("Gemv_T operation with incompatible sizes between A and y");
                }

                MatrixProperty prop;
                prop.shape = std::make_pair(dataA->shape.second, 1); // Result is n x 1
                return matrix_property_data(prop);
            }
        }
    }
    throw AnalysisError("Gemv_T expects Matrix inputs");
}

static AnalysisData analyze_orgqr(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 1, "Orgqr");
    auto data = egraph.get_class_analysis_data(children.at(0));
    if (auto *props = std::get_if<TupleProperty>(&data.property)) {
        if (!props->empty()) {
            return AnalysisData{(*props)[0]};
        }
    }
    throw AnalysisError("Orgqr expects Geqrf output");
}

static AnalysisData analyze_ormqr_ln(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Ormqr_LN");
    auto data = egraph.get_class_analysis_data(children.at(0));
    if (auto *props = std::get_if<TupleProperty>(&data.property)) {
        if (auto C = get_matrix_data(egraph, children.at(1))) {
            MatrixProperty prop;
            prop.shape = {(*props)[0].shape.first, C->shape.second};
            prop.flags.is_full_rank = C->flags.is_full_rank;
            prop.flags.is_non_singular = C->flags.is_non_singular;
            prop.flags.is_orthogonal = (*props)[0].flags.is_orthogonal && C->flags.is_orthogonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Ormqr_LN expects Geqrf output and a matrix");
}

static AnalysisData analyze_ormqr_lt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Ormqr_LT");
    auto data = egraph.get_class_analysis_data(children.at(0));
    if (auto *props = std::get_if<TupleProperty>(&data.property)) {
        if (auto C = get_matrix_data(egraph, children.at(1))) {
            MatrixProperty prop;
            prop.shape = {(*props)[0].shape.second, C->shape.second};
            prop.flags.is_full_rank = C->flags.is_full_rank;
            prop.flags.is_non_singular = C->flags.is_non_singular;
            prop.flags.is_orthogonal = (*props)[0].flags.is_orthogonal && C->flags.is_orthogonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Ormqr_LT expects Geqrf output and a matrix");
}

static AnalysisData analyze_ormqr_rn(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Ormqr_RN");
    auto data = egraph.get_class_analysis_data(children.at(0));
    if (auto *props = std::get_if<TupleProperty>(&data.property)) {
        if (auto C = get_matrix_data(egraph, children.at(1))) {
            MatrixProperty prop;
            prop.shape = {C->shape.first, (*props)[0].shape.second};
            prop.flags.is_full_rank = C->flags.is_full_rank;
            prop.flags.is_non_singular = C->flags.is_non_singular;
            prop.flags.is_orthogonal = (*props)[0].flags.is_orthogonal && C->flags.is_orthogonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Ormqr_RN expects Geqrf output and a matrix");
}

static AnalysisData analyze_ormqr_rt(const EGraph &egraph, const std::vector<Id> &children) {
    check_arity(children, 2, "Ormqr_RT");
    auto data = egraph.get_class_analysis_data(children.at(0));
    if (auto *props = std::get_if<TupleProperty>(&data.property)) {
        if (auto C = get_matrix_data(egraph, children.at(1))) {
            MatrixProperty prop;
            prop.shape = {C->shape.first, (*props)[0].shape.first};
            prop.flags.is_full_rank = C->flags.is_full_rank;
            prop.flags.is_non_singular = C->flags.is_non_singular;
            prop.flags.is_orthogonal = (*props)[0].flags.is_orthogonal && C->flags.is_orthogonal;
            return matrix_property_data(prop);
        }
    }
    throw AnalysisError("Ormqr_RT expects Geqrf output and a matrix");
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
    case Minus:
        return analyze_minus(egraph, children);
    case QR:
        return analyze_qr(egraph, children);
    case LU:
        return analyze_lu(egraph, children);
    case LLt:
        return analyze_llt(egraph, children);
    case UtU:
        return analyze_utu(egraph, children);
    case Get:
        return analyze_get(egraph, children);
    case Sol:
        return analyze_solve(egraph, children);
    case SolR:
        return analyze_solve_right(egraph, children);
    case Det:
    case Log:
        return AnalysisData{};
    case Scale:
        return matrix_property_data(*get_matrix_data(egraph, children.at(0)));

    case Gemm_NN: {
        return analyze_gemm_nn(egraph, children);
    }
    case Gemm_TN: {
        return analyze_gemm_tn(egraph, children);
    }
    case Gemm_NT: {
        return analyze_gemm_nt(egraph, children);
    }
    case Gemm_TT: {
        return analyze_gemm_tt(egraph, children);
    }
    case Syrk_N: {
        return analyze_syrk_n(egraph, children);
    }
    case Syrk_T: {
        return analyze_syrk_t(egraph, children);
    }
    case Trsm_LN: {
        return analyze_trsm_ln(egraph, children);
    }
    case Trsm_LT: {
        return analyze_trsm_lt(egraph, children);
    }
    case Trsm_RN: {
        return analyze_trsm_rn(egraph, children);
    }
    case Trsm_RT: {
        return analyze_trsm_rt(egraph, children);
    }
    case Potrf_L: {
        return analyze_potrf_l(egraph, children);
    }
    case Potrf_U: {
        return analyze_potrf_u(egraph, children);
    }
    case Geqrf: {
        return analyze_geqrf(egraph, children);
    }
    case Trtri: {
        return analyze_trtri(egraph, children);
    }
    case Gemv_N: {
        return analyze_gemv_n(egraph, children);
    }
    case Gemv_T: {
        return analyze_gemv_t(egraph, children);
    }
    case Orgqr:
        return analyze_orgqr(egraph, children);
    case Ormqr_LN:
        return analyze_ormqr_ln(egraph, children);
    case Ormqr_LT:
        return analyze_ormqr_lt(egraph, children);
    case Ormqr_RN:
        return analyze_ormqr_rn(egraph, children);
    case Ormqr_RT:
        return analyze_ormqr_rt(egraph, children);
    default:
        throw AnalysisError(std::string("Unknown operation in analysis: ") + std::string(magic_enum::enum_name(op)));
    }
}