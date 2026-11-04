#include "frontend/egraph_parser.h"
#include "frontend/manifest_writer.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>

using namespace egraph;
using namespace egraph::frontend;

namespace {

class ManifestWriterTest : public ::testing::Test {
  protected:
    std::filesystem::path directory;

    void SetUp() override {
        directory = std::filesystem::path() /
                    ("egraph_manifest_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(directory / "manifest.json", ec);
        std::filesystem::remove(directory / "manifest.txt", ec);
        std::filesystem::remove(directory, ec);
    }

    std::string read(const char *filename) {
        std::ifstream input(directory / filename, std::ios::binary);
        EXPECT_TRUE(input.is_open());
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
};

TEST_F(ManifestWriterTest, WritesTypedJsonWithMetadata) {
    auto manifest = parse_program(R"(symbols { A: Matrix { symmetric }; alpha: Scalar; }
expressions { e=(alpha+2.0)*A; }
operations { evaluate(e, { A="a.csv", alpha="alpha.csv" }, high); })");
    manifest.symbols[0].shape = {"n", 4};
    write_manifest(manifest, directory / "manifest.json");
    const auto json = read("manifest.json");
    EXPECT_NE(json.find("\"version\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"kind\": \"Matrix\""), std::string::npos);
    EXPECT_NE(json.find("\"property_flags\": [\"symmetric\"]"), std::string::npos);
    EXPECT_NE(json.find("\"shape\": [\"n\", 4]"), std::string::npos);
    EXPECT_NE(json.find("\"line\": 2"), std::string::npos);
    EXPECT_NE(json.find("\"name\": \"Scale\""), std::string::npos);
    EXPECT_NE(json.find("\"kind\": \"scalar\""), std::string::npos);
    EXPECT_NE(json.find("\"op\": \"Add\""), std::string::npos);
    EXPECT_NE(json.find("\"var\": \"alpha\""), std::string::npos);
    EXPECT_NE(json.find("\"target_variable\": \"e\""), std::string::npos);
    EXPECT_NE(json.find("\"effort\": \"high\""), std::string::npos);
}

TEST_F(ManifestWriterTest, EscapesStringsAndPreservesNumericPrecision) {
    ProgramManifest manifest;
    manifest.expressions.push_back({"e", Expression(0.12345678901234566), {1, 1}});
    OperationDesc op;
    op.target_variable = "e";
    op.data_bindings["A"] = "C:\\data\\\"a\"\n\t\r\b\f" + std::string(1, '\x01');
    manifest.operations.push_back(op);
    write_manifest(manifest, directory / "manifest.json");
    const auto json = read("manifest.json");
    EXPECT_NE(json.find("0.12345678901234566"), std::string::npos);
    EXPECT_NE(json.find("C:\\\\data\\\\\\\"a\\\"\\n\\t\\r\\b\\f\\u0001"), std::string::npos);
}

TEST_F(ManifestWriterTest, BindingOrderIsDeterministic) {
    ProgramManifest first;
    first.operations.emplace_back();
    first.operations[0].data_bindings.emplace("Z", "z.csv");
    first.operations[0].data_bindings.emplace("A", "a.csv");
    auto second = first;
    second.operations[0].data_bindings.clear();
    second.operations[0].data_bindings.emplace("A", "a.csv");
    second.operations[0].data_bindings.emplace("Z", "z.csv");
    write_manifest(first, directory / "manifest.json");
    const auto expected = read("manifest.json");
    write_manifest(second, directory / "manifest.json");
    EXPECT_EQ(read("manifest.json"), expected);
    EXPECT_LT(expected.find("\"A\": \"a.csv\""), expected.find("\"Z\": \"z.csv\""));
}

TEST_F(ManifestWriterTest, PreservesNegativeZeroAndIndexAtoms) {
    ProgramManifest manifest;
    manifest.expressions.push_back({"negative_zero", Expression(-0.0), {}});
    manifest.expressions.push_back({"index", Expression(7), {}});
    write_manifest(manifest, directory / "manifest.json");
    const auto json = read("manifest.json");
    EXPECT_NE(json.find("\"value\": -0.0"), std::string::npos);
    EXPECT_NE(json.find("\"kind\": \"index\", \"value\": 7"), std::string::npos);
}

TEST_F(ManifestWriterTest, ReportsFileOpenFailure) {
    EXPECT_THROW(write_manifest({}, directory / "missing" / "manifest.json"), std::ios_base::failure);
}

TEST_F(ManifestWriterTest, RejectsNonFiniteJsonBeforeReplacingExistingFile) {
    {
        std::ofstream existing(directory / "manifest.json");
        existing << "original";
    }
    ProgramManifest manifest;
    manifest.expressions.push_back({"e", Expression(std::numeric_limits<double>::infinity()), {}});
    EXPECT_THROW(write_manifest(manifest, directory / "manifest.json"), std::invalid_argument);
    EXPECT_EQ(read("manifest.json"), "original");
}

} // namespace
