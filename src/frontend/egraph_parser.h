#pragma once

#include "manifest.h"
#include <stdexcept>
#include <string>
#include <string_view>

namespace egraph::frontend {

class SyntaxError : public std::runtime_error {
public:
    SyntaxError(std::string_view message, SourceLocation loc)
        : std::runtime_error(std::string(message) + " at line " + std::to_string(loc.line) + ", column " + std::to_string(loc.column)),
          loc_(loc) {}

    const SourceLocation &location() const noexcept { return loc_; }

private:
    SourceLocation loc_;
};

class CompileError : public std::runtime_error {
public:
    explicit CompileError(std::string_view message, SourceLocation loc = {})
        : std::runtime_error(std::string(message) + (loc.line > 0 ? (" at line " + std::to_string(loc.line) + ", column " + std::to_string(loc.column)) : "")),
          loc_(loc) {}

    const SourceLocation &location() const noexcept { return loc_; }

private:
    SourceLocation loc_;
};

ProgramManifest parse_program(std::string_view source);

} // namespace egraph::frontend
