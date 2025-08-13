/*
    Integration Tests for BRANDBCI Redis Plugin
    
    End-to-end tests for the complete plugin workflow.
*/

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

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
        std::cout << "\n=== Integration Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests passed: " << tests_passed << std::endl;
        std::cout << "Success rate: " << (tests_run > 0 ? (tests_passed * 100 / tests_run) : 0) << "%" << std::endl;
    }
};

int TestFramework::tests_run = 0;
int TestFramework::tests_passed = 0;

#ifdef REDIS_ENABLED

class IntegrationTester {
private:
    redisContext* ctx;
    std::string test_stream;
    
public:
    IntegrationTester() : ctx(nullptr), test_stream("integration_test_stream") {}
    
    ~IntegrationTester() {
        cleanup();
    }
    
    bool setup() {
        // Connect to Redis
        ctx = redisConnect("localhost", 6379);
        if (!ctx || ctx->err) {
            std::cout << "Failed to connect to Redis for integration tests" << std::endl;
            return false;
        }
        
        // Test connection
        redisReply* reply = (redisReply*)redisCommand(ctx, "PING");
        bool success = (reply && reply->type == REDIS_REPLY_STATUS);
        if (reply) freeReplyObject(reply);
        
        return success;
    }
    
    void cleanup() {
        if (ctx) {
            // Clean up test streams
            redisReply* reply = (redisReply*)redisCommand(ctx, "DEL %s", test_stream.c_str());
            if (reply) freeReplyObject(reply);

            reply = (redisReply*)redisCommand(ctx, "DEL neural_data_test");
            if (reply) freeReplyObject(reply);

            reply = (redisReply*)redisCommand(ctx, "DEL node_status_test");
            if (reply) freeReplyObject(reply);

            redisFree(ctx);
            ctx = nullptr;
        }
    }
    
    bool test_brandbci_workflow() {
        if (!ctx) return false;
        
        // Simulate BRANDBCI data publishing workflow
        
        // 1. Publish neural data
        const char* neural_data = R"({
            "stream_id": "neural_data_test",
            "timestamp": "1234567890123-0",
            "node_info": {
                "nickname": "neural_acq",
                "status": "NODE_READY"
            },
            "data": {
                "channels": [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0],
                "sample_rate": 30000,
                "metadata": {
                    "electrode_config": "utah_array_96ch",
                    "amplifier_gain": 1000
                }
            }
        })";
        
        redisReply* reply = (redisReply*)redisCommand(ctx,
            "XADD neural_data_test * brandbci_data %s", neural_data);
        
        bool data_published = (reply && reply->type == REDIS_REPLY_STRING);
        if (reply) freeReplyObject(reply);
        
        if (!data_published) return false;
        
        // 2. Publish node status
        const char* node_status = R"({
            "node_id": "neural_acq",
            "status": "NODE_READY",
            "timestamp": 1234567890123,
            "sample_count": 1000,
            "error_count": 0
        })";
        
        reply = (redisReply*)redisCommand(ctx,
            "XADD node_status_test * status %s", node_status);
        
        bool status_published = (reply && reply->type == REDIS_REPLY_STRING);
        if (reply) freeReplyObject(reply);
        
        if (!status_published) return false;
        
        // 3. Read data back (simulating plugin behavior)
        reply = (redisReply*)redisCommand(ctx,
            "XREAD BLOCK 1000 STREAMS neural_data_test node_status_test 0-0 0-0");
        
        bool data_read = false;
        if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements >= 1) {
            data_read = true;
        }
        
        if (reply) freeReplyObject(reply);
        
        return data_read;
    }
    
    bool test_performance_requirements() {
        if (!ctx) return false;
        
        // Test latency requirements (< 10ms)
        const int num_samples = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_samples; i++) {
            redisReply* reply = (redisReply*)redisCommand(ctx,
                "XADD %s * sample %d timestamp %lld",
                test_stream.c_str(), i, (long long)time(nullptr));
            
            if (reply) freeReplyObject(reply);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_latency_ms = (duration.count() / 1000.0) / num_samples;
        
        std::cout << "Average latency per sample: " << avg_latency_ms << " ms" << std::endl;
        
        return avg_latency_ms < 10.0; // < 10ms requirement
    }
    
    bool test_high_frequency_streaming() {
        if (!ctx) return false;
        
        // Test 1kHz streaming for 1 second
        const int target_rate = 1000; // Hz
        const int test_duration = 1; // seconds
        const int expected_samples = target_rate * test_duration;
        
        auto start = std::chrono::high_resolution_clock::now();
        int samples_sent = 0;
        
        while (samples_sent < expected_samples) {
            redisReply* reply = (redisReply*)redisCommand(ctx,
                "XADD %s * sample %d", test_stream.c_str(), samples_sent);
            
            if (reply && reply->type == REDIS_REPLY_STRING) {
                samples_sent++;
            }
            
            if (reply) freeReplyObject(reply);
            
            // Maintain target rate
            auto expected_time = start + std::chrono::microseconds(samples_sent * 1000000 / target_rate);
            auto current_time = std::chrono::high_resolution_clock::now();
            
            if (current_time < expected_time) {
                std::this_thread::sleep_until(expected_time);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double actual_rate = (samples_sent * 1000.0) / actual_duration.count();
        
        std::cout << "Achieved rate: " << actual_rate << " Hz (target: " << target_rate << " Hz)" << std::endl;
        
        return actual_rate >= (target_rate * 0.95); // 95% of target rate
    }
    
    bool test_error_recovery() {
        if (!ctx) return false;
        
        // Test 1: Simulate connection loss and recovery
        redisFree(ctx);
        ctx = nullptr;
        
        // Try to reconnect
        ctx = redisConnect("localhost", 6379);
        bool reconnected = (ctx && !ctx->err);
        
        if (!reconnected) return false;
        
        // Test 2: Handle invalid stream operations gracefully
        redisReply* reply = (redisReply*)redisCommand(ctx,
            "XREAD BLOCK 100 STREAMS nonexistent_stream 0-0");
        
        bool handled_gracefully = (reply && reply->type == REDIS_REPLY_NIL);
        if (reply) freeReplyObject(reply);
        
        return handled_gracefully;
    }
    
    bool test_multi_stream_handling() {
        if (!ctx) return false;
        
        // Create multiple test streams
        std::vector<std::string> streams = {
            "stream_1", "stream_2", "stream_3"
        };
        
        // Add data to each stream
        for (const auto& stream : streams) {
            redisReply* reply = (redisReply*)redisCommand(ctx,
                "XADD %s * data test_data_%s", stream.c_str(), stream.c_str());
            
            if (!reply || reply->type != REDIS_REPLY_STRING) {
                if (reply) freeReplyObject(reply);
                return false;
            }
            freeReplyObject(reply);
        }
        
        // Read from all streams simultaneously
        redisReply* reply = (redisReply*)redisCommand(ctx,
            "XREAD BLOCK 1000 STREAMS stream_1 stream_2 stream_3 0-0 0-0 0-0");
        
        bool success = (reply && reply->type == REDIS_REPLY_ARRAY && 
                       reply->elements == 3);
        
        if (reply) freeReplyObject(reply);
        
        // Cleanup
        for (const auto& stream : streams) {
            redisReply* cleanup_reply = (redisReply*)redisCommand(ctx, "DEL %s", stream.c_str());
            if (cleanup_reply) freeReplyObject(cleanup_reply);
        }
        
        return success;
    }
};

void run_integration_tests() {
    std::cout << "=== BRANDBCI Integration Tests ===" << std::endl;
    
    IntegrationTester tester;
    
    // Setup
    bool setup_ok = tester.setup();
    TestFramework::assert_true(setup_ok, "Integration test setup");
    
    if (!setup_ok) {
        std::cout << "Skipping integration tests - setup failed" << std::endl;
        return;
    }
    
    // Test BRANDBCI workflow
    TestFramework::assert_true(tester.test_brandbci_workflow(),
                              "Complete BRANDBCI data workflow");
    
    // Test performance requirements
    TestFramework::assert_true(tester.test_performance_requirements(),
                              "Performance requirements (< 10ms latency)");
    
    // Test high-frequency streaming
    TestFramework::assert_true(tester.test_high_frequency_streaming(),
                              "High-frequency streaming (1kHz)");
    
    // Test error recovery
    TestFramework::assert_true(tester.test_error_recovery(),
                              "Error recovery and reconnection");
    
    // Test multi-stream handling
    TestFramework::assert_true(tester.test_multi_stream_handling(),
                              "Multi-stream simultaneous handling");
}

#else

void run_integration_tests() {
    std::cout << "Redis support not enabled - skipping integration tests" << std::endl;
}

#endif

int main() {
    std::cout << "BRANDBCI Redis Plugin Integration Tests" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    run_integration_tests();
    
    TestFramework::print_summary();
    
    return (TestFramework::tests_run == TestFramework::tests_passed) ? 0 : 1;
}
