#include "basic_types.h"
#include "e_graph.h"
#include "errors.h"
#include "expression.h"
#include "extractor.h"
#include "property_table.h"
#include "rewrite_sets.h"
#include "rewriter.h"
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

struct SessionState {
    std::vector<std::string> expressions;
    EGraph egraph;
    std::vector<Rewrite> rewrites;
};

std::vector<std::string> split_tokens(const std::string &line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
    return tokens;
}

std::string join_tokens(const std::vector<std::string> &tokens, size_t start_idx) {
    std::string out;
    for (size_t i = start_idx; i < tokens.size(); ++i) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += tokens[i];
    }
    return out;
}

static const std::vector<std::string_view> AvailableRuleSets = {
    "factorization", "algebraic", "inverse", "orthogonality", "zero-negation",
};

void print_available_rule_sets() {
    std::cout << "Available rule sets:\n";
    for (std::string_view name : AvailableRuleSets) {
        std::cout << "  - " << name << "\n";
    }
}

bool is_available_rule_set(const std::string &name) {
    for (std::string_view candidate : AvailableRuleSets) {
        if (name == candidate) {
            return true;
        }
    }
    return false;
}

void parse_expression(SessionState &state, const std::string &expr_str) {
    Expression expr = Expression(expr_str);
    Id id = state.egraph.add_expression(expr);
    std::cout << "Parsed expression with root id " << id << ".\n";
}

void add_properties_to_state(SessionState &state, const std::vector<std::string> &property_strings) {
    for (const auto &string : property_strings) {
        try {
            if (state.egraph.get_property_table().add_or_update_property_entry_by_string(string)) {
                std::cout << "Updated property: " << string << "\n";
            } else {
                std::cout << "Added property: " << string << "\n";
            }
        } catch (const ParseError &e) {
            std::cerr << "Failed to parse property string '" << string << "': " << e.what() << "\n";
        }
    }
}

void add_rule_set_to_state(SessionState &state, const std::vector<std::string> &set_names) {
    for (const auto &set_name : set_names) {
        if (!is_available_rule_set(set_name)) {
            throw std::invalid_argument("Unknown rule set: " + set_name);
        }
        std::vector<Rewrite> set_rewrites = get_rewrite_set_by_name(set_name);
        state.rewrites.insert(state.rewrites.end(), set_rewrites.begin(), set_rewrites.end());
    }
}

void rewrite_e_graph(SessionState &state, std::optional<int> num_iterations) {
    if (state.rewrites.empty()) {
        std::cerr << "No rewrites enabled. Use 'add rule-set <name>' to add a rewrite set.\n";
        return;
    }
    Rewriter rewriter(state.egraph, state.rewrites, 1000);
    if (num_iterations.has_value()) {
        rewriter.apply_rewrites(num_iterations.value());
    } else {
        rewriter.apply_rewrites();
    }
}

void extract_expression(SessionState &state, size_t expression_id) {
    if (!state.egraph.find_node(expression_id).has_value()) {
        std::cerr << "Expression id " << expression_id
                  << " does not exist in the e-graph. Make sure to rewrite the e-graph after parsing to populate the "
                     "e-classes.\n";
        return;
    }
    CostStorage cost_storage(state.egraph);
    Extractor extractor(state.egraph, cost_storage);
    auto result = extractor.extract(expression_id);
    std::cout << "Best extracted expression: " << result.expr.to_human_string() << "\n";
    std::cout << "Cost: " << result.cost << "\n";
}

void print_session_state(const SessionState &state) {
    std::cout << "Expressions:\n";
    if (state.expressions.empty()) {
        std::cout << "  <none>\n";
    } else {
        for (size_t i = 0; i < state.expressions.size(); ++i) {
            auto id = state.egraph.find_expression_id(Expression(state.expressions[i]));
            if (id.has_value()) {
                std::cout << "  [" << id.value() << "] " << state.expressions[i] << "\n";
            } else {
                std::cout << "  [unresolved] " << state.expressions[i] << "\n";
            }
        }
    }

    std::cout << "Properties:\n";
    state.egraph.get_property_table().print_all_properties();
    std::cout << "Enabled rewrites :\n";
    if (state.rewrites.empty()) {
        std::cout << "  <none>\n";
    } else {
        for (const auto &rewrite : state.rewrites) {
            std::cout << "  - " << rewrite.name << "\n";
        }
    }
}

void print_repl_help() {
    std::cout
        << "Session commands:\n"
        << "  parse <expr>                         Store an expression in the session\n"
        << "  add [properties|rule-set] <item...>  Add one or more properties or rule-set names\n"
        << "  rewrite <num-iterations|null>        Rewrite for N iterations or null for rewrite until saturation\n"
        << "  extract <expr-id>                    Extract best expression for a stored expression id\n"
        << "  show [state|available-rule-sets]     Display session state or available rule-sets\n"
        << "  reset                                Clear session state\n"
        << "  help                                 Show this message\n"
        << "  quit | exit                          Leave the interactive shell\n";
}

void execute_session_command(const std::vector<std::string> &tokens, SessionState &state) {
    if (tokens.empty()) {
        return;
    }

    const std::string &command = tokens[0];

    if (command == "help") {
        print_repl_help();
        return;
    }
    if (command == "add") {
        if (tokens.size() < 3) {
            std::cerr << "usage: add [properties|rule-set] <item...>\n";
            return;
        }
        if (tokens[1] == "properties") {
            add_properties_to_state(state, std::vector<std::string>(tokens.begin() + 2, tokens.end()));
        } else if (tokens[1] == "rule-set") {
            add_rule_set_to_state(state, std::vector<std::string>(tokens.begin() + 2, tokens.end()));
        } else {
            std::cerr << "usage: add [properties|rule-set] <item...>\n";
        }
        return;
    }
    if (command == "show") {
        if (tokens.size() == 1 || tokens[1] == "state") {
            print_session_state(state);
            return;
        }
        if (tokens.size() == 2 && tokens[1] == "available-rule-sets") {
            print_available_rule_sets();
            return;
        }
        std::cerr << "usage: show [state|available-rule-sets]\n";
        return;
    }
    if (command == "reset") {
        state = SessionState{};
        std::cout << "Session reset.\n";
        return;
    }
    if (command == "parse") {
        if (tokens.size() < 2) {
            std::cerr << "parse requires an expression\n";
            return;
        }
        const std::string expression_text = join_tokens(tokens, 1);
        try {
            parse_expression(state, expression_text);
            state.expressions.push_back(expression_text);
        } catch (const ParseError &e) {
            std::cerr << e.what() << "\n";
        } catch (const std::exception &e) {
            std::cerr << "parse failed: " << e.what() << "\n";
        }
        return;
    }
    if (command == "rewrite") {
        if (!state.expressions.size()) {
            std::cerr << "No expression loaded. Use parse <expr> first.\n";
            return;
        }
        if (tokens.size() != 2) {
            std::cerr << "usage: rewrite <num-iterations|null>\n";
            return;
        }

        try {
            if (tokens[1] == "null") {
                return rewrite_e_graph(state, std::nullopt);
            }

            return rewrite_e_graph(state, std::stoi(tokens[1]));

        } catch (const ParseError &e) {
            std::cerr << e.what() << "\n";
            return;
        } catch (const std::exception &e) {
            std::cerr << "rewrite failed: " << e.what() << "\n";
            return;
        }
    }
    if (command == "extract") {
        if (!state.expressions.size()) {
            std::cerr << "No expression loaded. Use parse <expr> first.\n";
            return;
        }
        if (tokens.size() != 2 && state.expressions.size() != 1) {
            std::cerr << "usage: extract <expr-id> when you have more than one expression loaded\n";
            return;
        }
        if (tokens.size() == 1) {
            auto expression_id = state.egraph.find_expression_id(Expression(state.expressions[0]));

            if (!expression_id.has_value()) {
                std::cerr << "Failed to find the expression in the e-graph.\n";
                return;
            }
            return extract_expression(state, expression_id.value());
        }
        try {
            extract_expression(state, std::stoul(tokens[1]));
            return;
        } catch (const ParseError &e) {
            std::cerr << e.what() << "\n";
            return;
        } catch (const std::exception &e) {
            std::cerr << "extract failed: " << e.what() << "\n";
            return;
        }
    }

    std::cerr << "Unknown session command: " << command << "\n";
    return;
}

} // namespace

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    print_repl_help();

    SessionState state;
    std::string line;
    while (true) {
        if (isatty(fileno(stdin))) {
            std::cout << "egraph> " << std::flush;
        }
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        std::vector<std::string> tokens = split_tokens(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string &command = tokens[0];
        if (command == "quit" || command == "exit") {
            break;
        }

        execute_session_command(tokens, state);
    }

    return 0;
}
