#include "errors.h"
#include "types.h"
#include <gtest/gtest.h>

TEST(MatrixPropertyFromString, ParsesNumericShapeAndFlags) {
    MatrixProperty p = MatrixProperty::from_string("Matrix(3x4) [identity] [zero] [diagonal]");

    EXPECT_EQ(std::get<int>(p.shape.first), 3);
    EXPECT_EQ(std::get<int>(p.shape.second), 4);
    EXPECT_TRUE(p.flags.is_identity);
    EXPECT_TRUE(p.flags.is_zero);
    EXPECT_TRUE(p.flags.is_diagonal);
    EXPECT_FALSE(p.flags.is_symmetric);
}

TEST(MatrixPropertyFromString, ParsesSymbolicShapeAndWhitespace) {
    MatrixProperty p = MatrixProperty::from_string("  Matrix(  m  x  n )   [upper_triangular]   [wide]   ");

    EXPECT_EQ(std::get<std::string>(p.shape.first), "m");
    EXPECT_EQ(std::get<std::string>(p.shape.second), "n");
    EXPECT_TRUE(p.flags.is_upper_triangular);
    EXPECT_TRUE(p.flags.is_wide);
    EXPECT_FALSE(p.flags.is_lower_triangular);
}

TEST(MatrixPropertyFromString, RoundTripsToString) {
    MatrixProperty original;
    original.shape = {2, std::string("k")};
    original.flags.is_orthogonal = true;
    original.flags.is_tall = true;
    original.flags.is_positive_definite = true;

    MatrixProperty parsed = MatrixProperty::from_string(original.to_string());
    EXPECT_TRUE(parsed.strict_equal(original));
}

TEST(MatrixPropertyFromString, ThrowsOnUnknownFlag) {
    EXPECT_THROW(MatrixProperty::from_string("Matrix(2x2) [not_a_flag]"), ParseError);
}

TEST(MatrixPropertyFromString, ThrowsOnMalformedShape) {
    EXPECT_THROW(MatrixProperty::from_string("Matrix(2,2) [identity]"), ParseError);
    EXPECT_THROW(MatrixProperty::from_string("Matrix(2x) [identity]"), ParseError);
    EXPECT_THROW(MatrixProperty::from_string("2x2 [identity]"), ParseError);
}
