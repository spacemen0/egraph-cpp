#include "egraph_config.h"
#include "expression.h"
#include <gtest/gtest.h>
#include <sstream>

using namespace egraph;

TEST(ConfigTest, InitializeConfigForSimpleExpression) {
    Expression simple("A");
    auto config = initialize_config_for_expression(simple);

    EXPECT_GE(config.rewrite.node_limit, 5000);
    EXPECT_GE(config.rewrite.max_iterations, 10);
    EXPECT_GE(config.pruner.rewrite_steps_per_iteration, 10);
    EXPECT_GE(config.pruner.prune_samples_per_iteration, 10);
}

TEST(ConfigTest, InitializeConfigForDeepExpression) {
    Expression deep("((((A + B) * (C + D)) + ((A + B) * (C + D))) * (A + B))");
    auto config = initialize_config_for_expression(deep);

    EXPECT_GE(config.rewrite.node_limit, 5000);
    EXPECT_GE(config.rewrite.max_iterations, 10);
    EXPECT_GE(config.pruner.rewrite_steps_per_iteration, 10);
}

TEST(ConfigTest, PrintConfigOutput) {
    EGraphConfig config;
    std::stringstream buffer;
    std::streambuf *old_cout = std::cout.rdbuf(buffer.rdbuf());
    config.print_config();
    std::cout.rdbuf(old_cout);

    std::string out = buffer.str();
    EXPECT_TRUE(out.find("EGraphConfig:") != std::string::npos);
    EXPECT_TRUE(out.find("RewriteConfig:") != std::string::npos);
    EXPECT_TRUE(out.find("node_limit: 5000") != std::string::npos);
    EXPECT_TRUE(out.find("ExtractorConfig:") != std::string::npos);
    EXPECT_TRUE(out.find("PrunerConfig:") != std::string::npos);
}
