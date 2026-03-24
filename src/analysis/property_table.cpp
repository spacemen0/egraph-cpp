#include "property_table.h"

#include "errors.h"
#include "utils.h"
#include <charconv>

MatrixProperty MatrixProperty::from_string(std::string_view text) {
    auto parse_size = [](std::string_view token) -> Size {
        int v = 0;
        auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), v);
        if (ec == std::errc() && ptr == token.data() + token.size()) {
            return v;
        }
        if (token.empty()) {
            throw ParseError("MatrixProperty::from_string: empty size token");
        }
        return std::string(token);
    };

    std::string_view view = trim(text);
    constexpr std::string_view prefix = "Matrix(";
    if (!view.starts_with(prefix)) {
        throw ParseError("MatrixProperty::from_string: expected 'Matrix('");
    }

    size_t close_paren = view.find(')');
    if (close_paren == std::string_view::npos) {
        throw ParseError("MatrixProperty::from_string: missing ')' in shape");
    }

    std::string_view dims = trim(view.substr(prefix.size(), close_paren - prefix.size()));
    size_t sep = dims.find('x');
    if (sep == std::string_view::npos) {
        throw ParseError("MatrixProperty::from_string: expected shape '<rows>x<cols>'");
    }

    std::string_view rows_token = trim(dims.substr(0, sep));
    std::string_view cols_token = trim(dims.substr(sep + 1));
    if (rows_token.empty() || cols_token.empty()) {
        throw ParseError("MatrixProperty::from_string: expected non-empty row/col sizes");
    }

    MatrixProperty prop;
    prop.shape = {parse_size(rows_token), parse_size(cols_token)};

    std::string_view rest = trim(view.substr(close_paren + 1));
    while (!rest.empty()) {
        if (!rest.starts_with("[")) {
            throw ParseError("MatrixProperty::from_string: expected '[' before flag");
        }

        size_t end = rest.find(']');
        if (end == std::string_view::npos) {
            throw ParseError("MatrixProperty::from_string: missing closing ']' for flag");
        }

        std::string_view label = trim(rest.substr(1, end - 1));
        bool matched = false;
        for (const auto &flag : flag_descriptors) {
            if (label == flag.label) {
                prop.flags.*(flag.member) = true;
                matched = true;
                break;
            }
        }

        if (!matched) {
            throw ParseError("MatrixProperty::from_string: unknown flag '" + std::string(label) + "'");
        }

        rest = trim(rest.substr(end + 1));
    }

    return prop;
}

PropertyTable::PropertyTable(std::vector<std::string> propertiy_strings) {
    for (const auto &string : propertiy_strings) {
        auto name_end = string.find(':');
        if (name_end == std::string::npos) {
            throw ParseError("PropertyTable::PropertyTable: expected ':' separating name and property");
        }
        auto name = std::string(trim(string.substr(0, name_end)));
        MatrixProperty property = MatrixProperty::from_string(trim(string.substr(name_end + 1)));
        properties.insert_or_assign(name, property);
    }
}

std::optional<MatrixProperty> PropertyTable::get_property(const std::string &name) const {
    if (auto it = properties.find(name); it != properties.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool PropertyTable::has_property(const std::string &name) const { return properties.contains(name); }

bool PropertyTable::add_or_update_property_entry(const std::string &name, MatrixProperty property) {
    auto result = properties.insert_or_assign(name, std::move(property));
    return result.second;
}