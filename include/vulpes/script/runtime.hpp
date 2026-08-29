#pragma once

#include "vulpes/model/lifecycle.hpp"
#include "vulpes/script/hook.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace vulpes::script {

struct Limits {
    std::size_t memory_bytes{1U * 1024U * 1024U};
    std::size_t instruction_count{100'000U};
};

// Runs independent Lua chunks against a deliberately data-only host API. A
// fresh interpreter is created for every invocation, which prevents mutable
// globals, modules, and state from leaking between application events.
class Runtime final : public model::DatasetLifecycle {
  public:
    explicit Runtime(std::vector<Definition> definitions, Limits limits = {});

    void on_open() const;
    void on_command(std::string_view command) const;
    void invoke(model::DatasetHook hook, model::DatasetRecord& record) override;

  private:
    void invoke(Hook hook, model::DatasetRecord* record, std::string_view command) const;

    std::vector<Definition> definitions_;
    Limits limits_;
};

} // namespace vulpes::script
