#pragma once

#include "basic_types.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

struct MatrixProperty {
    Shape shape; // (rows, cols)
    struct MatrixFlags {
        bool is_symmetric = false;
        bool is_orthogonal = false;
        bool is_orthonormal = false;
        bool is_identity = false;
        bool is_zero = false;
        bool is_upper_triangular = false;
        bool is_lower_triangular = false;
        bool is_diagonal = false;
        bool is_positive_definite = false;
        bool is_singular = false;
        bool is_permutation = false;
        bool is_tall = false;
        bool is_wide = false;
    };
    MatrixFlags flags;

    struct FlagDescriptor {
        bool MatrixFlags::*member;
        const char *label;
    };

    static constexpr std::array<FlagDescriptor, 13> flag_descriptors = {{
        {&MatrixFlags::is_symmetric, "symmetric"},
        {&MatrixFlags::is_orthogonal, "orthogonal"},
        {&MatrixFlags::is_orthonormal, "orthonormal"},
        {&MatrixFlags::is_identity, "identity"},
        {&MatrixFlags::is_zero, "zero"},
        {&MatrixFlags::is_upper_triangular, "upper_triangular"},
        {&MatrixFlags::is_lower_triangular, "lower_triangular"},
        {&MatrixFlags::is_diagonal, "diagonal"},
        {&MatrixFlags::is_positive_definite, "positive_definite"},
        {&MatrixFlags::is_singular, "singular"},
        {&MatrixFlags::is_permutation, "permutation"},
        {&MatrixFlags::is_tall, "tall"},
        {&MatrixFlags::is_wide, "wide"},
    }};

    static MatrixProperty from_string(std::string_view text);

    bool is_square() const { return shape.first == shape.second; }
    bool has_symbolic_shape() const {
        return !std::holds_alternative<int>(shape.first) || !std::holds_alternative<int>(shape.second);
    }
    bool is_tall_matrix() const {
        if (auto rows = std::get_if<int>(&shape.first)) {
            if (auto cols = std::get_if<int>(&shape.second)) {
                return *rows > *cols;
            }
        }
        return flags.is_tall;
    }
    bool is_wide_matrix() const {
        if (auto rows = std::get_if<int>(&shape.first)) {
            if (auto cols = std::get_if<int>(&shape.second)) {
                return *rows < *cols;
            }
        }
        return flags.is_wide;
    }
    bool is_scalar() const {
        if (auto w = std::get_if<int>(&shape.first)) {
            if (auto h = std::get_if<int>(&shape.second)) {
                return *w == 1 && *h == 1;
            }
        }
        return false;
    }
    bool is_vector() const {
        if (auto w = std::get_if<int>(&shape.first)) {
            if (auto h = std::get_if<int>(&shape.second)) {
                return (*w == 1 && *h > 1) || (*h == 1 && *w > 1);
            } else {
                return *w == 1;
            }
        } else if (auto h = std::get_if<int>(&shape.second)) {
            return *h == 1;
        }
        return false;
    }

    bool operator==(const MatrixProperty &other) const { return shape == other.shape; };
    bool strict_equal(const MatrixProperty &other) const {
        if (shape != other.shape) {
            return false;
        }

        for (const auto &flag : flag_descriptors) {
            if (this->flags.*(flag.member) != other.flags.*(flag.member)) {
                return false;
            }
        }

        return true;
    }

    std::string to_string() const {
        std::string s = "Matrix(";
        if (auto w = std::get_if<int>(&shape.first))
            s += std::to_string(*w);
        else
            s += std::get<std::string>(shape.first);

        s += "x";

        if (auto h = std::get_if<int>(&shape.second))
            s += std::to_string(*h);
        else
            s += std::get<std::string>(shape.second);

        s += ")";

        for (const auto &flag : flag_descriptors) {
            if (this->flags.*(flag.member)) {
                s += " [";
                s += flag.label;
                s += "]";
            }
        }

        return s;
    }
};

using TupleProperty = std::vector<MatrixProperty>;

struct AnalysisData {
    std::variant<MatrixProperty, TupleProperty, std::string> property;
    bool operator==(const AnalysisData &other) const {
        if (property.index() != other.property.index())
            return false;
        if (const auto *p1 = std::get_if<MatrixProperty>(&property)) {
            const auto *p2 = std::get_if<MatrixProperty>(&other.property);
            return *p1 == *p2;
        } else {
            const auto *p3 = std::get_if<TupleProperty>(&property);
            const auto *p2 = std::get_if<TupleProperty>(&other.property);
            return *p3 == *p2;
        }
    }
};

class PropertyTable {
  public:
    bool add_or_update_property_entry(const std::string &name, MatrixProperty property);

    std::optional<MatrixProperty> get_property(const std::string &name) const;

    bool has_property(const std::string &name) const;

  private:
    std::unordered_map<std::string, MatrixProperty> properties;
};
