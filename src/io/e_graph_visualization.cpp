#include "e_graph_visualization.h"
#include "e_graph.h"
#include "utils.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace EGraphVisualization {
std::string to_dot(const EGraph &egraph) {
    std::ostringstream oss;
    oss << "digraph egraph {\n";
    oss << "  compound=true;\n";
    oss << "  clusterrank=local;\n\n";
    oss << " graph [ranksep = 1.0, nodesep = 0.5, fontsize = 10];\n\n";

    for (const auto class_id : egraph.get_all_class_ids()) {
        oss << "  subgraph cluster_" << class_id << " {\n";
        oss << "    style=dotted;\n";

        const auto representative = get_representative_expression(egraph, class_id);
        oss << "    label=\" EC-" << class_id << ": " << representative.to_string() << "\";\n";

        int i = 0;
        for (const auto &node : egraph.get_class_nodes(class_id)) {
            oss << "    node_" << class_id << "_" << i << " [label=\"" << node->to_string() << "\"];\n";
            i++;
        }
        oss << "  }\n";
    }

    oss << "\n";

    for (const auto class_id : egraph.get_all_class_ids()) {
        int i_in_class = 0;

        for (const auto &node : egraph.get_class_nodes(class_id)) {
            int arg_i = 0;
            int len = node->get_children().size();

            for (auto child_id : node->get_children()) {
                auto child_leader = egraph.find_class_id(child_id);

                std::string anchor = "";
                std::string label = "";

                // Determine anchor and label based on the number of children (just like
                // egg)
                if (len == 1 && arg_i == 0) {
                } else if (len == 2 && arg_i == 0) {
                    anchor = ":sw";
                } else if (len == 2 && arg_i == 1) {
                    anchor = ":se";
                } else if (len == 3 && arg_i == 0) {
                    anchor = ":sw";
                } else if (len == 3 && arg_i == 1) {
                    anchor = ":s";
                } else if (len == 3 && arg_i == 2) {
                    anchor = ":se";
                } else {
                    label = "label=" + std::to_string(arg_i);
                }

                if (child_leader == class_id) {
                    // Self-edge to the same eclass
                    oss << "  node_" << class_id << "_" << i_in_class << anchor << " -> node_" << class_id << "_"
                        << i_in_class << ":n";
                    if (!label.empty())
                        oss << ", " << label;
                    oss << "\n";
                } else {
                    // Edge to a different eclass (points to the 0th node of the target
                    // cluster)
                    oss << "  node_" << class_id << "_" << i_in_class << anchor << " -> node_" << child_leader
                        << "_0 [lhead=cluster_" << child_leader;
                    if (!label.empty())
                        oss << ", " << label;
                    oss << "]\n";
                }
                arg_i++;
            }
            i_in_class++;
        }
    }

    oss << "}\n";
    return oss.str();
}

void to_dot_file(const EGraph &egraph, const std::string &filename) {
    std::ofstream out(filename);
    if (out.is_open()) {
        out << to_dot(egraph);
        out.close();
    }
}

void to_img(const EGraph &egraph, const std::string &filename, const std::string &format) {
    if (format != "png" && format != "svg") {
        std::cerr << "Error: Unsupported format '" << format << "'. Supported formats are 'png' and 'svg'."
                  << std::endl;
        return;
    }
    std::string dot_filename = filename + ".dot";
    to_dot_file(egraph, dot_filename);
    std::string command = "dot -T" + format + " " + dot_filename + " -o " + filename + "." + format;
    int result = system((command + " > /dev/null 2>&1").c_str());
    if (result != 0) {
        std::cerr << "Error: Failed to execute command: " << command << std::endl;
    }
    std::remove(dot_filename.c_str());
}
} // namespace EGraphVisualization
