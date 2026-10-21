#pragma once

#include "basic_types.h"
#include "property_table.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace egraph {

class EGraph;

using AnalyzeFn = std::function<AnalysisData(const EGraph &egraph, const std::vector<Id> &children)>;

struct OpDescriptor {
    Op op{};
    std::string name;
    bool is_kernel = false;
    AnalyzeFn analyze = nullptr;
};

void init_builtin_ops(class OpRegistry &registry);

class OpRegistry {
  public:
    static OpRegistry &instance();

    Op register_op(const std::string &name, bool is_kernel, AnalyzeFn analyze = nullptr);

    // Queries
    bool is_kernel(Op op) const;
    static std::string_view name(Op op);
    std::optional<Op> find_by_name(std::string_view name) const;
    static Op op(std::string_view name);
    const OpDescriptor &get(Op op) const;
    bool contains(Op op) const;
    bool contains(std::string_view name) const;

  private:
    OpRegistry();
    void init_builtins();

    std::vector<OpDescriptor> ops;
    std::unordered_map<std::string, Op> name_to_op_;
};

} // namespace egraph
