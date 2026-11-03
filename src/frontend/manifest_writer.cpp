#include "manifest_writer.h"
#include "utils.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>

namespace egraph::frontend {
namespace {

std::string_view name(SymbolKind kind) {
    constexpr std::array names{"Matrix", "Scalar", "ColVector", "RowVector"};
    return names.at(static_cast<size_t>(kind));
}

std::string_view name(EffortLevel effort) {
    constexpr std::array names{"low", "medium", "high"};
    return names.at(static_cast<size_t>(effort));
}

std::string_view name(OperationKind kind) {
    constexpr std::array names{"extract_best", "extract_all", "evaluate"};
    return names.at(static_cast<size_t>(kind));
}

std::string_view name(ScalarOp op) {
    constexpr std::array names{"Value", "Var", "Add", "Sub", "Mul", "Div", "Neg"};
    return names.at(static_cast<size_t>(op));
}

void quoted(std::ostream &out, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 0x20) {
                out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    out << '"';
}

template <typename Range, typename Writer> void array(std::ostream &out, const Range &values, Writer write) {
    out << '[';
    bool first = true;
    for (const auto &value : values) {
        if (!first)
            out << ", ";
        first = false;
        write(value);
    }
    out << ']';
}

void size_json(std::ostream &out, const Size &size) {
    if (const auto *n = std::get_if<int>(&size))
        out << *n;
    else
        quoted(out, std::get<std::string>(size));
}

void location_json(std::ostream &out, SourceLocation loc) {
    out << "{\"line\": " << loc.line << ", \"column\": " << loc.column << '}';
}

void scalar_json(std::ostream &out, const ScalarExpr &scalar) {
    if (!std::isfinite(scalar.val)) {
        throw std::invalid_argument("Cannot serialize a non-finite scalar value as JSON");
    }
    out << "{\"op\": ";
    quoted(out, name(scalar.op));
    out << ", \"value\": ";
    if (scalar.val == 0.0 && std::signbit(scalar.val)) {
        out << "-0.0";
    } else {
        out << scalar.val;
    }
    out << ", \"var\": ";
    quoted(out, scalar.var);
    out << ", \"children\": ";
    array(out, scalar.children, [&](const auto &child) {
        scalar_json(out, child);
    });
    out << '}';
}

void expression_json(std::ostream &out, const Expression &expr) {
    out << "{\"atom\": {\"kind\": ";
    if (const auto *op = std::get_if<Op>(&expr.atom)) {
        out << "\"op\", \"name\": ";
        quoted(out, OpRegistry::name(*op));
    } else if (const auto *symbol = std::get_if<uint32_t>(&expr.atom)) {
        out << "\"symbol\", \"name\": ";
        quoted(out, get_string_from_lookup(*symbol));
    } else if (const auto *index = std::get_if<int>(&expr.atom)) {
        out << "\"index\", \"value\": " << *index;
    } else {
        out << "\"scalar\", \"expression\": ";
        scalar_json(out, std::get<ScalarExpr>(expr.atom));
    }
    out << "}, \"children\": ";
    array(out, expr.children, [&](const auto &child) {
        expression_json(out, child);
    });
    out << '}';
}

void manifest_json(std::ostream &out, const ProgramManifest &manifest) {
    out << "{\n  \"version\": 1,\n  \"symbols\": ";
    array(out, manifest.symbols, [&](const SymbolDecl &symbol) {
        out << "{\"name\": ";
        quoted(out, symbol.name);
        out << ", \"kind\": ";
        quoted(out, name(symbol.kind));
        out << ", \"property_flags\": ";
        array(out, symbol.property_flags, [&](const auto &flag) {
            quoted(out, flag);
        });
        out << ", \"shape\": [";
        size_json(out, symbol.shape.first);
        out << ", ";
        size_json(out, symbol.shape.second);
        out << "], \"loc\": ";
        location_json(out, symbol.loc);
        out << '}';
    });
    out << ",\n  \"expressions\": ";
    array(out, manifest.expressions, [&](const NamedExpr &expr) {
        out << "{\"name\": ";
        quoted(out, expr.name);
        out << ", \"expr\": ";
        expression_json(out, expr.expr);
        out << ", \"loc\": ";
        location_json(out, expr.loc);
        out << '}';
    });
    out << ",\n  \"operations\": ";
    array(out, manifest.operations, [&](const OperationDirective &op) {
        out << "{\"kind\": ";
        quoted(out, name(op.kind));
        out << ", \"target_variable\": ";
        quoted(out, op.target_variable);
        out << ", \"effort\": ";
        quoted(out, name(op.effort));
        out << ", \"data_bindings\": {";
        bool first = true;
        for (const auto &[key, path] :
             std::map<std::string, std::string>(op.data_bindings.begin(), op.data_bindings.end())) {
            if (!first)
                out << ", ";
            first = false;
            quoted(out, key);
            out << ": ";
            quoted(out, path);
        }
        out << "}, \"loc\": ";
        location_json(out, op.loc);
        out << '}';
    });
    out << "\n}\n";
}

void scalar_text(std::ostream &out, const ScalarExpr &scalar, size_t depth) {
    out << std::string(depth * 2, ' ') << "Scalar " << name(scalar.op);
    if (scalar.op == ScalarOp::Value)
        out << ' ' << scalar.val;
    else if (scalar.op == ScalarOp::Var) {
        out << ' ';
        quoted(out, scalar.var);
    }
    out << '\n';
    for (const auto &child : scalar.children)
        scalar_text(out, child, depth + 1);
}

void expression_text(std::ostream &out, const Expression &expr, size_t depth) {
    if (const auto *scalar = std::get_if<ScalarExpr>(&expr.atom)) {
        scalar_text(out, *scalar, depth);
    } else {
        out << std::string(depth * 2, ' ');
        if (const auto *op = std::get_if<Op>(&expr.atom))
            out << "Op " << OpRegistry::name(*op);
        else if (const auto *symbol = std::get_if<uint32_t>(&expr.atom)) {
            out << "Symbol ";
            quoted(out, get_string_from_lookup(*symbol));
        } else
            out << "Index " << std::get<int>(expr.atom);
        out << '\n';
    }
    for (const auto &child : expr.children)
        expression_text(out, child, depth + 1);
}

void manifest_text(std::ostream &out, const ProgramManifest &manifest) {
    out << "Program manifest\nSymbols:\n";
    for (const auto &symbol : manifest.symbols) {
        out << "  " << symbol.name << ": " << name(symbol.kind) << " [rows=";
        size_json(out, symbol.shape.first);
        out << ", cols=";
        size_json(out, symbol.shape.second);
        out << "] ";
        array(out, symbol.property_flags, [&](const auto &flag) {
            quoted(out, flag);
        });
        out << " @" << symbol.loc.line << ':' << symbol.loc.column << '\n';
    }
    out << "Expressions:\n";
    for (const auto &expr : manifest.expressions) {
        out << "  " << expr.name << " @" << expr.loc.line << ':' << expr.loc.column << '\n';
        expression_text(out, expr.expr, 2);
    }
    out << "Operations:\n";
    for (const auto &op : manifest.operations) {
        out << "  " << name(op.kind) << ' ' << op.target_variable << " [effort=" << name(op.effort) << "] @"
            << op.loc.line << ':' << op.loc.column << '\n';
        for (const auto &[key, path] :
             std::map<std::string, std::string>(op.data_bindings.begin(), op.data_bindings.end())) {
            out << "    ";
            quoted(out, key);
            out << " = ";
            quoted(out, path);
            out << '\n';
        }
    }
}

} // namespace

void write_manifest(const ProgramManifest &manifest, const std::filesystem::path &path, ManifestFormat format) {
    // Serialize first so invalid data does not truncate an existing file.
    std::ostringstream contents;
    contents.imbue(std::locale::classic());
    contents << std::setprecision(std::numeric_limits<double>::max_digits10);
    switch (format) {
    case ManifestFormat::Json:
        manifest_json(contents, manifest);
        break;
    case ManifestFormat::Text:
        manifest_text(contents, manifest);
        break;
    default:
        throw std::invalid_argument("Unknown manifest format");
    }
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(path, std::ios::binary | std::ios::trunc);
    output << contents.str();
    output.close();
}

} // namespace egraph::frontend
