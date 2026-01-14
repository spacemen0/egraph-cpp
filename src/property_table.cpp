#include "property_table.h"

bool PropertyTable::add_property_entry(const std::string &name, MatrixProperty property)
{
    auto result = properties.try_emplace(name, std::move(property));
    return result.second;
}

std::optional<MatrixProperty> PropertyTable::get_property(const std::string &name) const
{
    if (auto it = properties.find(name); it != properties.end())
    {
        return it->second;
    }
    return std::nullopt;
}

bool PropertyTable::has_property(const std::string &name) const
{
    return properties.contains(name);
}