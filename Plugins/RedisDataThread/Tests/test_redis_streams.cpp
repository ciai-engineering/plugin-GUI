/*
    Redis Stream Support Unit Tests
    
    Tests for Redis stream functionality (XREAD commands)
    to replace BLPOP implementation for BRANDBCI integration.
*/

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

// Simple test framework
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
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests passed: " << tests_passed << std::endl;
        std::cout << "Success rate: " << (tests_run > 0 ? (tests_passed * 100 / tests_run) : 0) << "%" << std::endl;
    }
};

int TestFramework::tests_run = 0;
int TestFramework::tests_passed = 0;

#ifdef REDIS_ENABLED

class RedisStreamTester {
private:
    redisContext* ctx;
    std::string test_stream;
    
public:
    RedisStreamTester() : ctx(nullptr), test_stream("test_stream_" + std::to_string(time(nullptr))) {}
    
    ~RedisStreamTester() {
        cleanup();
    }
    
    bool connect(const std::string& host = "localhost", int port = 6379) {
        ctx = redisConnect(host.c_str(), port);
        if (ctx == nullptr || ctx->err) {
            if (ctx) {
                std::cout << "Redis connection error: " << ctx->errstr << std::endl;
                redisFree(ctx);
                ctx = nullptr;
            } else {
                std::cout << "Redis connection error: can't allocate redis context" << std::endl;
            }
            return false;
        }
        
        // Test connection
        redisReply* reply = (redisReply*)redisCommand(ctx, "PING");
        bool success = (reply && reply->type == REDIS_REPLY_STATUS && 
                       std::string(reply->str) == "PONG");
        if (reply) freeReplyObject(reply);
        
        return success;
    }
    
    void cleanup() {
        if (ctx) {
            // Clean up test stream
            redisReply* reply = (redisReply*)redisCommand(ctx, "DEL %s", test_stream.c_str());
            if (reply) freeReplyObject(reply);
            
            redisFree(ctx);
            ctx = nullptr;
        }
    }
    
    bool test_stream_creation() {
        if (!ctx) return false;
        
        // Add data to stream using XADD
        redisReply* reply = (redisReply*)redisCommand(ctx, 
            "XADD %s * data test_value timestamp %lld", 
            test_stream.c_str(), 
            (long long)time(nullptr));
        
        bool success = (reply && reply->type == REDIS_REPLY_STRING);
        if (reply) freeReplyObject(reply);
        
        return success;
    }
    
    bool test_stream_read() {
        if (!ctx) return false;
        
        // First add some test data
        redisReply* add_reply = (redisReply*)redisCommand(ctx,
            "XADD %s * channels [1.0,2.0,3.0] timestamp %lld",
            test_stream.c_str(),
            (long long)time(nullptr));
        
        if (!add_reply || add_reply->type != REDIS_REPLY_STRING) {
            if (add_reply) freeReplyObject(add_reply);
            return false;
        }
        freeReplyObject(add_reply);
        
        // Now read using XREAD
        redisReply* read_reply = (redisReply*)redisCommand(ctx,
            "XREAD BLOCK 1000 STREAMS %s 0-0",
            test_stream.c_str());
        
        bool success = false;
        if (read_reply && read_reply->type == REDIS_REPLY_ARRAY && read_reply->elements > 0) {
            // XREAD returns array of [stream_name, [entries]]
            if (read_reply->element[0]->type == REDIS_REPLY_ARRAY && 
                read_reply->element[0]->elements == 2) {
                success = true;
            }
        }
        
        if (read_reply) freeReplyObject(read_reply);
        return success;
    }
    
    bool test_stream_timeout() {
        if (!ctx) return false;
        
        std::string empty_stream = "empty_stream_" + std::to_string(time(nullptr));
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Try to read from non-existent stream with 1 second timeout
        redisReply* reply = (redisReply*)redisCommand(ctx,
            "XREAD BLOCK 1000 STREAMS %s 0-0",
            empty_stream.c_str());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        bool success = false;
        if (reply) {
            // Should return NIL on timeout
            success = (reply->type == REDIS_REPLY_NIL);
            freeReplyObject(reply);
        }
        
        // Should take approximately 1 second
        bool timeout_correct = (duration.count() >= 900 && duration.count() <= 1500);
        
        return success && timeout_correct;
    }
    
    bool test_multiple_streams() {
        if (!ctx) return false;
        
        std::string stream1 = test_stream + "_1";
        std::string stream2 = test_stream + "_2";
        
        // Add data to both streams
        redisReply* reply1 = (redisReply*)redisCommand(ctx,
            "XADD %s * data stream1_data", stream1.c_str());
        redisReply* reply2 = (redisReply*)redisCommand(ctx,
            "XADD %s * data stream2_data", stream2.c_str());
        
        bool add_success = (reply1 && reply1->type == REDIS_REPLY_STRING &&
                           reply2 && reply2->type == REDIS_REPLY_STRING);
        
        if (reply1) freeReplyObject(reply1);
        if (reply2) freeReplyObject(reply2);
        
        if (!add_success) return false;
        
        // Read from both streams
        redisReply* read_reply = (redisReply*)redisCommand(ctx,
            "XREAD BLOCK 1000 STREAMS %s %s 0-0 0-0",
            stream1.c_str(), stream2.c_str());
        
        bool success = false;
        if (read_reply && read_reply->type == REDIS_REPLY_ARRAY) {
            // Should return data from both streams
            success = (read_reply->elements == 2);
        }
        
        if (read_reply) freeReplyObject(read_reply);
        
        // Cleanup
        redisCommand(ctx, "DEL %s", stream1.c_str());
        redisCommand(ctx, "DEL %s", stream2.c_str());
        
        return success;
    }
    
    bool test_brandbci_data_format() {
        if (!ctx) return false;
        
        // Test BRANDBCI-style data format
        const char* brandbci_data = R"({
            "stream_id": "neural_data_test",
            "timestamp": "1234567890123-0",
            "node_info": {
                "nickname": "test_node",
                "status": "NODE_READY"
            },
            "data": {
                "channels": [1.0, 2.0, 3.0, 4.0],
                "sample_rate": 30000,
                "metadata": {
                    "electrode_config": "test_array",
                    "amplifier_gain": 1000
                }
            }
        })";
        
        // Add BRANDBCI format data to stream
        redisReply* reply = (redisReply*)redisCommand(ctx,
            "XADD %s * brandbci_data %s",
            test_stream.c_str(),
            brandbci_data);
        
        bool success = (reply && reply->type == REDIS_REPLY_STRING);
        if (reply) freeReplyObject(reply);
        
        return success;
    }
};

void run_redis_stream_tests() {
    std::cout << "=== Redis Stream Tests ===" << std::endl;
    
    RedisStreamTester tester;
    
    // Test Redis connection
    bool connected = tester.connect();
    TestFramework::assert_true(connected, "Redis connection established");
    
    if (!connected) {
        std::cout << "Skipping Redis tests - no connection" << std::endl;
        return;
    }
    
    // Test stream creation
    TestFramework::assert_true(tester.test_stream_creation(), 
                              "Stream creation with XADD");
    
    // Test stream reading
    TestFramework::assert_true(tester.test_stream_read(), 
                              "Stream reading with XREAD");
    
    // Test timeout behavior
    TestFramework::assert_true(tester.test_stream_timeout(), 
                              "XREAD timeout behavior");
    
    // Test multiple streams
    TestFramework::assert_true(tester.test_multiple_streams(), 
                              "Multiple stream reading");
    
    // Test BRANDBCI data format
    TestFramework::assert_true(tester.test_brandbci_data_format(), 
                              "BRANDBCI data format support");
}

#else

void run_redis_stream_tests() {
    std::cout << "Redis support not enabled - skipping stream tests" << std::endl;
}

#endif

int main() {
    std::cout << "Redis Stream Support Unit Tests" << std::endl;
    std::cout << "===============================" << std::endl;
    
    run_redis_stream_tests();
    
    TestFramework::print_summary();
    
    return (TestFramework::tests_run == TestFramework::tests_passed) ? 0 : 1;
}
