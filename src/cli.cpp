#include "api.h"
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif
#include <vector>


namespace egraph {
using namespace EGraphRunner;

namespace {

struct SessionState {
    std::vector<std::string> expressions;
    Context ctx;
    std::vector<std::string> rulesets;
    bool numeric_mode = true;
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
    "complete", "simplification", "transformation", "expansion", "lowering",
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

bool parse_session_mode(const std::string &text, bool &numeric_mode) {
    if (text == "numeric") {
        numeric_mode = true;
        return true;
    }
    if (text == "symbolic") {
        numeric_mode = false;
        return true;
    }
    return false;
}

bool parse_property_assignment(const std::string &string_value, std::string &name, std::string &property_str) {
    auto name_end = string_value.find(':');
    if (name_end == std::string::npos) {
        throw std::runtime_error("Property assignment must use '<name>:<property>' (missing ':')");
    }

    name = std::string(string_value.substr(0, name_end));
    // strip whitespace
    name.erase(0, name.find_first_not_of(" \t\r\n"));
    name.erase(name.find_last_not_of(" \t\r\n") + 1);

    property_str = std::string(string_value.substr(name_end + 1));
    property_str.erase(0, property_str.find_first_not_of(" \t\r\n"));
    property_str.erase(property_str.find_last_not_of(" \t\r\n") + 1);
    return true;
}

bool parse_binding_token(const std::string &token, std::string &key, int &value) {
    size_t eq = token.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
        return false;
    }

    key = token.substr(0, eq);
    try {
        value = std::stoi(token.substr(eq + 1));
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

void parse_expression(SessionState &state, const std::string &expr_str, size_t index) {
    Expression expr = Expression(expr_str);
    Id id = state.ctx.add(expr);
    std::cout << "Parsed expression #" << index << " with root id " << id << ".\n";
}

void add_properties_to_state(SessionState &state, const std::vector<std::string> &property_strings) {
    for (const auto &string : property_strings) {
        try {
            std::string name;
            std::string property_str;
            parse_property_assignment(string, name, property_str);
            state.ctx.define_matrix(name, property_str);
            std::cout << "Added (Updated) property.\n";
        } catch (const std::exception &e) {
            std::cerr << "Failed to parse property string '" << string << "': " << e.what() << "\n";
        }
    }
}

void add_rule_set_to_state(SessionState &state, const std::vector<std::string> &set_names) {
    for (const auto &set_name : set_names) {
        if (!is_available_rule_set(set_name)) {
            throw std::invalid_argument("Unknown rule set: " + set_name);
        }
        state.rulesets.push_back(set_name);
    }
}

void rewrite_e_graph(SessionState &state, std::optional<int> num_iterations) {
    if (state.rulesets.empty()) {
        std::cerr << "No rewrites enabled. Use 'add rule-set <name>' to add a rewrite set.\n";
        return;
    }
    state.ctx.config.rewrite.node_limit = 10000;
    state.ctx.config.rewrite.enable_backoff = true;
    if (num_iterations.has_value()) {
        state.ctx.config.rewrite.max_iterations = num_iterations.value();
    }
    state.ctx.rewrite(state.rulesets);
}

void extract_expression(SessionState &state, size_t expression_id) {
    try {
        Expression expr = state.ctx.extract(expression_id).expr;
        std::cout << "Best extracted expression: " << expr.to_string(true) << "\n";
    } catch (const std::exception &e) {
        std::cerr << "Failed to extract expression: " << e.what() << "\n";
    }
}

void extract_expression_with_bindings(SessionState &state, size_t expression_id, const SizeBindings &bindings) {
    try {
        Expression expr = state.ctx.extract(expression_id, bindings).expr;
        std::cout << "Best extracted expression (with bindings): " << expr.to_string(true) << "\n";
    } catch (const std::exception &e) {
        std::cerr << "Failed to extract expression: " << e.what() << "\n";
    }
}

void extract_symbolic_expressions(SessionState &state, size_t expression_id) {
    try {
        auto results = state.ctx.extract_symbolic(expression_id);
        if (results.empty()) {
            std::cout << "No symbolic extraction candidates found.\n";
            return;
        }

        std::cout << "Symbolic extraction candidates: " << results.size() << "\n";
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "  [" << i << "] " << results[i].expr.to_string(true) << "\n";
            std::cout << "      Cost: " << results[i].cost << "\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "Failed to extract expression: " << e.what() << "\n";
    }
}

void print_session_state(const SessionState &state) {
    std::cout << "Expressions:\n";
    if (state.expressions.empty()) {
        std::cout << "  <none>\n";
    } else {
        for (size_t i = 0; i < state.expressions.size(); ++i) {
            auto id = state.ctx.find_expr(Expression(state.expressions[i]));
            if (id.has_value()) {
                std::cout << "  #" << i << " [id: " << id.value() << "] " << state.expressions[i] << "\n";
            } else {
                std::cout << "  #" << i << " [id: unresolved] " << state.expressions[i] << "\n";
            }
        }
    }

    std::cout << "Properties:\n";
    state.ctx.print_properties();
    std::cout << "Mode: " << (state.numeric_mode ? "numeric" : "symbolic") << "\n";
    std::cout << "Enabled rewrites :\n";
    if (state.rulesets.empty()) {
        std::cout << "  <none>\n";
    } else {
        for (const auto &rs : state.rulesets) {
            std::cout << "  - " << rs << "\n";
        }
    }
}

void print_repl_help() {
    std::cout << "Session commands:\n"
              << "  parse <expr>                         Store an expression in the session\n"
              << "  add [properties|rule-set] <item...>  Add one or more properties or rule-set names\n"
              << "  mode [numeric|symbolic]              Show or set session mode\n"
              << "  rewrite [num-iterations]             Rewrite for N iterations or no args for saturation\n"
              << "  extract [index] [A=10 B=20 ...]      Extract in current mode (bindings optional in symbolic mode)\n"
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
    if (command == "mode") {
        if (tokens.size() == 1) {
            std::cout << "Current mode: " << (state.numeric_mode ? "numeric" : "symbolic") << "\n";
            return;
        }
        if (tokens.size() != 2) {
            std::cerr << "usage: mode [numeric|symbolic]\n";
            return;
        }
        bool parsed_mode;
        if (!parse_session_mode(tokens[1], parsed_mode)) {
            std::cerr << "usage: mode [numeric|symbolic]\n";
            return;
        }
        state.numeric_mode = parsed_mode;
        std::cout << "Mode set to " << (state.numeric_mode ? "numeric" : "symbolic") << ".\n";
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
            parse_expression(state, expression_text, state.expressions.size());
            state.expressions.push_back(expression_text);
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
        if (tokens.size() > 2) {
            std::cerr << "usage: rewrite [num-iterations]\n";
            return;
        }

        try {
            if (tokens.size() == 1) {
                // rewrite until saturation
                return rewrite_e_graph(state, std::nullopt);
            }

            return rewrite_e_graph(state, std::stoi(tokens[1]));

        } catch (const std::exception &e) {
            std::cerr << "Command failed: " << e.what() << "\n";
            return;
        }
    }
    if (command == "extract") {
        if (!state.expressions.size()) {
            std::cerr << "No expression loaded. Use parse <expr> first.\n";
            return;
        }

        size_t arg_start = 1;
        std::optional<size_t> requested_val;
        if (tokens.size() > 1 && tokens[1].find('=') == std::string::npos) {
            try {
                requested_val = std::stoul(tokens[1]);
            } catch (const std::exception &) {
                std::cerr << "extract: invalid expression index or id '" << tokens[1] << "'\n";
                return;
            }
            arg_start = 2;
        }

        size_t expression_id = 0;
        if (requested_val.has_value()) {
            size_t val = requested_val.value();
            if (val < state.expressions.size()) {
                auto id_opt = state.ctx.find_expr(Expression(state.expressions[val]));
                if (id_opt.has_value()) {
                    expression_id = id_opt.value();
                } else {
                    std::cerr << "Failed to find expression #" << val << " in the e-graph.\n";
                    return;
                }
            } else {
                std::cerr << "extract: invalid expression index " << val << ". "
                          << "Available indices: 0 to " << (state.expressions.size() - 1) << ".\n";
                return;
            }
        } else if (state.expressions.size() == 1) {
            auto expression_id_opt = state.ctx.find_expr(Expression(state.expressions[0]));
            if (!expression_id_opt.has_value()) {
                std::cerr << "Failed to find the expression in the e-graph.\n";
                return;
            }
            expression_id = expression_id_opt.value();
        } else {
            std::cerr << "usage: extract [index] [A=10 B=20 ...] when you have more than one expression loaded\n";
            return;
        }

        SizeBindings bindings;
        for (size_t i = arg_start; i < tokens.size(); ++i) {
            std::string key;
            int value = 0;
            if (!parse_binding_token(tokens[i], key, value)) {
                std::cerr << "Invalid size binding token '" << tokens[i] << "'. Expected format KEY=INT\n";
                return;
            }
            bindings[key] = value;
        }

        try {
            if (!state.numeric_mode) {
                if (bindings.empty()) {
                    extract_symbolic_expressions(state, expression_id);
                } else {
                    extract_expression_with_bindings(state, expression_id, bindings);
                }
            } else {
                if (!bindings.empty()) {
                    std::cerr << "Numeric mode does not accept size bindings in extract. Use mode symbolic first.\n";
                    return;
                }
                extract_expression(state, expression_id);
            }
            return;
        } catch (const std::exception &e) {
            std::cerr << "Command failed: " << e.what() << "\n";
            return;
        }
    }

    std::cerr << "Unknown session command: " << command << "\n";
    return;
}

} // namespace

} // namespace egraph

using namespace egraph;

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

