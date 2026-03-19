#pragma once
#include "types.h"
#include <optional>
#include <string>
#include <unordered_map>

class PropertyTable {
  public:
    bool add_or_update_property_entry(const std::string &name, MatrixProperty property);

    std::optional<MatrixProperty> get_property(const std::string &name) const;

    bool has_property(const std::string &name) const;

  private:
    std::unordered_map<std::string, MatrixProperty> properties;
};
