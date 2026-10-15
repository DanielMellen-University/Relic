// Small hand-written test harness (see docs/TESTING.md). No test library.
#include "core.hpp"
#include "diag/diag.hpp"

#include <iostream>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << __FILE__ << ":" << __LINE__ << ": FAIL " #cond "\n"; \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

int main() {
    CHECK(relic::version_string().rfind("relic ", 0) == 0);
    CHECK(relic::diag::format("a.relic", 3, 5, "x") == "a.relic:3:5: error: x\n");

    auto empty = relic::run_source("e.relic", "");
    CHECK(empty.exit_code == 0 && empty.out.empty());

    auto comments = relic::run_source("c.relic", "# hi\n\n    # indented\n");
    CHECK(comments.exit_code == 0 && comments.err.empty());

    auto code = relic::run_source("p.relic", "\n  print(\"hi\")\n");
    CHECK(code.exit_code == 1);
    CHECK(code.err.rfind("p.relic:2:3: error:", 0) == 0);

    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "unit: all passed\n";
    return 0;
}
