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

    /** Stream support methods */
    void setStreamMode(bool useStreams);
    void setStreamPattern(const String& pattern);
    Array<String> discoverStreams(const String& pattern = "*");
    bool subscribeToStream(const String& streamName);
    void unsubscribeFromStream(const String& streamName);

    /** Configuration getters */
    String getRedisHost() const { return redisHost; }
    int getRedisPort() const { return redisPort; }
    String getRedisPassword() const { return redisPassword; }
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
    redisContext* redisCtx;
#else
    void* redisCtx; // placeholder when Redis is not available
#endif

    // Connection parameters
    String redisHost;
    int redisPort;
    String redisPassword;
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
    std::array<DataPacket, BUFFER_POOL_SIZE> bufferPool;
    std::atomic<size_t> poolIndex{0};
    std::mutex poolMutex;

    // Pre-allocated working buffers
    std::vector<float> workingBuffer;
    std::vector<int64> sampleNumberBuffer;
    std::vector<double> timestampBuffer;
    std::vector<uint64> eventCodeBuffer;

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
    bool processStreamEntry(redisReply* fieldsReply);

    // Error handling
    void handleRedisError(const String& operation);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThread);
};

#endif // __REDISDATATHREAD_H_INCLUDED__
