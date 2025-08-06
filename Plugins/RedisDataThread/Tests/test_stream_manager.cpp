/*
    Stream Manager Unit Tests
    
    Tests for multi-stream management and discovery functionality.
*/

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

// Mock JUCE classes (reuse from parser tests)
namespace juce {
    class String {
    public:
        std::string str;
        String() = default;
        String(const char* s) : str(s) {}
        String(const std::string& s) : str(s) {}
        
        const char* toRawUTF8() const { return str.c_str(); }
        size_t length() const { return str.length(); }
    };
    
    template<typename T>
    class Array {
    public:
        std::vector<T> data;
        
        void clear() { data.clear(); }
        void add(const T& item) { data.push_back(item); }
        size_t size() const { return data.size(); }
        T& operator[](size_t index) { return data[index]; }
        const T& operator[](size_t index) const { return data[index]; }
    };
}

using namespace juce;

// Test framework (forward declaration)
class TestFramework {
public:
    static int tests_run;
    static int tests_passed;

    static void assert_true(bool condition, const std::string& message);
    static void print_summary();
};

// Stream Manager class for testing
class BrandStreamManager {
private:
    redisContext* redisCtx;
    std::unordered_map<std::string, std::string> streamPositions;
    
public:
    BrandStreamManager(redisContext* ctx) : redisCtx(ctx) {}
    
    ~BrandStreamManager() {}
    
    Array<String> discoverStreams(const String& pattern) {
        Array<String> streams;
        
#ifdef REDIS_ENABLED
        if (!redisCtx) return streams;
        
        // Use KEYS command to find streams matching pattern
        redisReply* reply = (redisReply*)redisCommand(redisCtx, "KEYS %s", pattern.toRawUTF8());
        
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < reply->elements; i++) {
                if (reply->element[i]->type == REDIS_REPLY_STRING) {
                    // Verify it's actually a stream
                    redisReply* type_reply = (redisReply*)redisCommand(redisCtx, 
                        "TYPE %s", reply->element[i]->str);
                    
                    if (type_reply && type_reply->type == REDIS_REPLY_STATUS &&
                        std::string(type_reply->str) == "stream") {
                        streams.add(String(reply->element[i]->str));
                    }
                    
                    if (type_reply) freeReplyObject(type_reply);
                }
            }
        }
        
        if (reply) freeReplyObject(reply);
#endif
        
        return streams;
    }
    
    bool subscribeToStream(const String& streamName, const String& startId = "0-0") {
#ifdef REDIS_ENABLED
        if (!redisCtx) return false;
        
        // Check if stream exists
        redisReply* reply = (redisReply*)redisCommand(redisCtx, "EXISTS %s", streamName.toRawUTF8());
        bool exists = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
        
        if (reply) freeReplyObject(reply);
        
        if (exists) {
            streamPositions[streamName.str] = startId.str;
            return true;
        }
#endif
        return false;
    }
    
    bool readStreamData(const String& streamName, String& data, String& newId) {
#ifdef REDIS_ENABLED
        if (!redisCtx) return false;
        
        auto it = streamPositions.find(streamName.str);
        if (it == streamPositions.end()) return false;
        
        std::string lastId = it->second;
        
        // Read from stream
        redisReply* reply = (redisReply*)redisCommand(redisCtx,
            "XREAD BLOCK 1000 STREAMS %s %s",
            streamName.toRawUTF8(),
            lastId.c_str());
        
        bool success = false;
        if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
            // Parse stream data
            if (reply->element[0]->type == REDIS_REPLY_ARRAY && 
                reply->element[0]->elements == 2) {
                
                // Get entries array
                redisReply* entries = reply->element[0]->element[1];
                if (entries->type == REDIS_REPLY_ARRAY && entries->elements > 0) {
                    // Get first entry
                    redisReply* entry = entries->element[0];
                    if (entry->type == REDIS_REPLY_ARRAY && entry->elements >= 2) {
                        // Update stream position
                        newId = String(entry->element[0]->str);
                        streamPositions[streamName.str] = newId.str;
                        
                        // Extract data (simplified)
                        data = String("test_data");
                        success = true;
                    }
                }
            }
        }
        
        if (reply) freeReplyObject(reply);
        return success;
#else
        return false;
#endif
    }
    
    void unsubscribeFromStream(const String& streamName) {
        streamPositions.erase(streamName.str);
    }
    
    bool isStreamActive(const String& streamName) const {
        return streamPositions.find(streamName.str) != streamPositions.end();
    }
    
    size_t getActiveStreamCount() const {
        return streamPositions.size();
    }
};

#ifdef REDIS_ENABLED

void run_stream_manager_tests() {
    std::cout << "\n=== Stream Manager Tests ===" << std::endl;
    
    // Connect to Redis
    redisContext* ctx = redisConnect("localhost", 6379);
    if (!ctx || ctx->err) {
        std::cout << "Skipping stream manager tests - no Redis connection" << std::endl;
        if (ctx) redisFree(ctx);
        return;
    }
    
    BrandStreamManager manager(ctx);
    
    // Test 1: Stream discovery with no streams
    Array<String> empty_streams = manager.discoverStreams(String("nonexistent_*"));
    TestFramework::assert_true(empty_streams.size() == 0, "Discover no streams when none exist");
    
    // Test 2: Create test streams and discover them
    std::string test_stream1 = "test_neural_stream_1";
    std::string test_stream2 = "test_neural_stream_2";
    
    // Create test streams
    redisReply* reply1 = (redisReply*)redisCommand(ctx, "XADD %s * data test1", test_stream1.c_str());
    redisReply* reply2 = (redisReply*)redisCommand(ctx, "XADD %s * data test2", test_stream2.c_str());
    
    bool streams_created = (reply1 && reply1->type == REDIS_REPLY_STRING &&
                           reply2 && reply2->type == REDIS_REPLY_STRING);
    
    if (reply1) freeReplyObject(reply1);
    if (reply2) freeReplyObject(reply2);
    
    TestFramework::assert_true(streams_created, "Create test streams");
    
    if (streams_created) {
        // Test 3: Discover created streams
        Array<String> found_streams = manager.discoverStreams(String("test_neural_*"));
        TestFramework::assert_true(found_streams.size() >= 2, "Discover created streams");
        
        // Test 4: Subscribe to streams
        bool sub1 = manager.subscribeToStream(String(test_stream1));
        bool sub2 = manager.subscribeToStream(String(test_stream2));
        TestFramework::assert_true(sub1 && sub2, "Subscribe to existing streams");
        
        // Test 5: Check active stream count
        TestFramework::assert_true(manager.getActiveStreamCount() == 2, "Correct active stream count");
        
        // Test 6: Check stream active status
        TestFramework::assert_true(manager.isStreamActive(String(test_stream1)), "Stream 1 is active");
        TestFramework::assert_true(manager.isStreamActive(String(test_stream2)), "Stream 2 is active");
        
        // Test 7: Try to subscribe to non-existent stream
        bool sub_invalid = manager.subscribeToStream(String("nonexistent_stream"));
        TestFramework::assert_true(!sub_invalid, "Reject subscription to non-existent stream");
        
        // Test 8: Read stream data
        String data, newId;
        bool read_success = manager.readStreamData(String(test_stream1), data, newId);
        TestFramework::assert_true(read_success, "Read data from subscribed stream");
        
        // Test 9: Unsubscribe from stream
        manager.unsubscribeFromStream(String(test_stream1));
        TestFramework::assert_true(!manager.isStreamActive(String(test_stream1)), "Stream 1 unsubscribed");
        TestFramework::assert_true(manager.getActiveStreamCount() == 1, "Active count after unsubscribe");
        
        // Cleanup test streams
        redisCommand(ctx, "DEL %s", test_stream1.c_str());
        redisCommand(ctx, "DEL %s", test_stream2.c_str());
    }
    
    redisFree(ctx);
}

#else

void run_stream_manager_tests() {
    std::cout << "\n=== Stream Manager Tests ===" << std::endl;
    std::cout << "Redis support not enabled - skipping stream manager tests" << std::endl;
}

#endif
