#include "examples.h"
#include <iostream>
#include <string>



void print_usage(const char *prog_name) {
    std::cout << "Usage: " << prog_name << " <case_name>\n";
    std::cout << "Available case names:\n";
    std::cout << "  - OLS          : Ordinary Least Squares example\n";
    std::cout << "  - GLS          : Generalized Least Squares example\n";
    std::cout << "  - Image        : Image Restoration example\n";
    std::cout << "  - Sto : Stochastic Newton example\n";
    std::cout << "  - all          : Run all example cases\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    std::string case_name = argv[1];

    if (case_name == "OLS" || case_name == "ols") {
        return run_ols();
    } else if (case_name == "GLS" || case_name == "gls") {
        return run_gls();
    } else if (case_name == "Image" || case_name == "image" || case_name == "ImageRestoration") {
        return run_image();

    } else if (case_name == "Sto" || case_name == "sto") {
        return run_stochastic_newton();
    } else if (case_name == "all" || case_name == "ALL") {
        int res = 0;
        res |= run_ols();
        res |= run_gls();
        res |= run_image();
        res |= run_stochastic_newton();
        return res;
    } else {
        std::cerr << "Unknown case name: '" << case_name << "'\n\n";
        print_usage(argv[0]);
        return 1;
    }
}
