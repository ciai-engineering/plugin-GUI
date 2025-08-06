/*
    Test Runner for Redis Stream Support
    
    Combines all unit tests into a single executable.
*/

#include <iostream>
#include <string>

// Test framework implementation
class TestFramework {
public:
    static int tests_run;
    static int tests_passed;
    
    static void assert_true(bool condition, const std::string& message) {
        tests_run++;
        if (condition) {
            tests_passed++;
            std::cout << "✓ PASS: " << message << std::endl;
        } else {
            std::cout << "✗ FAIL: " << message << std::endl;
        }
    }
    
    static void print_summary() {
        std::cout << "\n=== Overall Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests passed: " << tests_passed << std::endl;
        std::cout << "Success rate: " << (tests_run > 0 ? (tests_passed * 100 / tests_run) : 0) << "%" << std::endl;
        
        if (tests_passed == tests_run) {
            std::cout << "🎉 All tests passed!" << std::endl;
        } else {
            std::cout << "❌ Some tests failed." << std::endl;
        }
    }
};

int TestFramework::tests_run = 0;
int TestFramework::tests_passed = 0;

// Forward declarations for test functions
void run_redis_stream_tests();
void run_brandbci_parser_tests();
void run_stream_manager_tests();

int main() {
    std::cout << "Redis Stream Support Test Suite" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Run all test suites
    run_redis_stream_tests();
    run_brandbci_parser_tests();
    run_stream_manager_tests();
    
    // Print overall summary
    TestFramework::print_summary();
    
    return (TestFramework::tests_run == TestFramework::tests_passed) ? 0 : 1;
}
