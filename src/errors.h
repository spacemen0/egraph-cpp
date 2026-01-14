#pragma once

#include <stdexcept>
#include <string>

class ShapeMismatchError : public std::runtime_error
{
public:
    explicit ShapeMismatchError(const std::string &msg) : std::runtime_error("Shape mismatch: " + msg) {}
};

class InvalidOperationError : public std::runtime_error
{
public:
    explicit InvalidOperationError(const std::string &msg) : std::runtime_error("Invalid operation: " + msg) {}
};

class AnalysisError : public std::runtime_error
{
public:
    explicit AnalysisError(const std::string &msg) : std::runtime_error("Analysis failed: " + msg) {}
};

class ParseError : public std::runtime_error
{
public:
    explicit ParseError(const std::string &msg) : std::runtime_error("Parse error: " + msg) {}
};

class ExtractionError : public std::runtime_error
{
public:
    explicit ExtractionError(const std::string &msg) : std::runtime_error("Extraction failed: " + msg) {}
};
