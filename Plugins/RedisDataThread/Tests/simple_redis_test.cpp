/*
    Simple Redis Stream Test
    
    Basic test to verify Redis stream functionality works.
*/

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

int main() {
    std::cout << "Simple Redis Stream Test" << std::endl;
    std::cout << "========================" << std::endl;
    
#ifdef REDIS_ENABLED
    // Connect to Redis
    redisContext* ctx = redisConnect("localhost", 6379);
    if (!ctx || ctx->err) {
        std::cout << "❌ Failed to connect to Redis" << std::endl;
        if (ctx) {
            std::cout << "Error: " << ctx->errstr << std::endl;
            redisFree(ctx);
        }
        return 1;
    }
    
    std::cout << "✓ Connected to Redis" << std::endl;
    
    // Test PING
    redisReply* reply = (redisReply*)redisCommand(ctx, "PING");
    if (reply && reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "PONG") {
        std::cout << "✓ Redis PING successful" << std::endl;
    } else {
        std::cout << "❌ Redis PING failed" << std::endl;
        if (reply) freeReplyObject(reply);
        redisFree(ctx);
        return 1;
    }
    freeReplyObject(reply);
    
    // Test stream creation with XADD
    std::string test_stream = "test_stream_" + std::to_string(time(nullptr));
    reply = (redisReply*)redisCommand(ctx, "XADD %s * data test_value", test_stream.c_str());
    
    if (reply && reply->type == REDIS_REPLY_STRING) {
        std::cout << "✓ Stream creation with XADD successful" << std::endl;
        std::cout << "  Stream ID: " << reply->str << std::endl;
    } else {
        std::cout << "❌ Stream creation failed" << std::endl;
        if (reply) freeReplyObject(reply);
        redisFree(ctx);
        return 1;
    }
    freeReplyObject(reply);
    
    // Test stream reading with XREAD
    reply = (redisReply*)redisCommand(ctx, "XREAD BLOCK 1000 STREAMS %s 0-0", test_stream.c_str());
    
    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
        std::cout << "✓ Stream reading with XREAD successful" << std::endl;
        std::cout << "  Found " << reply->elements << " stream(s)" << std::endl;
    } else {
        std::cout << "❌ Stream reading failed" << std::endl;
        if (reply) freeReplyObject(reply);
        redisFree(ctx);
        return 1;
    }
    freeReplyObject(reply);
    
    // Test BRANDBCI-style data
    const char* brandbci_data = R"({
        "stream_id": "neural_data_test",
        "timestamp": "1234567890123-0",
        "data": {
            "channels": [1.0, 2.0, 3.0, 4.0],
            "sample_rate": 30000
        }
    })";
    
    reply = (redisReply*)redisCommand(ctx, "XADD %s * brandbci_data %s", 
                                     test_stream.c_str(), brandbci_data);
    
    if (reply && reply->type == REDIS_REPLY_STRING) {
        std::cout << "✓ BRANDBCI data format test successful" << std::endl;
    } else {
        std::cout << "❌ BRANDBCI data format test failed" << std::endl;
        if (reply) freeReplyObject(reply);
        redisFree(ctx);
        return 1;
    }
    freeReplyObject(reply);
    
    // Performance test - measure latency
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10; i++) {
        reply = (redisReply*)redisCommand(ctx, "XADD %s * sample %d", test_stream.c_str(), i);
        if (reply) freeReplyObject(reply);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_latency_ms = (duration.count() / 1000.0) / 10.0;
    
    std::cout << "✓ Performance test completed" << std::endl;
    std::cout << "  Average latency: " << avg_latency_ms << " ms per operation" << std::endl;
    
    if (avg_latency_ms < 10.0) {
        std::cout << "✓ Latency requirement met (< 10ms)" << std::endl;
    } else {
        std::cout << "⚠ Latency requirement not met (>= 10ms)" << std::endl;
    }
    
    // Cleanup
    reply = (redisReply*)redisCommand(ctx, "DEL %s", test_stream.c_str());
    if (reply) freeReplyObject(reply);
    redisFree(ctx);
    
    std::cout << "\n🎉 All basic Redis stream tests passed!" << std::endl;
    return 0;
    
#else
    std::cout << "❌ Redis support not enabled" << std::endl;
    std::cout << "Compile with -DREDIS_ENABLED=1 and link with -lhiredis" << std::endl;
    return 1;
#endif
}
