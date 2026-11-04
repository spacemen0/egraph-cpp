#include <gtest/gtest.h>
#include "frontend/egraph_parser.h"

using namespace egraph;
using namespace egraph::frontend;

namespace {

class EGraphParserTest : public ::testing::Test {
protected:
    bool parse_and_check_syntax(std::string_view dsl) {
        try {
            parse_program(dsl);
            return true;
        } catch (const SyntaxError &) {
            return false;
        } catch (const CompileError &) {
            // A CompileError means syntax parsed successfully, but failed semantic check
            return true;
        } catch (...) {
            return false;
        }
    }
};

} // namespace

TEST_F(EGraphParserTest, AcceptsRequiredEmptyBlocks) {
    auto manifest = parse_program("symbols {} expressions {} operations {}");
    EXPECT_TRUE(manifest.symbols.empty());
    EXPECT_TRUE(manifest.expressions.empty());
    EXPECT_TRUE(manifest.operations.empty());
}

TEST_F(EGraphParserTest, RejectsMissingProgramBlocks) {
    for (const auto *script :
         {"", "symbols {}", "expressions {}", "operations {}", "symbols {} expressions {}", "symbols {} operations {}",
          "expressions {} operations {}"}) {
        SCOPED_TRACE(script);
        EXPECT_THROW(parse_program(script), SyntaxError);
    }
}

TEST_F(EGraphParserTest, RejectsReorderedOrRepeatedProgramBlocks) {
    for (const auto *script :
         {"symbols {} operations {} expressions {}", "expressions {} symbols {} operations {}",
          "expressions {} operations {} symbols {}", "operations {} symbols {} expressions {}",
          "operations {} expressions {} symbols {}", "symbols {} symbols {} expressions {} operations {}",
          "symbols {} expressions {} expressions {} operations {}",
          "symbols {} expressions {} operations {} operations {}"}) {
        SCOPED_TRACE(script);
        EXPECT_THROW(parse_program(script), SyntaxError);
    }
}

TEST_F(EGraphParserTest, RegressionTestsSyntaxValidity) {
    // 1. x = 2.5; -> Valid
    EXPECT_TRUE(parse_and_check_syntax("symbols {} expressions { x = 2.5; } operations {}"));

    // 2. x = -2.5; -> Valid
    EXPECT_TRUE(parse_and_check_syntax("symbols {} expressions { x = -2.5; } operations {}"));

    // 3. x = 2; -> Invalid (double requires decimal dot and fractional digits)
    EXPECT_FALSE(parse_and_check_syntax("symbols {} expressions { x = 2; } operations {}"));

    // 4. x = 2.; -> Invalid (double requires trailing fractional digits)
    EXPECT_FALSE(parse_and_check_syntax("symbols {} expressions { x = 2.; } operations {}"));

    // 5. x = a-2.5; -> Valid
    EXPECT_TRUE(parse_and_check_syntax("symbols { a: Scalar; } expressions { x = a - 2.5; } operations {}"));

    // 6. x = a--2.5; -> Valid
    EXPECT_TRUE(parse_and_check_syntax("symbols { a: Scalar; } expressions { x = a - -2.5; } operations {}"));

    // 7. Matrix: Scalar; -> Invalid (Matrix is reserved keyword)
    EXPECT_FALSE(parse_and_check_syntax("symbols { Matrix : Scalar; } expressions {} operations {}"));

    // 8. Matrix2: Scalar; -> Valid
    EXPECT_TRUE(parse_and_check_syntax("symbols { Matrix2 : Scalar; } expressions {} operations {}"));

    // 9. trans: Matrix; -> Invalid (trans is reserved keyword)
    EXPECT_FALSE(parse_and_check_syntax("symbols { trans : Matrix(n, m); } expressions {} operations {}"));

    // 10. transpose: Matrix; -> Valid
    EXPECT_TRUE(parse_and_check_syntax("symbols { transpose : Matrix(n, m); } expressions {} operations {}"));

    // 11. x = 1.0e3; -> Invalid (scientific exponent notation not allowed)
    EXPECT_FALSE(parse_and_check_syntax("symbols {} expressions { x = 1.0e3; } operations {}"));

    // 12. evaluate(A,{file="a.csv"}); -> Valid
    EXPECT_TRUE(parse_and_check_syntax(
        "symbols { A : Matrix(10, 20); }\n"
        "expressions { e = A; }\n"
        "operations { evaluate(e, { A = \"a.csv\" }); }"));

    // 13. evaluate(A,{count=10}); -> Invalid (count=10 is not string literal)
    EXPECT_FALSE(parse_and_check_syntax(
        "symbols { A : Matrix(10, 20); }\n"
        "expressions { e = A; }\n"
        "operations { evaluate(e, { count = 10 }); }"));

    // 14. Valid property literals
    EXPECT_TRUE(parse_and_check_syntax(
        "symbols { A : Matrix(10, 20) { symmetric, positive_definite }; } expressions {} operations {}"));

    // 15. Invalid property literal
    EXPECT_FALSE(parse_and_check_syntax("symbols { A : Matrix(10, 20) { unknown_prop }; } expressions {} operations {}"));
}

TEST_F(EGraphParserTest, LowersCompleteValidProgram) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20) { symmetric };
            B : Matrix(20, 30);
            v : ColVector(30);
        }
        expressions {
            e1 = A * B;
            e2 = e1 + A;
        }
        operations {
            extract_best(e2, { A = "a.csv", B = "b.csv" }, effort = high);
        }
    )";

    ProgramManifest manifest = parse_program(script);

    EXPECT_EQ(manifest.symbols.size(), 3);
    EXPECT_EQ(manifest.symbols[0].name, "A");
    EXPECT_EQ(manifest.symbols[0].kind, SymbolKind::Matrix);
    EXPECT_EQ(manifest.symbols[0].shape, (Shape{10, 20}));
    ASSERT_EQ(manifest.symbols[0].property_flags.size(), 1);
    EXPECT_EQ(manifest.symbols[0].property_flags[0], "symmetric");

    EXPECT_EQ(manifest.symbols[1].name, "B");
    EXPECT_EQ(manifest.symbols[1].kind, SymbolKind::Matrix);
    EXPECT_EQ(manifest.symbols[1].shape, (Shape{20, 30}));

    EXPECT_EQ(manifest.symbols[2].name, "v");
    EXPECT_EQ(manifest.symbols[2].kind, SymbolKind::ColVector);
    EXPECT_EQ(manifest.symbols[2].shape, (Shape{30, 1}));

    EXPECT_EQ(manifest.expressions.size(), 2);
    EXPECT_EQ(manifest.expressions[0].name, "e1");
    EXPECT_EQ(manifest.expressions[1].name, "e2");

    // Inlined expression check: e2 should have expanded e1 into (A * B) + A
    Expression expected_e2 = (Expression("A") * Expression("B")) + Expression("A");
    EXPECT_EQ(manifest.expressions[1].expr, expected_e2);

    EXPECT_EQ(manifest.operations.size(), 1);
    const auto &op = manifest.operations[0];
    EXPECT_EQ(op.kind, OperationKind::ExtractBest);
    EXPECT_EQ(op.target_variable, "e2");
    EXPECT_EQ(op.effort, EffortLevel::High);
    EXPECT_EQ(op.data_bindings.at("A"), "a.csv");
    EXPECT_EQ(op.data_bindings.at("B"), "b.csv");
}

TEST_F(EGraphParserTest, LowersOperationsWithOptionalEffortAfterBindings) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
        }
        expressions {
            e = A;
        }
        operations {
            extract_best(e, { A = "a.csv" }, effort = low);
            evaluate(e, { A = "a.csv" }, effort = medium);
            extract_all(e, { A = "a.csv" });
        }
    )";

    ProgramManifest manifest = parse_program(script);
    ASSERT_EQ(manifest.operations.size(), 3);

    EXPECT_EQ(manifest.operations[0].kind, OperationKind::ExtractBest);
    EXPECT_EQ(manifest.operations[0].effort, EffortLevel::Low);
    EXPECT_EQ(manifest.operations[0].data_bindings.at("A"), "a.csv");

    EXPECT_EQ(manifest.operations[1].kind, OperationKind::Evaluate);
    EXPECT_EQ(manifest.operations[1].effort, EffortLevel::Medium);
    EXPECT_EQ(manifest.operations[1].data_bindings.at("A"), "a.csv");

    EXPECT_EQ(manifest.operations[2].kind, OperationKind::ExtractAll);
    EXPECT_EQ(manifest.operations[2].effort, EffortLevel::Medium); // default
    EXPECT_EQ(manifest.operations[2].data_bindings.at("A"), "a.csv");
}

TEST_F(EGraphParserTest, DesugarsScalarDivision) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
        }
        expressions {
            e = A / 2.0;
        }
        operations {
            extract_best(e, { A = "a.csv" });
        }
    )";

    ProgramManifest manifest = parse_program(script);
    ASSERT_EQ(manifest.expressions.size(), 1);
    EXPECT_EQ(manifest.expressions[0].expr, scale(Expression("A"), 0.5));
}

TEST_F(EGraphParserTest, RejectsMalformedOrRepeatedOperationArguments) {
    for (const auto *call : {
             "evaluate(e { A=\"a.csv\" } lowhigh);",
             "evaluate(e { A=\"a.csv\" });",
             "evaluate(e, { A=\"a.csv\" } effort=low);",
             "evaluate(e, effort=low { A=\"a.csv\" });",
             "evaluate(e, { A=\"a.csv\" }, lowhigh);",
             "evaluate(e, { A=\"a.csv\" }, low1);",
             "evaluate(e, effort=low, effort=high, { A=\"a.csv\" });",
             "evaluate(e, { A=\"a.csv\" }, low, high);",
             "evaluate(e, { A=\"a.csv\" }, { A=\"b.csv\" });",
             "evaluate(e, high, { A=\"a.csv\" }, { A=\"b.csv\" });",
         }) {
        SCOPED_TRACE(call);
        const std::string script =
            "symbols { A: Matrix(10, 20); } expressions { e=A; } operations { " + std::string(call) + " }";
        EXPECT_THROW(parse_program(script), SyntaxError);
    }
}

TEST_F(EGraphParserTest, RejectsEffortBeforeBindings) {
    for (const auto *call :
         {"evaluate(e, low, { A=\"a.csv\" });", "extract_best(e, effort=high, { A=\"a.csv\" });",
          "extract_all(e, medium, { A=\"a.csv\" });"}) {
        SCOPED_TRACE(call);
        const std::string script =
            "symbols { A: Matrix(10, 20); } expressions { e=A; } operations { " + std::string(call) + " }";
        EXPECT_THROW(parse_program(script), SyntaxError);
    }
}

TEST_F(EGraphParserTest, AcceptsBareEffortAfterBindings) {
    auto manifest = parse_program(R"(
        symbols { A: Matrix(10, 20); }
        expressions { e=A; }
        operations {
            evaluate(e, { A="a.csv" }, low);
            extract_best(e, { A="a.csv" }, high);
        }
    )");
    ASSERT_EQ(manifest.operations.size(), 2);
    EXPECT_EQ(manifest.operations[0].effort, EffortLevel::Low);
    EXPECT_EQ(manifest.operations[1].effort, EffortLevel::High);
    EXPECT_EQ(manifest.operations[0].data_bindings.at("A"), "a.csv");
    EXPECT_EQ(manifest.operations[1].data_bindings.at("A"), "a.csv");
}

TEST_F(EGraphParserTest, RejectsDuplicateBindingKeys) {
    EXPECT_THROW(
        parse_program(R"(
        symbols { A: Matrix(10, 20); }
        expressions { e=A; }
        operations { evaluate(e, { A="a.csv", A="b.csv" }); }
    )"),
        CompileError);
}

TEST_F(EGraphParserTest, RejectsUnknownExpressionReferences) {
    for (const auto *expression : {"typo", "A * typo", "typo + A", "e", "later"}) {
        SCOPED_TRACE(expression);
        const std::string script =
            "symbols { A: Matrix(10, 20); }\nexpressions {\n e=" + std::string(expression) + ";\n later=A;\n} operations {}";
        try {
            parse_program(script);
            FAIL() << "Expected unknown reference to be rejected";
        } catch (const CompileError &err) {
            EXPECT_NE(std::string(err.what()).find("Undefined identifier"), std::string::npos);
            EXPECT_EQ(err.location().line, 3);
        }
    }
}

TEST_F(EGraphParserTest, RejectsUnknownOperationTarget) {
    try {
        parse_program(R"(symbols { A: Matrix(10, 20); }
expressions { e=A; }
operations { evaluate(missing, { A="a.csv" }); })");
        FAIL() << "Expected unknown operation target to be rejected";
    } catch (const CompileError &err) {
        EXPECT_NE(std::string(err.what()).find("Undefined operation target"), std::string::npos);
        EXPECT_EQ(err.location().line, 3);
    }
}

TEST_F(EGraphParserTest, AcceptsDeclaredSymbolOperationTarget) {
    auto manifest = parse_program(R"(
        symbols { A: Matrix(10, 20); }
        expressions {}
        operations { evaluate(A, { A="a.csv" }); }
    )");
    ASSERT_EQ(manifest.operations.size(), 1);
    EXPECT_EQ(manifest.operations[0].target_variable, "A");
}

TEST_F(EGraphParserTest, RejectsMatrixDivision) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
            B : Matrix(20, 30);
        }
        expressions {
            e = A / B;
        }
        operations {
            extract_best(e, { A = "a.csv", B = "b.csv" });
        }
    )";

    EXPECT_THROW(
        {
            try {
                parse_program(script);
            } catch (const CompileError &err) {
                EXPECT_TRUE(std::string(err.what()).find("Direct matrix division is unsupported") != std::string::npos);
                throw;
            }
        },
        CompileError);
}

TEST_F(EGraphParserTest, RejectsDivisionByZero) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
        }
        expressions {
            e = A / 0.0;
        }
        operations {
            extract_best(e, { A = "a.csv" });
        }
    )";

    EXPECT_THROW(
        {
            try {
                parse_program(script);
            } catch (const CompileError &err) {
                EXPECT_TRUE(std::string(err.what()).find("Division by zero") != std::string::npos);
                throw;
            }
        },
        CompileError);
}

TEST_F(EGraphParserTest, RejectsZeroDenominatorsAfterScalarSubstitution) {
    for (const auto *expression : {"A / z", "1.0 / z", "alpha / z", "A / (two - two)", "A / (alpha * (1.0 / z))"}) {
        SCOPED_TRACE(expression);
        const std::string script = "symbols { A : Matrix(10, 20); alpha : Scalar; }\n"
                                   "expressions {\n"
                                   "  two = 2.0;\n"
                                   "  z = two - two;\n"
                                   "  e = " +
                                   std::string(expression) + ";\n} operations {}";
        try {
            parse_program(script);
            FAIL() << "Expected division by zero to be rejected";
        } catch (const CompileError &err) {
            EXPECT_NE(std::string(err.what()).find("Division by zero"), std::string::npos);
            EXPECT_EQ(err.location().line, 5);
        }
    }
}

TEST_F(EGraphParserTest, FoldsScalarAliasesInsideMatrixExpressions) {
    auto manifest = parse_program(R"(
        symbols { A : Matrix(10, 20); }
        expressions {
            s = 2.0;
            t = s + 2.0;
            e = A + trans(A / t);
        }
        operations {}
    )");
    EXPECT_EQ(manifest.expressions[1].expr, Expression(4.0));
    EXPECT_EQ(manifest.expressions[2].expr, Expression("A") + transpose(scale(Expression("A"), 0.25)));
}

TEST_F(EGraphParserTest, FoldsScalarArithmeticAfterSubstitution) {
    auto manifest = parse_program(R"(
        symbols {}
        expressions {
            s = 2.0;
            e = -(s + 4.0) * (s - 1.0) / s;
        }
        operations {}
    )");
    EXPECT_EQ(manifest.expressions[1].expr, Expression(-3.0));
}

TEST_F(EGraphParserTest, PreservesSymbolicScalarPayloadAfterFoldingConstants) {
    auto manifest = parse_program(R"(
        symbols { A : Matrix(10, 20); alpha : Scalar; }
        expressions {
            s = 2.0;
            e = A / (alpha + s * s);
        }
        operations {}
    )");
    const auto &expr = manifest.expressions[1].expr;
    ASSERT_EQ(expr.children.size(), 2);
    const auto &coefficient = expr.children[1];
    EXPECT_TRUE(coefficient.children.empty());
    ASSERT_TRUE(std::holds_alternative<ScalarExpr>(coefficient.atom));
    const auto &scalar = std::get<ScalarExpr>(coefficient.atom);
    EXPECT_EQ(scalar, ScalarExpr(1.0) / (ScalarExpr("alpha") + 4.0));
    EXPECT_DOUBLE_EQ(scalar.evaluate({{"alpha", {4.0}}}), 0.125);
}

TEST_F(EGraphParserTest, ArithmeticPrecedenceAndParentheses) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
            B : Matrix(20, 30);
            C : Matrix(30, 40);
        }
        expressions {
            e1 = A + B * C;
            e2 = (A + B) * C;
            e3 = trans(A) * inv(B);
            e4 = -A;
        }
        operations {
            extract_best(e1, { A = "a.csv", B = "b.csv", C = "c.csv" });
        }
    )";

    ProgramManifest manifest = parse_program(script);
    ASSERT_EQ(manifest.expressions.size(), 4);

    EXPECT_EQ(manifest.expressions[0].expr, Expression("A") + (Expression("B") * Expression("C")));
    EXPECT_EQ(manifest.expressions[1].expr, (Expression("A") + Expression("B")) * Expression("C"));
    EXPECT_EQ(manifest.expressions[2].expr, transpose(Expression("A")) * inverse(Expression("B")));
    EXPECT_EQ(manifest.expressions[3].expr, -Expression("A"));
}

TEST_F(EGraphParserTest, RejectsForwardReferenceBeforeCycleCanBeFormed) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
        }
        expressions {
            e1 = e2 + A;
            e2 = e1 * A;
        }
        operations {
            extract_best(e1, { A = "a.csv" });
        }
    )";

    EXPECT_THROW(
        {
            try {
                parse_program(script);
            } catch (const CompileError &err) {
                EXPECT_TRUE(std::string(err.what()).find("Undefined identifier 'e2'") != std::string::npos);
                throw;
            }
        },
        CompileError);
}

TEST_F(EGraphParserTest, MissingDataBindingsThrowsCompileError) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
        }
        expressions {
            e = A;
        }
        operations {
            extract_best(e, {}, effort = low);
        }
    )";

    EXPECT_THROW(
        {
            try {
                parse_program(script);
            } catch (const CompileError &err) {
                EXPECT_TRUE(std::string(err.what()).find("Missing required data_bindings") != std::string::npos);
                throw;
            }
        },
        CompileError);
}

TEST_F(EGraphParserTest, SyntaxErrorReportsLocation) {
    std::string script = R"(symbols {
    A : Matrix
}
expressions {
    e = A;
}
operations {
    extract_best(e, { A = "a.csv" });
}
)";

    EXPECT_THROW(
        {
            try {
                parse_program(script);
            } catch (const SyntaxError &err) {
                EXPECT_GE(err.location().line, 2);
                throw;
            }
        },
        SyntaxError);
}

TEST_F(EGraphParserTest, SupportsComments) {
    std::string script = R"(
        // C++ style line comment
        symbols {
            # Shell style line comment
            A : Matrix(10, 20); // trailing comment
        }
        expressions {
            e = A; # trailing shell comment
        }
        operations {
            extract_best(e, { A = "a.csv" });
        }
    )";

    ProgramManifest manifest = parse_program(script);
    EXPECT_EQ(manifest.symbols.size(), 1);
    EXPECT_EQ(manifest.symbols[0].name, "A");
    EXPECT_EQ(manifest.expressions.size(), 1);
    EXPECT_EQ(manifest.expressions[0].name, "e");
    EXPECT_EQ(manifest.operations.size(), 1);
}

TEST_F(EGraphParserTest, MixedScalarMatrixMultiplicationAndDivision) {
    std::string script = R"(
        symbols {
            A : Matrix(10, 20);
            B : Matrix(20, 30);
            alpha : Scalar;
        }
        expressions {
            e1 = 2.0 * A;
            e2 = A * 2.0;
            e3 = alpha * A;
            e4 = A * alpha;
            e5 = A / alpha;
            e6 = 2.0 * 3.0;
            e7 = 6.0 / 2.0;
            e8 = 2.0 * 3.0 * A;
            e9 = A / (2.0 * 3.0);
            e10 = (A / 2.0) * B;
        }
        operations {
            extract_best(e1, { A = "a.csv" });
        }
    )";

    ProgramManifest manifest = parse_program(script);
    ASSERT_EQ(manifest.expressions.size(), 10);

    // e1 = 2.0 * A -> Scale(A, 2.0)
    EXPECT_EQ(manifest.expressions[0].expr, scale(Expression("A"), 2.0));

    // e2 = A * 2.0 -> Scale(A, 2.0)
    EXPECT_EQ(manifest.expressions[1].expr, scale(Expression("A"), 2.0));

    // e3 = alpha * A -> Scale(A, alpha)
    EXPECT_EQ(manifest.expressions[2].expr, scale(Expression("A"), ScalarExpr("alpha")));

    // e4 = A * alpha -> Scale(A, alpha)
    EXPECT_EQ(manifest.expressions[3].expr, scale(Expression("A"), ScalarExpr("alpha")));

    // e5 = A / alpha -> Scale(A, 1.0 / alpha)
    EXPECT_EQ(manifest.expressions[4].expr, scale(Expression("A"), ScalarExpr(1.0) / ScalarExpr("alpha")));

    // e6 = 2.0 * 3.0 -> 6.0
    EXPECT_EQ(manifest.expressions[5].expr, Expression(6.0));

    // e7 = 6.0 / 2.0 -> 3.0
    EXPECT_EQ(manifest.expressions[6].expr, Expression(3.0));

    // e8 = 2.0 * 3.0 * A -> Scale(A, 6.0)
    EXPECT_EQ(manifest.expressions[7].expr, scale(Expression("A"), 6.0));

    // e9 = A / (2.0 * 3.0) -> Scale(A, 1.0 / 6.0)
    EXPECT_EQ(manifest.expressions[8].expr, scale(Expression("A"), 1.0 / 6.0));

    // e10 = (A / 2.0) * B -> Mul(Scale(A, 0.5), B)
    EXPECT_EQ(manifest.expressions[9].expr, scale(Expression("A"), 0.5) * Expression("B"));
}

TEST_F(EGraphParserTest, RejectsInvalidMixedScalarMatrixOperations) {
    // 1. Matrix + Scalar is invalid
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { A : Matrix(10, 20); }
                expressions { e = A + 2.0; }
                operations { extract_best(e, { A = "a.csv" }); }
            )");
        },
        CompileError);

    // 2. Scalar / Matrix is invalid
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { A : Matrix(10, 20); }
                expressions { e = 2.0 / A; }
                operations { extract_best(e, { A = "a.csv" }); }
            )");
        },
        CompileError);

    // 3. trans(scalar) is invalid
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { alpha : Scalar; }
                expressions { e = trans(alpha); }
                operations { extract_best(e, { alpha = "a.csv" }); }
            )");
        },
        CompileError);

    // 4. inv(scalar) is invalid
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { alpha : Scalar; }
                expressions { e = inv(alpha); }
                operations { extract_best(e, { alpha = "a.csv" }); }
            )");
        },
        CompileError);

    // 5. Undeclared or matrix variable in division denominator is rejected immediately
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { A : Matrix(10, 20); }
                expressions { e = A / undeclared_sym; }
                operations { extract_best(e, { A = "a.csv" }); }
            )");
        },
        CompileError);

    // 6. Adding scalar to matrix intermediate expression is rejected immediately
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { A : Matrix(10, 20); }
                expressions {
                    e1 = A;
                    e2 = 1.0 + e1;
                }
                operations { extract_best(e2, { A = "a.csv" }); }
            )");
        },
        CompileError);

    // 7. Duplicate symbol or expression definitions are rejected immediately
    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { A : Matrix(10, 20); A : Scalar; }
                expressions { e = 1.0; }
                operations { extract_best(e, {}); }
            )");
        },
        CompileError);

    EXPECT_THROW(
        {
            parse_program(R"(
                symbols { A : Matrix(10, 20); }
                expressions {
                    A = 1.0;
                }
                operations { extract_best(A, {}); }
            )");
        },
        CompileError);
}

TEST_F(EGraphParserTest, ParsesMatrixAndVectorDimensions) {
    std::string script = R"(
        symbols {
            A : Matrix(100, 20) { symmetric };
            B : Matrix(n, m);
            v : ColVector(100);
            w : ColVector(k);
            r : RowVector(200);
            u : RowVector(p);
            s : Scalar;
        }
        expressions {}
        operations {}
    )";

    ProgramManifest manifest = parse_program(script);
    ASSERT_EQ(manifest.symbols.size(), 7);

    EXPECT_EQ(manifest.symbols[0].name, "A");
    EXPECT_EQ(manifest.symbols[0].kind, SymbolKind::Matrix);
    EXPECT_EQ(manifest.symbols[0].shape, (Shape{100, 20}));
    ASSERT_EQ(manifest.symbols[0].property_flags.size(), 1);
    EXPECT_EQ(manifest.symbols[0].property_flags[0], "symmetric");

    EXPECT_EQ(manifest.symbols[1].name, "B");
    EXPECT_EQ(manifest.symbols[1].kind, SymbolKind::Matrix);
    EXPECT_EQ(manifest.symbols[1].shape, (Shape{"n", "m"}));

    EXPECT_EQ(manifest.symbols[2].name, "v");
    EXPECT_EQ(manifest.symbols[2].kind, SymbolKind::ColVector);
    EXPECT_EQ(manifest.symbols[2].shape, (Shape{100, 1}));

    EXPECT_EQ(manifest.symbols[3].name, "w");
    EXPECT_EQ(manifest.symbols[3].kind, SymbolKind::ColVector);
    EXPECT_EQ(manifest.symbols[3].shape, (Shape{"k", 1}));

    EXPECT_EQ(manifest.symbols[4].name, "r");
    EXPECT_EQ(manifest.symbols[4].kind, SymbolKind::RowVector);
    EXPECT_EQ(manifest.symbols[4].shape, (Shape{1, 200}));

    EXPECT_EQ(manifest.symbols[5].name, "u");
    EXPECT_EQ(manifest.symbols[5].kind, SymbolKind::RowVector);
    EXPECT_EQ(manifest.symbols[5].shape, (Shape{1, "p"}));

    EXPECT_EQ(manifest.symbols[6].name, "s");
    EXPECT_EQ(manifest.symbols[6].kind, SymbolKind::Scalar);
    EXPECT_EQ(manifest.symbols[6].shape, (Shape{1, 1}));
}

TEST_F(EGraphParserTest, RejectsInvalidDimensionSyntax) {
    // Missing dimensions
    EXPECT_THROW(parse_program("symbols { A : Matrix; } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { v : ColVector; } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { r : RowVector; } expressions {} operations {}"), SyntaxError);

    // Wrong number of dimensions
    EXPECT_THROW(parse_program("symbols { A : Matrix(10); } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { A : Matrix(10, 20, 30); } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { v : ColVector(10, 20); } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { r : RowVector(10, 20); } expressions {} operations {}"), SyntaxError);

    // Scalar must not have dimensions
    EXPECT_THROW(parse_program("symbols { s : Scalar(); } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { s : Scalar(1); } expressions {} operations {}"), SyntaxError);
    EXPECT_THROW(parse_program("symbols { s : Scalar(1, 1); } expressions {} operations {}"), SyntaxError);

    // Dimensions after property flags
    EXPECT_THROW(parse_program("symbols { A : Matrix { symmetric } (10, 20); } expressions {} operations {}"), SyntaxError);
}
