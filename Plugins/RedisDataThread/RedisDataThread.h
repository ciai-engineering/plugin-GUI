/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __REDISDATATHREAD_H_INCLUDED__
#define __REDISDATATHREAD_H_INCLUDED__

#include <DataThreadHeaders.h>
#include <map>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

#include <atomic>
#include <memory>
#include <thread>
#include <unordered_map>

#ifdef REDIS_ENABLED
/**
 * RAII wrapper for Redis reply objects to prevent memory leaks
 */
class RedisReplyRAII {
private:
    redisReply* reply_;

public:
    explicit RedisReplyRAII(redisReply* reply) : reply_(reply) {}

    ~RedisReplyRAII() {
        if (reply_) {
            freeReplyObject(reply_);
        }
    }

    redisReply* get() const { return reply_; }
    redisReply* operator->() const { return reply_; }
    redisReply& operator*() const { return *reply_; }
    bool isValid() const { return reply_ != nullptr; }
    bool hasError() const { return reply_ && reply_->type == REDIS_REPLY_ERROR; }

    // Non-copyable, movable
    RedisReplyRAII(const RedisReplyRAII&) = delete;
    RedisReplyRAII& operator=(const RedisReplyRAII&) = delete;

    RedisReplyRAII(RedisReplyRAII&& other) noexcept : reply_(other.reply_) {
        other.reply_ = nullptr;
    }

    RedisReplyRAII& operator=(RedisReplyRAII&& other) noexcept {
        if (this != &other) {
            if (reply_) {
                freeReplyObject(reply_);
            }
            reply_ = other.reply_;
            other.reply_ = nullptr;
        }
        return *this;
    }
};

/**
 * RAII wrapper for Redis context to ensure proper cleanup and exception safety
 */
class RedisContextRAII {
private:
    redisContext* ctx_;

public:
    RedisContextRAII() : ctx_(nullptr) {}

    explicit RedisContextRAII(const char* host, int port, const timeval& timeout)
        : ctx_(redisConnectWithTimeout(host, port, timeout)) {}

    ~RedisContextRAII() {
        if (ctx_) {
            redisFree(ctx_);
        }
    }

    redisContext* get() const { return ctx_; }
    redisContext* operator->() const { return ctx_; }
    redisContext& operator*() const { return *ctx_; }
    bool isValid() const { return ctx_ && !ctx_->err; }
    bool hasError() const { return ctx_ && ctx_->err; }
    const char* getErrorString() const { return ctx_ ? ctx_->errstr : "null context"; }
    int getErrorCode() const { return ctx_ ? ctx_->err : -1; }

    // Release ownership of the context
    redisContext* release() {
        redisContext* temp = ctx_;
        ctx_ = nullptr;
        return temp;
    }

    // Reset with new context
    void reset(redisContext* newCtx = nullptr) {
        if (ctx_) {
            redisFree(ctx_);
        }
        ctx_ = newCtx;
    }

    // Non-copyable, movable
    RedisContextRAII(const RedisContextRAII&) = delete;
    RedisContextRAII& operator=(const RedisContextRAII&) = delete;

    RedisContextRAII(RedisContextRAII&& other) noexcept : ctx_(other.ctx_) {
        other.ctx_ = nullptr;
    }

    RedisContextRAII& operator=(RedisContextRAII&& other) noexcept {
        if (this != &other) {
            if (ctx_) {
                redisFree(ctx_);
            }
            ctx_ = other.ctx_;
            other.ctx_ = nullptr;
        }
        return *this;
    }
};

/**
 * Optimized string utilities to reduce unnecessary copies
 */
namespace StringUtils {
    // Fast string comparison without creating String objects
    inline bool equals(const char* str, size_t len, const char* target) {
        size_t targetLen = strlen(target);
        return len == targetLen && memcmp(str, target, len) == 0;
    }

    // Create String only when necessary, with length hint
    inline String createString(const char* str, size_t len) {
        return String(str, len);
    }

    // Fast string view for temporary operations
    struct StringView {
        const char* data;
        size_t length;

        StringView(const char* str, size_t len) : data(str), length(len) {}
        StringView(const char* str) : data(str), length(strlen(str)) {}

        bool equals(const char* target) const {
            size_t targetLen = strlen(target);
            return length == targetLen && memcmp(data, target, length) == 0;
        }

        bool startsWith(const char* prefix) const {
            size_t prefixLen = strlen(prefix);
            return length >= prefixLen && memcmp(data, prefix, prefixLen) == 0;
        }

        String toString() const {
            return String(data, length);
        }
    };
}

/**
 * Secure string class for sensitive data like passwords
 * Automatically clears memory on destruction and provides secure operations
 */
class SecureString {
private:
    std::vector<char> data_;
    bool isValid_;

    // Secure memory clearing function
    void secureClear() {
        if (!data_.empty()) {
            // Use volatile to prevent compiler optimization
            volatile char* ptr = data_.data();
            for (size_t i = 0; i < data_.size(); ++i) {
                ptr[i] = 0;
            }
            data_.clear();
        }
        isValid_ = false;
    }

public:
    SecureString() : isValid_(false) {}

    explicit SecureString(const String& str) : isValid_(false) {
        if (str.isNotEmpty()) {
            data_.resize(str.length() + 1);
            str.copyToUTF8(data_.data(), data_.size());
            data_[str.length()] = '\0';
            isValid_ = true;
        }
    }

    explicit SecureString(const char* str) : isValid_(false) {
        if (str && strlen(str) > 0) {
            size_t len = strlen(str);
            data_.resize(len + 1);
            memcpy(data_.data(), str, len);
            data_[len] = '\0';
            isValid_ = true;
        }
    }

    ~SecureString() {
        secureClear();
    }

    // Non-copyable to prevent accidental password duplication
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;

    // Movable for efficiency
    SecureString(SecureString&& other) noexcept
        : data_(std::move(other.data_)), isValid_(other.isValid_) {
        other.isValid_ = false;
    }

    SecureString& operator=(SecureString&& other) noexcept {
        if (this != &other) {
            secureClear();
            data_ = std::move(other.data_);
            isValid_ = other.isValid_;
            other.isValid_ = false;
        }
        return *this;
    }

    const char* c_str() const {
        return isValid_ && !data_.empty() ? data_.data() : "";
    }

    bool isEmpty() const {
        return !isValid_ || data_.empty() || data_[0] == '\0';
    }

    bool isNotEmpty() const {
        return !isEmpty();
    }

    size_t length() const {
        return isValid_ && !data_.empty() ? strlen(data_.data()) : 0;
    }

    // Secure comparison to prevent timing attacks
    bool equals(const SecureString& other) const {
        if (!isValid_ || !other.isValid_) {
            return !isValid_ && !other.isValid_;
        }

        size_t len1 = length();
        size_t len2 = other.length();

        // Always compare the same number of bytes to prevent timing attacks
        size_t maxLen = std::max(len1, len2);
        if (maxLen == 0) return true;

        int result = 0;
        for (size_t i = 0; i < maxLen; ++i) {
            char c1 = (i < len1) ? data_[i] : 0;
            char c2 = (i < len2) ? other.data_[i] : 0;
            result |= (c1 ^ c2);
        }

        return result == 0 && len1 == len2;
    }

    // Clear the password from memory
    void clear() {
        secureClear();
    }

    // Create a temporary String for Redis operations (use sparingly)
    String toStringForAuth() const {
        if (!isValid_ || data_.empty()) {
            return String();
        }
        return String(data_.data());
    }
};
#endif

class SourceNode;

/**
 * Redis DataThread for streaming data from Redis server to OpenEphys
 * 
 * This plugin connects to a Redis server and subscribes to channels
 * to receive real-time data streams. Supports JSON and binary data formats.
 */
class TESTABLE RedisDataThread : public DataThread
{
public:
    /** Constructor */
    RedisDataThread(SourceNode* sn);

    /** Destructor */
    ~RedisDataThread();

    /** Creates the custom editor for this DataThread */
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;

    /** Initializes data transfer */
    bool startAcquisition() override;

    /** Stops data transfer */
    bool stopAcquisition() override;

    /** Returns true if the data source is connected and ready */
    bool foundInputSource() override;

    /** Returns true if the data source is connected and ready */
    bool isReady() override;

    /** Updates buffer with new data from Redis */
    bool updateBuffer() override;

    /** Called when the chain updates */
    void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                       OwnedArray<EventChannel>* eventChannels,
                       OwnedArray<SpikeChannel>* spikeChannels,
                       OwnedArray<DataStream>* sourceStreams,
                       OwnedArray<DeviceInfo>* devices,
                       OwnedArray<ConfigurationObject>* configurationObjects) override;

    /** Called when the chain updates, to resize the sourceBuffers */
    void resizeBuffers() override;

    /** Redis connection management */
    bool connectToRedis(const String& host, int port, const String& password = "");
    bool reconnectToRedis(); // Reconnect using stored credentials
    void disconnectFromRedis();
    bool isConnected() const;

    /** Data retrieval methods */
    Array<String> getLatestRecords(int numRecords = 10);

    /** Configuration setters */
    void setRedisHost(const String& host);
    void setRedisPort(int port);
    void setRedisPassword(const String& password);
    void setRedisChannel(const String& channel);
    void setSampleRate(float sampleRate);
    void setNumChannels(int numChannels);
    void setDataFormat(const String& format);

    // Secure password operations
    bool hasPassword() const;
    void clearPassword();

    /** Stream support methods */
    void setStreamMode(bool useStreams);
    void setStreamPattern(const String& pattern);
    Array<String> discoverStreams(const String& pattern = "*");
    bool subscribeToStream(const String& streamName);
    void unsubscribeFromStream(const String& streamName);

    /** Configuration getters */
    String getRedisHost() const { return redisHost; }
    int getRedisPort() const { return redisPort; }
    String getRedisPassword() const { return redisPassword.toStringForAuth(); }
    String getRedisChannel() const { return redisChannel; }
    float getSampleRate() const { return sampleRate; }
    int getNumChannels() const { return numChannels; }
    String getDataFormat() const { return dataFormat; }
    String getConnectionStatus() const;

    /** Stream getters */
    bool getStreamMode() const { return useStreamMode; }
    String getStreamPattern() const { return streamPattern; }
    Array<String> getActiveStreams() const;

private:
#ifdef REDIS_ENABLED
    RedisContextRAII redisCtx;
#else
    void* redisCtx; // placeholder when Redis is not available
#endif

    // Connection parameters
    String redisHost;
    int redisPort;
    SecureString redisPassword; // Use secure string for password storage
    String redisChannel;

    // Data configuration
    float sampleRate;
    int numChannels;
    String dataFormat; // "json", "binary", "brandbci"

    // Stream configuration
    bool useStreamMode;
    String streamPattern;
    String currentStreamId; // Default stream ID for new streams
    Array<String> activeStreams;
    std::map<String, String> streamPositions; // Per-stream position tracking

    // State management
    std::atomic<bool> isAcquiring;
    std::atomic<bool> connectionStatus;
    mutable std::mutex connectionMutex; // For thread-safe connection management
    mutable std::mutex configMutex; // For thread-safe configuration changes
    mutable std::mutex streamMutex; // For thread-safe stream management

    // Sample counting
    std::atomic<int64> currentSampleNumber;

    // Enhanced performance monitoring
    struct PerformanceMetrics {
        std::atomic<float> avgLatency{0.0f};
        std::atomic<float> maxLatency{0.0f};
        std::atomic<float> throughput{0.0f};
        std::atomic<int> droppedSamples{0};
        std::atomic<int64> totalSamplesProcessed{0};

        // Connection health metrics
        std::atomic<float> connectionLatency{0.0f};
        std::atomic<int> consecutiveFailures{0};
        std::atomic<int64> lastSuccessfulOperation{0};
        std::atomic<float> connectionStability{1.0f}; // 0.0 to 1.0
    };

    PerformanceMetrics perfMetrics;

    // Connection health monitoring
    struct HealthCheckConfig {
        bool enableHealthChecks = true;
        int healthCheckIntervalMs = 5000;  // 5 seconds
        int healthCheckTimeoutMs = 1000;   // 1 second
        int maxConsecutiveFailures = 3;
        bool enableProactiveReconnect = true;
    };

    HealthCheckConfig healthConfig;
    std::atomic<int64> lastHealthCheck{0};
    std::atomic<bool> healthCheckInProgress{false};

    // High-performance buffer management
    struct DataPacket {
        std::vector<float> channelData;
        int64 sampleNumber;
        double timestamp;
        uint64 eventCode;
        bool inUse;

        DataPacket() : sampleNumber(0), timestamp(0.0), eventCode(0), inUse(false) {}

        void reset() {
            channelData.clear();
            sampleNumber = 0;
            timestamp = 0.0;
            eventCode = 0;
            inUse = false;
        }
    };

    // Pre-allocated buffer pool
    static constexpr size_t BUFFER_POOL_SIZE = 32;

    // Performance optimization constants for smooth LFP display
    // These values are tuned to match File Reader performance (50Hz LFP refresh rate)
    static constexpr int REDIS_BLOCK_TIMEOUT_MS = 50;        // Redis blocking timeout (was 1000ms)
    static constexpr int PROCESSING_THREAD_TIMEOUT_MS = 20;  // Processing thread wait timeout (was 100ms)
    static constexpr int OPERATION_TIMEOUT_MS = 100;        // Socket operation timeout (was 1000ms)
    static constexpr int OPTIMIZED_BUFFER_SIZE = 5000;      // Buffer size for real-time display (was 10000)
    std::array<DataPacket, BUFFER_POOL_SIZE> bufferPool;
    std::atomic<size_t> poolIndex{0};
    std::mutex poolMutex;

    // Pre-allocated working buffers
    std::vector<float> workingBuffer;
    std::vector<int64> sampleNumberBuffer;
    std::vector<double> timestampBuffer;
    std::vector<uint64> eventCodeBuffer;

    // Optimized string buffer for reducing allocations
    mutable std::string stringBuffer; // Reusable string buffer for temporary operations

    // High-performance multi-threading
    struct RawDataPacket {
        std::vector<char> rawData;
        size_t dataLength;
        std::string dataFormat;
        std::chrono::steady_clock::time_point receiveTime;
        bool inUse;

        RawDataPacket() : dataLength(0), inUse(false) {
            rawData.reserve(8192); // Pre-allocate 8KB
        }

        void reset() {
            rawData.clear();
            dataLength = 0;
            dataFormat.clear();
            inUse = false;
        }
    };

    // Thread-safe queue for raw data
    static constexpr size_t RAW_QUEUE_SIZE = 64;
    std::array<RawDataPacket, RAW_QUEUE_SIZE> rawDataQueue;
    std::atomic<size_t> rawQueueWriteIndex{0};
    std::atomic<size_t> rawQueueReadIndex{0};
    std::atomic<size_t> rawQueueCount{0};

    // Data processing thread
    std::unique_ptr<std::thread> dataProcessingThread;
    std::atomic<bool> shouldProcessData{false};
    std::condition_variable dataAvailableCondition;
    std::mutex dataQueueMutex;

public:
    // Enhanced error handling
    enum class ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CONNECTION_FAILED,
        RECONNECTING,
        AUTHENTICATION_FAILED,
        TIMEOUT
    };

private:

    struct ErrorInfo {
        String errorMessage;
        String errorCode;
        Time timestamp;
        int severity; // 1=Info, 2=Warning, 3=Error, 4=Critical

        ErrorInfo() = default;
        ErrorInfo(const String& msg, const String& code, int sev = 3)
            : errorMessage(msg), errorCode(code), timestamp(Time::getCurrentTime()), severity(sev) {}
    };

    std::atomic<ConnectionState> currentConnectionState{ConnectionState::DISCONNECTED};
    std::vector<ErrorInfo> errorHistory;
    mutable std::mutex errorHistoryMutex;

    // Connection retry mechanism
    struct RetryConfig {
        int maxRetries = 5;
        int retryDelayMs = 1000;
        int maxRetryDelayMs = 30000;
        float backoffMultiplier = 2.0f;
        bool enableAutoReconnect = true;
    };

    RetryConfig retryConfig;
    std::atomic<int> currentRetryCount{0};
    std::atomic<int64> lastConnectionAttempt{0};
    std::atomic<int64> nextRetryTime{0};



public:
    // Enhanced error handling methods
    void addError(const String& message, const String& code = "", int severity = 3);
    void clearErrors();
    std::vector<ErrorInfo> getRecentErrors(int maxCount = 10) const;
    String getLastErrorMessage() const;
    ConnectionState getConnectionState() const { return currentConnectionState.load(); }
    String getConnectionStateString() const;

    // Connection management with retry
    bool connectToRedisWithRetry();
    bool attemptReconnection();
    void resetRetryState();
    bool shouldAttemptReconnect() const;
    void updateConnectionState(ConnectionState newState);
    void scheduleReconnection();

    // Enhanced Redis operations with error handling
    bool executeRedisCommand(const String& command, String& result, String& errorMsg);
    bool testRedisConnection();
    void handleRedisError(const String& operation, const String& details = "");

    // Configuration validation
    bool validateConfiguration(String& errorMessage) const;
    bool isValidHost(const String& host) const;
    bool isValidPort(int port) const;
    bool isValidChannelName(const String& channel) const;

    // Connection health monitoring
    bool performConnectionHealthCheck();
    bool shouldPerformHealthCheck() const;
    void updateConnectionMetrics();
    void recordOperationLatency(float latencyMs);
    void recordOperationSuccess();
    void recordOperationFailure();
    float getConnectionStability() const { return perfMetrics.connectionStability.load(); }
    int getConsecutiveFailures() const { return perfMetrics.consecutiveFailures.load(); }

    // Performance metrics getters
    float getAverageLatency() const { return perfMetrics.avgLatency.load(); }
    float getMaxLatency() const { return perfMetrics.maxLatency.load(); }
    float getThroughput() const { return perfMetrics.throughput.load(); }
    int64 getTotalSamplesProcessed() const { return perfMetrics.totalSamplesProcessed.load(); }

    // High-performance buffer management
    DataPacket* acquireDataPacket();
    void releaseDataPacket(DataPacket* packet);
    void initializeBufferPool();
    bool parseJsonDataOptimized(const char* jsonStr, size_t length, DataPacket* packet);
    bool processDataPacketToBuffer(DataPacket* packet);

    // High-performance multi-threading
    void startDataProcessingThread();
    void stopDataProcessingThread();
    void dataProcessingLoop();
    bool enqueueRawData(const char* data, size_t length, const std::string& format);
    bool dequeueRawData(RawDataPacket*& packet);
    void releaseRawDataPacket(RawDataPacket* packet);

    // Data parsing methods
    bool parseJsonData(const String& jsonStr, Array<float>& channelData);
    bool parseBinaryData(const char* data, size_t length, Array<float>& channelData);
    bool parseBrandBCIData(const String& jsonStr, Array<float>& channelData);

    // Stream methods
    bool updateBufferFromStreams();
    bool updateBufferFromList(); // Legacy BLPOP method
    bool readFromStream(const String& streamName, String& data, String& newId);
#ifdef REDIS_ENABLED
    bool processStreamEntry(redisReply* fieldsReply);
#else
    bool processStreamEntry(void* fieldsReply);
#endif

    // Error handling
    void handleRedisError(const String& operation);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThread);
};

#endif // __REDISDATATHREAD_H_INCLUDED__
