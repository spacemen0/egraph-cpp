#include "analysis.h"
#include "e_graph.h"
#include "errors.h"
#include "op_costs.h"
#include "op_registry.h"
#include "property_table.h"
#include "utils.h"
#include <cmath>
#include <variant>

namespace egraph {
AnalysisData MatrixAnalysis::make(const EGraph &egraph, const ENode &node) {
    const auto atom = node.get_atom();

    if (const auto op = std::get_if<Op>(&atom)) {
        return analyze_matrix_op(egraph, node, *op);
    }

    else if (const auto *a = std::get_if<uint32_t>(&atom)) {
        const auto s = get_string_from_lookup(*a);
        if (auto prop_opt = egraph.get_property_table().get_property(s); prop_opt.has_value()) {
            auto property = prop_opt.value();
            enforce_hierarchy(property);
            return AnalysisData{property};
        }
        throw AnalysisError("Variable has no property: " + s);
    } else {
        return {};
    }
}

// return true if the analysis data has changed after merging
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

// enforce the hierarchy of matrix properties, e.g., if a matrix is orthogonal, it must also be non-singular and
// full-rank
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

        if (p.is_square()) {
            p.flags.is_symmetric = true;
            p.flags.is_upper_triangular = true;
            p.flags.is_lower_triangular = true;
        }
    }

    if (p.is_square() && p.flags.is_full_rank) {
        p.flags.is_non_singular = true;
    }

    if (p.flags.is_non_singular) {
        p.flags.is_full_rank = true;
    }
}

// check if arity matches the operation
static void check_arity(const std::vector<Operand> &operands, size_t expected, const char *op_name) {
    if (operands.size() != expected) {
        throw AnalysisError(
            std::string(op_name) + " expects " + std::to_string(expected) + " children, got " +
            std::to_string(operands.size()));
    }
}

static AnalysisData make_matrix_property_data(MatrixProperty prop) {
    MatrixAnalysis::enforce_hierarchy(prop);
    return AnalysisData{prop};
}

static AnalysisData make_tuple_property_data(std::vector<MatrixProperty> &props) {
    for (auto &prop : props) {
        MatrixAnalysis::enforce_hierarchy(prop);
    }
    return AnalysisData{TupleProperty(props)};
}

static AnalysisData analyze_add(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "+");
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    if (mat0 && mat1) {
        const auto &data1 = mat0->property;
        const auto &data2 = mat1->property;
        if (data1.shape != data2.shape)
            throw ShapeMismatchError("+ size mismatch");

        MatrixProperty prop;
        prop.shape = data1.shape;
        prop.flags.is_symmetric = data1.flags.is_symmetric && data2.flags.is_symmetric;
        prop.flags.is_upper_triangular = data1.flags.is_upper_triangular && data2.flags.is_upper_triangular;
        prop.flags.is_lower_triangular = data1.flags.is_lower_triangular && data2.flags.is_lower_triangular;
        prop.flags.is_diagonal = data1.flags.is_diagonal && data2.flags.is_diagonal;
        prop.flags.is_zero = data1.flags.is_zero && data2.flags.is_zero;
        prop.flags.is_positive_definite = (data1.flags.is_positive_definite && data2.flags.is_positive_definite) ||
                                          (data1.flags.is_positive_definite && data2.flags.is_symmetric) ||
                                          (data2.flags.is_positive_definite && data1.flags.is_symmetric);
        prop.flags.is_full_rank = data1.flags.is_full_rank && data2.flags.is_full_rank;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("+ expects Matrix inputs");
}

static AnalysisData analyze_mul(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "*");
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    if (mat0 && mat1) {
        const auto &data1 = mat0->property;
        const auto &data2 = mat1->property;
        if (data1.shape.second != data2.shape.first) {
            throw ShapeMismatchError("* operation with incompatible sizes");
        }

        MatrixProperty prop;
        prop.shape = std::make_pair(data1.shape.first, data2.shape.second);
        prop.flags.is_identity = data1.flags.is_identity && data2.flags.is_identity;
        prop.flags.is_permutation = data1.flags.is_permutation && data2.flags.is_permutation;
        prop.flags.is_non_singular = data1.flags.is_non_singular && data2.flags.is_non_singular;
        prop.flags.is_full_rank = data1.flags.is_full_rank && data2.flags.is_full_rank;
        prop.flags.is_zero = data1.flags.is_zero || data2.flags.is_zero;
        prop.flags.is_lower_triangular = data1.flags.is_lower_triangular && data2.flags.is_lower_triangular;
        prop.flags.is_upper_triangular = data1.flags.is_upper_triangular && data2.flags.is_upper_triangular;
        prop.flags.is_orthogonal = data1.flags.is_orthogonal && data2.flags.is_orthogonal;
        prop.flags.is_diagonal = data1.flags.is_diagonal && data2.flags.is_diagonal;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("* expects Matrix inputs");
}

static AnalysisData analyze_transpose(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Tr");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        auto child_size = data.shape;
        MatrixProperty prop = data;

        prop.shape = std::make_pair(child_size.second, child_size.first);
        prop.flags.is_lower_triangular = data.flags.is_upper_triangular;
        prop.flags.is_upper_triangular = data.flags.is_lower_triangular;
        prop.flags.is_tall = data.flags.is_wide;
        prop.flags.is_wide = data.flags.is_tall;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Tr expects a Matrix input");
}

static AnalysisData analyze_invert(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Inv");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        if (!data.is_square()) {
            throw InvalidOperationError("Inv operation on non-square matrix");
        }
        if (!data.flags.is_non_singular) {
            throw InvalidOperationError("Inv operation on singular matrix");
        }

        MatrixProperty prop = data;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Inv expects a Matrix input");
}

static AnalysisData analyze_minus(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "-");
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    if (mat0 && mat1) {
        const auto &data1 = mat0->property;
        const auto &data2 = mat1->property;
        if (data1.shape != data2.shape)
            throw ShapeMismatchError("- size mismatch");

        MatrixProperty prop;
        prop.shape = data1.shape;
        prop.flags.is_symmetric = data1.flags.is_symmetric && data2.flags.is_symmetric;
        prop.flags.is_upper_triangular = data1.flags.is_upper_triangular && data2.flags.is_upper_triangular;
        prop.flags.is_lower_triangular = data1.flags.is_lower_triangular && data2.flags.is_lower_triangular;
        prop.flags.is_diagonal = data1.flags.is_diagonal && data2.flags.is_diagonal;
        prop.flags.is_zero = data1.flags.is_zero && data2.flags.is_zero;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("- expects Matrix inputs");
}

// Doing thin QR
static AnalysisData analyze_geqrf(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Geqrf");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        MatrixProperty Q;
        MatrixProperty R;

        Q.flags.has_orthonormal_columns = true;
        R.flags.is_upper_triangular = true;

        if (data.is_square()) {
            Q.shape = data.shape;
            R.shape = data.shape;
            Q.flags.is_orthogonal = true;
            if (data.flags.is_non_singular) {
                R.flags.is_non_singular = true;
            }
        } else if (data.is_tall_matrix() && data.flags.is_full_rank) {
            Q.shape = data.shape;
            Q.flags.is_tall = true;
            R.shape = std::make_pair(data.shape.second, data.shape.second);
            R.flags.is_non_singular = true;
        } else if (data.is_wide_matrix()) {
            Q.shape = std::make_pair(data.shape.first, data.shape.first);
            Q.flags.is_orthogonal = true;
            R.shape = data.shape;
            R.flags.is_wide = true;
            R.flags.is_upper_triangular = false; // is_upper_trapezoidal
            if (data.flags.is_full_rank) {
                R.flags.is_full_rank = true;
            }
        } else {
            throw InvalidOperationError("Geqrf operation on symbolic matrix with ambiguous shape");
        }

        if (data.flags.is_identity) {
            Q.shape = data.shape;
            R.shape = data.shape;
            Q.flags.is_orthogonal = true;
            Q.flags.is_identity = true;
            R.flags.is_upper_triangular = true;
            R.flags.is_identity = true;
            R.flags.is_diagonal = true;
            auto props = std::vector{Q, R};
            return make_tuple_property_data(props);
        }

        if (data.flags.is_zero) {
            throw InvalidOperationError("Geqrf operation on zero matrix is meaningless");
        }

        auto props = std::vector{Q, R};
        return make_tuple_property_data(props);
    }
    throw AnalysisError("Geqrf expects a Matrix input");
}

static AnalysisData analyze_qr(const std::vector<Operand> &operands) { return analyze_geqrf(operands); }

static AnalysisData analyze_lu(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "LU");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        if (data.shape.first != data.shape.second) {
            throw InvalidOperationError("LU operation on non-square matrix");
        }

        MatrixProperty L;
        MatrixProperty U;
        MatrixProperty P;

        L.shape = data.shape;
        L.flags.is_lower_triangular = true;
        L.flags.is_non_singular = true;

        U.shape = data.shape;
        U.flags.is_upper_triangular = true;
        if (data.flags.is_non_singular) {
            U.flags.is_non_singular = true;
        }

        P.shape = data.shape;
        P.flags.is_permutation = true;
        auto props = std::vector{L, U, P};

        return make_tuple_property_data(props);
    }
    throw AnalysisError("LU expects a Matrix input");
}

static AnalysisData analyze_get(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Get");
    const auto *tuple = as_tuple(operands[0]);
    const auto *idx = as_index(operands[1]);
    if (tuple && idx) {
        if (*idx >= 0 && static_cast<size_t>(*idx) < tuple->size()) {
            return AnalysisData{(*tuple)[*idx].property};
        }
        throw AnalysisError("Get index out of bounds");
    }
    if (!idx) {
        throw AnalysisError("Get index must be an integer");
    }
    throw AnalysisError("Get called on non-tuple");
}

static AnalysisData analyze_potrf_l(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Potrf_L");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        if (data.shape.first != data.shape.second) {
            throw InvalidOperationError("Potrf_L operation on non-square matrix");
        }
        if (!data.flags.is_positive_definite) {
            throw InvalidOperationError("Potrf_L operation on non-positive-definite matrix");
        }
        if (!data.flags.is_symmetric) {
            throw InvalidOperationError("Potrf_L operation on non-symmetric matrix");
        }
        MatrixProperty L;
        L.shape = data.shape;
        L.flags.is_lower_triangular = true;
        L.flags.is_non_singular = true;
        auto props = std::vector{L};
        return make_tuple_property_data(props);
    }
    throw AnalysisError("Potrf_L expects a Matrix input");
}

static AnalysisData analyze_cholel(const std::vector<Operand> &operands) { return analyze_potrf_l(operands); }

static AnalysisData analyze_gemm_nn(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Gemm_NN");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    const auto *matC = as_matrix(operands[2]);
    if (matA && matB && matC) {
        const auto &data = matA->property;
        const auto &data2 = matB->property;
        const auto &data3 = matC->property;
        if (data.shape.second != data2.shape.first || data2.shape.second != data3.shape.second ||
            data.shape.first != data3.shape.first) {
            throw ShapeMismatchError("Gemm_NN operation with incompatible sizes");
        }

        MatrixProperty prop;
        prop.shape = std::make_pair(data.shape.first, data3.shape.second);
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Gemm_NN expects Matrix inputs");
}

static AnalysisData analyze_gemm_tn(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Gemm_TN");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    const auto *matC = as_matrix(operands[2]);
    if (matA && matB && matC) {
        const auto &data = matA->property;
        const auto &data2 = matB->property;
        const auto &data3 = matC->property;
        if (data.shape.first != data2.shape.first || data2.shape.second != data3.shape.second ||
            data.shape.second != data3.shape.first) {
            throw ShapeMismatchError("Gemm_TN operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(data.shape.second, data2.shape.second);
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Gemm_TN expects Matrix inputs");
}

static AnalysisData analyze_gemm_nt(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Gemm_NT");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    const auto *matC = as_matrix(operands[2]);
    if (matA && matB && matC) {
        const auto &data = matA->property;
        const auto &data2 = matB->property;
        const auto &data3 = matC->property;
        if (data.shape.second != data2.shape.second || data2.shape.first != data3.shape.second ||
            data.shape.first != data3.shape.first) {
            throw ShapeMismatchError("Gemm_NT operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(data.shape.first, data2.shape.first);
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Gemm_NT expects Matrix inputs");
}

static AnalysisData analyze_gemm_tt(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Gemm_TT");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    const auto *matC = as_matrix(operands[2]);
    if (matA && matB && matC) {
        const auto &data = matA->property;
        const auto &data2 = matB->property;
        const auto &data3 = matC->property;
        if (data.shape.first != data2.shape.second || data2.shape.first != data3.shape.second ||
            data.shape.second != data3.shape.first) {
            throw ShapeMismatchError("Gemm_TT operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(data.shape.second, data2.shape.first);
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Gemm_TT expects Matrix inputs");
}

static AnalysisData analyze_syrk_n(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Syrk_N");
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    if (mat0 && mat1) {
        const auto &data = mat0->property;
        const auto &data2 = mat1->property;
        if (data.shape.first != data2.shape.first || data.shape.first != data2.shape.second) {
            throw ShapeMismatchError("Syrk_N operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(data.shape.first, data.shape.first);
        prop.flags.is_symmetric = true;
        bool full_row_rank = (data.is_square() || data.is_wide_matrix()) && data.flags.is_full_rank;
        if (full_row_rank || data.flags.is_positive_definite) {
            prop.flags.is_positive_definite = true;
            prop.flags.is_full_rank = true;
            prop.flags.is_non_singular = true;
        }
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Syrk_N expects Matrix inputs");
}

static AnalysisData analyze_syrk_t(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Syrk_T");
    const auto *mat0 = as_matrix(operands[0]);
    const auto *mat1 = as_matrix(operands[1]);
    if (mat0 && mat1) {
        const auto &data = mat0->property;
        const auto &data2 = mat1->property;
        if (data.shape.second != data2.shape.first || data.shape.second != data2.shape.second) {
            throw ShapeMismatchError("Syrk_T operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(data.shape.second, data.shape.second);
        prop.flags.is_symmetric = true;
        bool full_col_rank = (data.is_square() || data.is_tall_matrix()) && data.flags.is_full_rank;
        if (full_col_rank || data.flags.is_positive_definite) {
            prop.flags.is_positive_definite = true;
            prop.flags.is_full_rank = true;
            prop.flags.is_non_singular = true;
        }
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Syrk_T expects Matrix inputs");
}

static AnalysisData analyze_trmm_ln(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trmm_LN");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.is_square())
            throw InvalidOperationError("Trmm_LN expects square A");
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trmm_LN operation on non-triangular matrix A");
        }
        if (dataA.shape.second != dataB.shape.first) {
            throw ShapeMismatchError("Trmm_LN operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataA.shape.first, dataB.shape.second);
        prop.flags.is_full_rank = dataB.flags.is_full_rank && dataA.flags.is_full_rank;
        prop.flags.is_non_singular = prop.flags.is_full_rank && prop.is_square();
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trmm_LN expects Matrix inputs");
}

static AnalysisData analyze_trmm_lt(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trmm_LT");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.is_square())
            throw InvalidOperationError("Trmm_LT expects square A");
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trmm_LT operation on non-triangular matrix A");
        }
        if (dataA.shape.first != dataB.shape.first) {
            throw ShapeMismatchError("Trmm_LT operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataA.shape.second, dataB.shape.second);
        prop.flags.is_full_rank = dataB.flags.is_full_rank && dataA.flags.is_full_rank;
        prop.flags.is_non_singular = prop.flags.is_full_rank && prop.is_square();
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trmm_LT expects Matrix inputs");
}

static AnalysisData analyze_trmm_rn(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trmm_RN");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.is_square())
            throw InvalidOperationError("Trmm_RN expects square A");
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trmm_RN operation on non-triangular matrix A");
        }
        if (dataB.shape.second != dataA.shape.first) {
            throw ShapeMismatchError("Trmm_RN operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataB.shape.first, dataA.shape.second);
        prop.flags.is_full_rank = dataB.flags.is_full_rank && dataA.flags.is_full_rank;
        prop.flags.is_non_singular = prop.flags.is_full_rank && prop.is_square();
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trmm_RN expects Matrix inputs");
}

static AnalysisData analyze_trmm_rt(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trmm_RT");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.is_square())
            throw InvalidOperationError("Trmm_RT expects square A");
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trmm_RT operation on non-triangular matrix A");
        }
        if (dataB.shape.second != dataA.shape.second) {
            throw ShapeMismatchError("Trmm_RT operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataB.shape.first, dataA.shape.first);
        prop.flags.is_full_rank = dataB.flags.is_full_rank && dataA.flags.is_full_rank;
        prop.flags.is_non_singular = prop.flags.is_full_rank && prop.is_square();
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trmm_RT expects Matrix inputs");
}

static AnalysisData analyze_symm_l(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Symm_L");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    const auto *matC = as_matrix(operands[2]);
    if (matA && matB && matC) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        const auto &dataC = matC->property;
        if (!dataA.flags.is_symmetric)
            throw InvalidOperationError("Symm_L expects A to be symmetric");
        if (dataA.shape.second != dataB.shape.first || dataA.shape.first != dataC.shape.first ||
            dataB.shape.second != dataC.shape.second) {
            throw ShapeMismatchError("Symm_L sizes mismatch");
        }
        MatrixProperty prop;
        prop.shape = dataC.shape;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Symm_L inputs missing");
}

static AnalysisData analyze_symm_r(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Symm_R");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    const auto *matC = as_matrix(operands[2]);
    if (matA && matB && matC) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        const auto &dataC = matC->property;
        if (!dataA.flags.is_symmetric)
            throw InvalidOperationError("Symm_R expects A to be symmetric");
        if (dataB.shape.second != dataA.shape.first || dataB.shape.first != dataC.shape.first ||
            dataA.shape.second != dataC.shape.second) {
            throw ShapeMismatchError("Symm_R sizes mismatch");
        }
        MatrixProperty prop;
        prop.shape = dataC.shape;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Symm_R inputs missing");
}

static AnalysisData analyze_trsm_ln(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trsm_LN");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trsm_LN operation on non-triangular matrix A");
        }
        if (dataA.shape.second != dataB.shape.first || !dataA.flags.is_non_singular) {
            throw ShapeMismatchError("Trsm_LN operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataA.shape.first, dataB.shape.second);
        prop.flags.is_full_rank = dataB.flags.is_full_rank;
        prop.flags.is_non_singular = dataB.flags.is_non_singular;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trsm_LN expects Matrix inputs");
}

static AnalysisData analyze_trsm_lt(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trsm_LT");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trsm_LT operation on non-triangular matrix A");
        }
        if (dataA.shape.first != dataB.shape.first || !dataA.flags.is_non_singular) {
            throw ShapeMismatchError("Trsm_LT operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataA.shape.second, dataB.shape.second);
        prop.flags.is_full_rank = dataB.flags.is_full_rank;
        prop.flags.is_non_singular = dataB.flags.is_non_singular;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trsm_LT expects Matrix inputs");
}

static AnalysisData analyze_trsm_rn(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trsm_RN");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trsm_RN operation on non-triangular matrix A");
        }
        if (dataA.shape.first != dataB.shape.second || !dataA.flags.is_non_singular) {
            throw ShapeMismatchError("Trsm_RN operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataB.shape.first, dataA.shape.second);
        prop.flags.is_full_rank = dataB.flags.is_full_rank;
        prop.flags.is_non_singular = dataB.flags.is_non_singular;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trsm_RN expects Matrix inputs");
}

static AnalysisData analyze_trsm_rt(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Trsm_RT");
    const auto *matA = as_matrix(operands[0]);
    const auto *matB = as_matrix(operands[1]);
    if (matA && matB) {
        const auto &dataA = matA->property;
        const auto &dataB = matB->property;
        if (!dataA.flags.is_lower_triangular && !dataA.flags.is_upper_triangular) {
            throw InvalidOperationError("Trsm_RT operation on non-triangular matrix A");
        }
        if (dataA.shape.second != dataB.shape.second || !dataA.flags.is_non_singular) {
            throw ShapeMismatchError("Trsm_RT operation with incompatible sizes");
        }
        MatrixProperty prop;
        prop.shape = std::make_pair(dataB.shape.first, dataA.shape.first);
        prop.flags.is_full_rank = dataB.flags.is_full_rank;
        prop.flags.is_non_singular = dataB.flags.is_non_singular;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trsm_RT expects Matrix inputs");
}

static AnalysisData analyze_potrf_u(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Potrf_U");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        if (data.shape.first != data.shape.second) {
            throw InvalidOperationError("Potrf_U operation on non-square matrix");
        }
        if (!data.flags.is_positive_definite) {
            throw InvalidOperationError("Potrf_U operation on non-positive-definite matrix");
        }
        if (!data.flags.is_symmetric) {
            throw InvalidOperationError("Potrf_U operation on non-symmetric matrix");
        }
        MatrixProperty U;
        U.shape = data.shape;
        U.flags.is_upper_triangular = true;
        U.flags.is_non_singular = true;
        auto props = std::vector{U};
        return make_tuple_property_data(props);
    }
    throw AnalysisError("Potrf_U expects a Matrix input");
}

static AnalysisData analyze_choleu(const std::vector<Operand> &operands) { return analyze_potrf_u(operands); }

static AnalysisData analyze_trtri(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Trtri");
    if (const auto *mat0 = as_matrix(operands[0])) {
        const auto &data = mat0->property;
        if (data.shape.first != data.shape.second) {
            throw InvalidOperationError("Trtri operation on non-square matrix");
        }
        if (!data.flags.is_upper_triangular && !data.flags.is_lower_triangular) {
            throw InvalidOperationError("Trtri operation on non-triangular matrix");
        }
        if (!data.flags.is_non_singular) {
            throw InvalidOperationError("Trtri operation on singular matrix");
        }

        MatrixProperty prop;
        prop.shape = data.shape;
        prop.flags.is_upper_triangular = data.flags.is_upper_triangular;
        prop.flags.is_lower_triangular = data.flags.is_lower_triangular;
        prop.flags.is_non_singular = true;
        prop.flags.is_full_rank = true;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Trtri expects a Matrix input");
}

static AnalysisData analyze_gemv_n(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Gemv_N");
    const auto *matA = as_matrix(operands[0]);
    const auto *matX = as_matrix(operands[1]);
    const auto *matY = as_matrix(operands[2]);
    if (matA && matX && matY) {
        const auto &data1 = matA->property;
        const auto &data2 = matX->property;
        const auto &data3 = matY->property;
        if (!data2.is_vector() || !data3.is_vector()) {
            throw InvalidOperationError("Gemv_N operation requires vector inputs for x and y");
        }

        if (data1.shape.second != data2.shape.first) {
            throw ShapeMismatchError("Gemv_N operation with incompatible sizes between A and x");
        }

        if (data1.shape.first != data3.shape.first) {
            throw ShapeMismatchError("Gemv_N operation with incompatible sizes between A and y");
        }

        MatrixProperty prop;
        prop.shape = std::make_pair(data1.shape.first, 1);
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Gemv_N expects Matrix inputs");
}

static AnalysisData analyze_gemv_t(const std::vector<Operand> &operands) {
    check_arity(operands, 3, "Gemv_T");
    const auto *matA = as_matrix(operands[0]);
    const auto *matX = as_matrix(operands[1]);
    const auto *matY = as_matrix(operands[2]);
    if (matA && matX && matY) {
        const auto &dataA = matA->property;
        const auto &dataX = matX->property;
        const auto &dataY = matY->property;

        if (!dataX.is_vector() || !dataY.is_vector()) {
            throw InvalidOperationError("Gemv_T operation requires vector inputs for x and y");
        }

        if (dataA.shape.first != dataX.shape.first) {
            throw ShapeMismatchError("Gemv_T operation with incompatible sizes between A and x");
        }

        if (dataA.shape.second != dataY.shape.first) {
            throw ShapeMismatchError("Gemv_T operation with incompatible sizes between A and y");
        }

        MatrixProperty prop;
        prop.shape = std::make_pair(dataA.shape.second, 1); // Result is n x 1
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Gemv_T expects Matrix inputs");
}

static AnalysisData analyze_orgqr(const std::vector<Operand> &operands) {
    check_arity(operands, 1, "Orgqr");
    const auto *tuple = as_tuple(operands[0]);
    if (tuple && !tuple->empty()) {
        return AnalysisData{(*tuple)[0].property};
    }
    throw AnalysisError("Orgqr expects Geqrf output");
}

static AnalysisData analyze_ormqr_ln(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Ormqr_LN");
    const auto *tuple = as_tuple(operands[0]);
    const auto *matC = as_matrix(operands[1]);
    if (tuple && !tuple->empty() && matC) {
        const auto &Q = (*tuple)[0].property;
        const auto &C = matC->property;
        MatrixProperty prop;
        prop.shape = {Q.shape.first, C.shape.second};
        prop.flags.is_full_rank = C.flags.is_full_rank;
        prop.flags.is_non_singular = C.flags.is_non_singular;
        prop.flags.is_orthogonal = Q.flags.is_orthogonal && C.flags.is_orthogonal;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Ormqr_LN expects Geqrf output and a matrix");
}

static AnalysisData analyze_ormqr_lt(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Ormqr_LT");
    const auto *tuple = as_tuple(operands[0]);
    const auto *matC = as_matrix(operands[1]);
    if (tuple && !tuple->empty() && matC) {
        const auto &Q = (*tuple)[0].property;
        const auto &C = matC->property;
        MatrixProperty prop;
        prop.shape = {Q.shape.second, C.shape.second};
        prop.flags.is_full_rank = C.flags.is_full_rank;
        prop.flags.is_non_singular = C.flags.is_non_singular;
        prop.flags.is_orthogonal = Q.flags.is_orthogonal && C.flags.is_orthogonal;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Ormqr_LT expects Geqrf output and a matrix");
}

static AnalysisData analyze_ormqr_rn(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Ormqr_RN");
    const auto *tuple = as_tuple(operands[0]);
    const auto *matC = as_matrix(operands[1]);
    if (tuple && !tuple->empty() && matC) {
        const auto &Q = (*tuple)[0].property;
        const auto &C = matC->property;
        MatrixProperty prop;
        prop.shape = {C.shape.first, Q.shape.second};
        prop.flags.is_full_rank = C.flags.is_full_rank;
        prop.flags.is_non_singular = C.flags.is_non_singular;
        prop.flags.is_orthogonal = Q.flags.is_orthogonal && C.flags.is_orthogonal;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Ormqr_RN expects Geqrf output and a matrix");
}

static AnalysisData analyze_ormqr_rt(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Ormqr_RT");
    const auto *tuple = as_tuple(operands[0]);
    const auto *matC = as_matrix(operands[1]);
    if (tuple && !tuple->empty() && matC) {
        const auto &Q = (*tuple)[0].property;
        const auto &C = matC->property;
        MatrixProperty prop;
        prop.shape = {C.shape.first, Q.shape.first};
        prop.flags.is_full_rank = C.flags.is_full_rank;
        prop.flags.is_non_singular = C.flags.is_non_singular;
        prop.flags.is_orthogonal = Q.flags.is_orthogonal && C.flags.is_orthogonal;
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Ormqr_RT expects Geqrf output and a matrix");
}

static AnalysisData analyze_axpy(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Axpy");
    return analyze_add(operands);
}

static AnalysisData analyze_scale(const std::vector<Operand> &operands) {
    check_arity(operands, 2, "Scale");
    const auto *mat = as_matrix(operands[0]);
    const auto *scalar = as_scalar(operands[1]);
    if (mat && scalar) {
        auto prop = mat->property;
        double s = *scalar;
        if (std::isnan(s)) {
            prop.flags.is_identity = false;
            prop.flags.is_orthogonal = false;
            prop.flags.has_orthonormal_columns = false;
        } else if (s == 0.0) {
            prop.flags.is_zero = true;
            prop.flags.is_identity = false;
            prop.flags.is_orthogonal = false;
            prop.flags.has_orthonormal_columns = false;
            prop.flags.is_diagonal = true;

            if (prop.is_square()) {
                prop.flags.is_symmetric = true;
                prop.flags.is_lower_triangular = true;
                prop.flags.is_upper_triangular = true;
            }
        } else {
            if (s != 1.0) {
                prop.flags.is_identity = false;
            }
            if (std::abs(s) != 1.0) {
                prop.flags.is_orthogonal = false;
                prop.flags.has_orthonormal_columns = false;
            }
        }
        return make_matrix_property_data(prop);
    }
    throw AnalysisError("Scale expects Matrix and Scalar inputs");
}

void init_builtin_ops(OpRegistry &registry) {
    registry.register_op("Add", false, analyze_add, compute_add_cost);
    registry.register_op("Mul", false, analyze_mul, compute_mul_cost);
    registry.register_op("Minus", false, analyze_minus, compute_minus_cost);
    registry.register_op("Tr", true, analyze_transpose, compute_tr_cost);
    registry.register_op("Inv", false, analyze_invert, compute_inv_cost);
    registry.register_op("Get", false, analyze_get, compute_get_cost);
    registry.register_op("Scale", true, analyze_scale, compute_scale_cost);

    registry.register_op("QR", false, analyze_qr, compute_qr_cost);
    registry.register_op("LU", false, analyze_lu, compute_lu_cost);
    registry.register_op("CholeL", false, analyze_cholel, compute_cholel_cost);
    registry.register_op("CholeU", false, analyze_choleu, compute_choleu_cost);

    registry.register_op("Geqrf", true, analyze_geqrf, compute_geqrf_cost);
    registry.register_op("Trtri", true, analyze_trtri, compute_trtri_cost);
    registry.register_op("Gemv_N", true, analyze_gemv_n, compute_gemv_n_cost);
    registry.register_op("Gemv_T", true, analyze_gemv_t, compute_gemv_t_cost);
    registry.register_op("Gemm_NN", true, analyze_gemm_nn, compute_gemm_nn_group_cost);
    registry.register_op("Gemm_TN", true, analyze_gemm_tn, compute_gemm_nn_group_cost);
    registry.register_op("Gemm_NT", true, analyze_gemm_nt, compute_gemm_nn_group_cost);
    registry.register_op("Gemm_TT", true, analyze_gemm_tt, compute_gemm_nn_group_cost);
    registry.register_op("Syrk_N", true, analyze_syrk_n, compute_syrk_n_syrk_t_cost);
    registry.register_op("Syrk_T", true, analyze_syrk_t, compute_syrk_n_syrk_t_cost);
    registry.register_op("Trmm_LN", true, analyze_trmm_ln, compute_trmm_ln_group_cost);
    registry.register_op("Trmm_LT", true, analyze_trmm_lt, compute_trmm_ln_group_cost);
    registry.register_op("Trmm_RN", true, analyze_trmm_rn, compute_trmm_ln_group_cost);
    registry.register_op("Trmm_RT", true, analyze_trmm_rt, compute_trmm_ln_group_cost);
    registry.register_op("Symm_L", true, analyze_symm_l, compute_symm_group_cost);
    registry.register_op("Symm_R", true, analyze_symm_r, compute_symm_group_cost);
    registry.register_op("Trsm_LN", true, analyze_trsm_ln, compute_trsm_ln_group_cost);
    registry.register_op("Trsm_LT", true, analyze_trsm_lt, compute_trsm_ln_group_cost);
    registry.register_op("Trsm_RN", true, analyze_trsm_rn, compute_trsm_ln_group_cost);
    registry.register_op("Trsm_RT", true, analyze_trsm_rt, compute_trsm_ln_group_cost);
    registry.register_op("Potrf_L", true, analyze_potrf_l, compute_potrf_l_potrf_u_cost);
    registry.register_op("Potrf_U", true, analyze_potrf_u, compute_potrf_l_potrf_u_cost);
    registry.register_op("Orgqr", true, analyze_orgqr, compute_orgqr_cost);
    registry.register_op("Ormqr_LN", true, analyze_ormqr_ln, compute_ormqr_cost);
    registry.register_op("Ormqr_LT", true, analyze_ormqr_lt, compute_ormqr_cost);
    registry.register_op("Ormqr_RN", true, analyze_ormqr_rn, compute_ormqr_cost);
    registry.register_op("Ormqr_RT", true, analyze_ormqr_rt, compute_ormqr_cost);
    registry.register_op("Axpy", true, analyze_axpy, compute_axpy_cost);
}

AnalysisData MatrixAnalysis::analyze_matrix_op(const EGraph &egraph, const ENode &node, Op op) {
    const auto &desc = OpRegistry::instance().get(op);
    if (!desc.analyze) {
        throw AnalysisError("Operation has no analysis registered: " + desc.name);
    }

    const auto &children = node.get_children();
    std::vector<Operand> operands;
    init_operands(egraph, children, operands);

    return desc.analyze(operands);
}
} // namespace egraph
