#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <utility>

struct MatrixProperty
{
    std::pair<size_t, size_t> shape = {0, 0};
    bool is_symmetric = false;
    bool is_orthogonal = false;

    bool is_square() const { return shape.first == shape.second; }
};

class PropertyTable
{
public:
    bool add_property_entry(const std::string &name, MatrixProperty property);

    std::optional<MatrixProperty> get_property(const std::string &name) const;

    bool has_property(const std::string &name) const;

private:
    std::unordered_map<std::string, MatrixProperty> properties;
};
