#include "analysis.h"
#include "analysis_ops.h"
#include "e_graph.h"
#include "errors.h"
#include <variant>

AnalysisData MatrixAnalysis::make(const EGraph &egraph, const ENode &node)
{
    const auto atom = node.get_atom();

    if (const auto op = std::get_if<Op>(&atom))
    {
        return analyze_matrix_op(egraph, node, *op);
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
    else
    {
        return AnalysisData{"Const"};
    }
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
        p1.is_orthonormal = p1.is_orthonormal || p2.is_orthonormal;
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
        if (!p2)
        {
            throw AnalysisError("Cannot merge MatrixProperty with non-MatrixProperty (likely TupleProperty)");
        }
        merge_matrix_props(*p1, *p2);
    }
    else if (auto *t1 = std::get_if<TupleProperty>(&data1.property))
    {
        const auto *t2 = std::get_if<TupleProperty>(&data2.property);
        if (!t2)
        {
            throw AnalysisError("Cannot merge TupleProperty with non-TupleProperty");
        }
        for (size_t i = 0; i < t1->size(); ++i)
        {
            merge_matrix_props((*t1)[i], (*t2)[i]);
        }
    }
}
