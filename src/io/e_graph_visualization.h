#pragma once
#include <string>

namespace egraph {
class EGraph;

namespace EGraphVisualization {
std::string to_dot(const EGraph &egraph);
void to_dot_file(const EGraph &egraph, const std::string &filename);
void to_img(const EGraph &egraph, const std::string &filename, const std::string &format);
} // namespace EGraphVisualization

} // namespace egraph
