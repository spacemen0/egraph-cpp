#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct SessionState {
    bool has_expression = false;
    std::string expression;
    std::vector<std::string> rule_sets;
};

void print_repl_help() {
    std::cout << "Session commands:\n"
              << "  parse <expr>           Store the current expression\n"
              << "  add-rule-set <name>    Enable a named rewrite preset/profile\n"
              << "  rewrite                Run rewrite pass on stored expression (TODO)\n"
              << "  extract                Extract best expression (TODO)\n"
              << "  show                   Display current session state\n"
              << "  reset                  Clear session state\n"
              << "  help                   Show this message\n"
              << "  quit | exit            Leave the interactive shell\n";
}

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

int execute_session_command(const std::vector<std::string> &tokens, SessionState &state) {
    if (tokens.empty()) {
        return 0;
    }

    const std::string &command = tokens[0];

    if (command == "help") {
        print_repl_help();
        return 0;
    }
    if (command == "show") {
        if (state.has_expression) {
            std::cout << "Current expression: " << state.expression << "\n";
        } else {
            std::cout << "Current expression: <none>\n";
        }
        std::cout << "Rule sets: ";
        if (state.rule_sets.empty()) {
            std::cout << "<none>\n";
        } else {
            for (size_t i = 0; i < state.rule_sets.size(); ++i) {
                std::cout << state.rule_sets[i];
                if (i + 1 < state.rule_sets.size()) {
                    std::cout << ", ";
                }
            }
            std::cout << "\n";
        }
        return 0;
    }
    if (command == "reset") {
        state = SessionState{};
        std::cout << "Session reset.\n";
        return 0;
    }
    if (command == "parse") {
        if (tokens.size() < 2) {
            std::cerr << "parse requires an expression\n";
            return 2;
        }
        state.expression = join_tokens(tokens, 1);
        state.has_expression = true;
        std::cout << "Stored expression.\n";
        return 0;
    }
    if (command == "add-rule-set") {
        if (tokens.size() != 2) {
            std::cerr << "add-rule-set requires exactly one name\n";
            return 2;
        }
        state.rule_sets.push_back(tokens[1]);
        std::cout << "Added rule set: " << tokens[1] << "\n";
        return 0;
    }
    if (command == "rewrite") {
        if (!state.has_expression) {
            std::cerr << "No expression loaded. Use parse <expr> first.\n";
            return 2;
        }
        std::cout << "rewrite: TODO (expression and rule sets are available in session state)\n";
        return 0;
    }
    if (command == "extract") {
        if (!state.has_expression) {
            std::cerr << "No expression loaded. Use parse <expr> first.\n";
            return 2;
        }
        std::cout << "extract: TODO (current expression ready for extraction)\n";
        return 0;
    }

    std::cerr << "Unknown session command: " << command << "\n";
    return 2;
}

void print_usage(const char *prog) {
    std::cout << "Usage: " << prog << " <command> [options]\n\n"
              << "Commands:\n"
              << "  parse      Parse an input expression (TODO)\n"
              << "  rewrite    Run rewrite preset/profile (TODO)\n"
              << "  extract    Extract best expression (TODO)\n"
              << "  repl       Start interactive shell\n"
              << "  help       Show this message\n";
}

int run_parse(const std::vector<std::string_view> &args) {
    (void)args;
    std::cerr << "parse: not implemented yet\n";
    return 2;
}

int run_rewrite(const std::vector<std::string_view> &args) {
    (void)args;
    std::cerr << "rewrite: not implemented yet\n";
    return 2;
}

int run_extract(const std::vector<std::string_view> &args) {
    (void)args;
    std::cerr << "extract: not implemented yet\n";
    return 2;
}

int run_repl(const std::vector<std::string_view> &args) {
    (void)args;

    SessionState state;
    std::cout << "egraph session started. Type 'help' for commands.\n";

    std::string line;
    while (true) {
        std::cout << "egraph> ";
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        std::vector<std::string> tokens = split_tokens(line);
        if (!tokens.empty() && (tokens[0] == "quit" || tokens[0] == "exit")) {
            break;
        }

        (void)execute_session_command(tokens, state);
    }

    std::cout << "Session ended.\n";
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string_view command(argv[1]);
    std::vector<std::string_view> args;
    args.reserve(static_cast<size_t>(argc > 2 ? argc - 2 : 0));
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (command == "help" || command == "--help" || command == "-h") {
        print_usage(argv[0]);
        return 0;
    }
    if (command == "parse") {
        return run_parse(args);
    }
    if (command == "rewrite") {
        return run_rewrite(args);
    }
    if (command == "extract") {
        return run_extract(args);
    }
    if (command == "repl") {
        return run_repl(args);
    }

    std::cerr << "Unknown command: " << command << "\n\n";
    print_usage(argv[0]);
    return 1;
}
