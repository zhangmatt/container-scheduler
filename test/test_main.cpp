#include "test_suite.h"

#include <iostream>
#include <vector>

int main() {
  std::vector<NamedTest> tests;
  auto scheduler_tests = SchedulerTests();
  auto simulation_tests = SimulationTests();
  tests.insert(tests.end(), scheduler_tests.begin(), scheduler_tests.end());
  tests.insert(tests.end(), simulation_tests.begin(), simulation_tests.end());

  int failures = 0;
  for (const auto& test : tests) {
    try {
      test.run();
      std::cout << "[PASS] " << test.name << "\n";
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "[FAIL] " << test.name << "\n  " << error.what() << "\n";
    }
  }

  std::cout << tests.size() - failures << "/" << tests.size()
            << " tests passed\n";
  return failures == 0 ? 0 : 1;
}
