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

#include "RedisDataThread.h"
#include "RedisDataThreadEditor.h"

RedisDataThread::RedisDataThread(SourceNode* sn)
    : DataThread(sn)
#ifdef REDIS_ENABLED
    , redisCtx()
#else
    , redisCtx(nullptr)
#endif
    , redisHost("localhost")
    , redisPort(6379)
    , redisChannel("neural_data_primary")
    , sampleRate(30000.0f)
    , numChannels(96)
    , dataFormat("brandbci")
    , useStreamMode(true)
    , streamPattern("neural_*")
    , currentStreamId("$")
    , isAcquiring(false)
    , connectionStatus(false)
    , currentSampleNumber(0)
{
    LOGD("🚀 RedisDataThread constructor called");
    LOGD("Default configuration:");
    LOGD("  - Host: ", redisHost, ":", redisPort);
    LOGD("  - Channel: ", redisChannel);
    LOGD("  - Format: ", dataFormat);
    LOGD("  - Channels: ", numChannels);
    LOGD("  - Sample Rate: ", sampleRate);
    LOGD("  - Stream Mode: ", useStreamMode ? "ENABLED" : "DISABLED");
    LOGD("  - Stream Pattern: ", streamPattern);
#ifdef REDIS_ENABLED
    LOGD("  - Redis context: ", (redisCtx.isValid() ? "initialized" : "null"));
#else
    LOGD("  - Redis context: ", (redisCtx ? "initialized" : "null"));
#endif
    LOGD("  - Acquiring: ", isAcquiring.load());
    LOGD("  - Connected: ", connectionStatus.load());

#ifdef REDIS_ENABLED
    LOGD("✓ Redis support is ENABLED");
#else
    LOGE("❌ Redis support is DISABLED - recompile with REDIS_ENABLED=1");
#endif

    LOGD("✅ RedisDataThread initialized");

    // Initialize high-performance buffer pool
    initializeBufferPool();

    // Initialize raw data queue
    for (size_t i = 0; i < RAW_QUEUE_SIZE; ++i) {
        rawDataQueue[i].reset();
    }
}

RedisDataThread::~RedisDataThread()
{
    LOGD("🔄 RedisDataThread destructor called");

    if (isAcquiring.load())
    {
        stopAcquisition();
    }

    disconnectFromRedis();

    // Securely clear password from memory
    redisPassword.clear();

    LOGD("✅ RedisDataThread destroyed");
}

std::unique_ptr<GenericEditor> RedisDataThread::createEditor(SourceNode* sn)
{
    std::unique_ptr<RedisDataThreadEditor> editor = std::make_unique<RedisDataThreadEditor>(sn, this);
    return std::move(editor);
}

bool RedisDataThread::foundInputSource()
{
    LOGD("foundInputSource() called - connection status: ", connectionStatus.load());
    // Return true if already connected, otherwise try to connect
    if (connectionStatus.load()) {
        LOGD("foundInputSource() returning true - already connected");
        return true;
    }

    // Try to connect to Redis to verify the source is available
    bool result = connectToRedis(redisHost, redisPort, redisPassword);
    LOGD("foundInputSource() connectToRedis result: ", result);
    return result;
}

bool RedisDataThread::isReady()
{
    return connectionStatus.load();
}

bool RedisDataThread::connectToRedis(const String& host, int port, const String& password)
{
#ifdef REDIS_ENABLED
    try {
        std::lock_guard<std::mutex> lock(connectionMutex);

        LOGD("Attempting to connect to Redis server: ", host, ":", port);
        updateConnectionState(ConnectionState::CONNECTING);

    // Disconnect existing connection
    if (connectionStatus.load())
    {
        LOGD("Disconnecting existing Redis connection");
        disconnectFromRedis();
    }

    // Create new connection with timeout
    LOGD("Creating Redis connection context");
    struct timeval timeout = { 2, 0 }; // 2 seconds timeout

    // Create new context using RAII wrapper
    RedisContextRAII newCtx(host.toRawUTF8(), port, timeout);

    if (!newCtx.isValid())
    {
        // Optimize: Build error message without String::formatted for simple cases
        String errorMsg = "Redis connection failed: " + String(newCtx.getErrorString()) +
                         " (error code: " + String(newCtx.getErrorCode()) + ")";
        addError(errorMsg, "CONNECT_ERROR", 3);
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        connectionStatus = false;
        return false;
    }

    LOGD("Redis context created successfully");

    // Set socket timeout for operations
    struct timeval tv = { 1, 0 }; // 1 second timeout for operations
    redisSetTimeout(newCtx.get(), tv);

    // Authenticate if password is provided
    if (password.isNotEmpty())
    {
        LOGD("Authenticating with Redis using provided password");
        String result, errorMsg;

        // Use secure password handling for authentication
        SecureString securePassword(password);
        String authCommand = "AUTH " + securePassword.toStringForAuth();
        bool authSuccess = executeRedisCommand(authCommand, result, errorMsg);

        // Clear the temporary auth command from memory
        authCommand = String(); // Clear the command string

        if (!authSuccess)
        {
            addError("Redis authentication failed: " + errorMsg, "AUTH_FAILED", 3);
            updateConnectionState(ConnectionState::AUTHENTICATION_FAILED);
            return false;
        }

        addError("Redis authentication successful", "AUTH_SUCCESS", 1);
    }
    else
    {
        LOGD("No password provided - skipping authentication");
    }

    // Test connection with PING
    LOGD("Testing Redis connection with PING command");
    if (!testRedisConnection())
    {
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        disconnectFromRedis();
        return false;
    }

    // Test channel access based on mode
    LOGD("Testing access to Redis channel: ", redisChannel, " (mode: ", useStreamMode ? "STREAM" : "LIST", ")");

#ifdef REDIS_ENABLED
    if (useStreamMode)
    {
        // For stream mode, check if key exists and is a stream
        RedisReplyRAII testReply((redisReply*)redisCommand(newCtx.get(), "TYPE %s", redisChannel.toRawUTF8()));
        if (testReply.isValid() && testReply->type == REDIS_REPLY_STATUS)
        {
            // Optimize: Use string view to avoid unnecessary String creation
            StringUtils::StringView keyType(testReply->str, testReply->len);
            if (keyType.equals("stream"))
            {
                LOGD("Redis stream '", redisChannel, "' found and accessible");
            }
            else if (keyType.equals("none"))
            {
                LOGD("Redis stream '", redisChannel, "' does not exist yet (will be created when data arrives)");
            }
            else
            {
                LOGE("Redis key '", redisChannel, "' exists but is not a stream (type: ", keyType.toString(), ")");
                return false;
            }
        }
        else
        {
            LOGE("Cannot check Redis key type for '", redisChannel, "'");
            return false;
        }
    }
    else
    {
        // For list mode, use LLEN
        RedisReplyRAII testReply((redisReply*)redisCommand(newCtx.get(), "LLEN %s", redisChannel.toRawUTF8()));
        if (!testReply.isValid() || testReply->type == REDIS_REPLY_ERROR)
        {
            LOGE("Cannot access Redis channel '", redisChannel, "': ",
                 (testReply.isValid() && testReply->str) ? testReply->str : "unknown error");
            return false;
        }
        else
        {
            int queueLength = (testReply->type == REDIS_REPLY_INTEGER) ? testReply->integer : -1;
            LOGD("Redis channel '", redisChannel, "' accessible, current length: ", queueLength);
        }
    }

    // Connection successful - move the context to our member variable
    redisCtx = std::move(newCtx);
#endif

    // Save connection parameters
    redisHost = host;
    redisPort = port;
    // Store password securely
    redisPassword.clear();
    if (password.isNotEmpty()) {
        redisPassword = SecureString(password);
    }
    connectionStatus = true;

        LOGD("✓ Successfully connected to Redis server: ", host, ":", port);
        LOGD("✓ Redis connection status set to TRUE");
        LOGD("✓ Channel: ", redisChannel, ", Format: ", dataFormat, ", Channels: ", numChannels, ", Sample Rate: ", sampleRate);
        return true;

    } catch (const std::bad_alloc& e) {
        addError("Memory allocation failed during Redis connection: " + String(e.what()), "CONNECT_MEMORY_ERROR", 4);
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        connectionStatus.store(false);
        return false;
    } catch (const std::runtime_error& e) {
        addError("Runtime error during Redis connection: " + String(e.what()), "CONNECT_RUNTIME_ERROR", 3);
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        connectionStatus.store(false);
        return false;
    } catch (const std::exception& e) {
        addError("Exception during Redis connection: " + String(e.what()), "CONNECT_EXCEPTION", 3);
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        connectionStatus.store(false);
        return false;
    } catch (...) {
        addError("Unknown exception during Redis connection", "CONNECT_UNKNOWN_EXCEPTION", 3);
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        connectionStatus.store(false);
        return false;
    }
#else
    LOGE("Redis support not compiled. Install hiredis library and recompile.");
    LOGE("To enable Redis: install libhiredis-dev and recompile with REDIS_ENABLED=1");
    return false;
#endif
}

void RedisDataThread::disconnectFromRedis()
{
#ifdef REDIS_ENABLED
    std::lock_guard<std::mutex> lock(connectionMutex);
    redisCtx.reset(); // RAII wrapper automatically handles cleanup
#endif
    connectionStatus.store(false);
}

bool RedisDataThread::isConnected() const
{
#ifdef REDIS_ENABLED
    std::lock_guard<std::mutex> lock(connectionMutex);
    return connectionStatus.load() && redisCtx.isValid();
#else
    return connectionStatus.load();
#endif
}

void RedisDataThread::setRedisHost(const String& host)
{
    std::lock_guard<std::mutex> lock(configMutex);
    redisHost = host;
}

void RedisDataThread::setRedisPort(int port)
{
    std::lock_guard<std::mutex> lock(configMutex);
    redisPort = port;
}

void RedisDataThread::setRedisPassword(const String& password)
{
    std::lock_guard<std::mutex> lock(configMutex);
    // Clear old password and set new one securely
    redisPassword.clear();
    if (password.isNotEmpty()) {
        redisPassword = SecureString(password);
    }
}

void RedisDataThread::setRedisChannel(const String& channel)
{
    std::lock_guard<std::mutex> lock(configMutex);
    redisChannel = channel;
}

void RedisDataThread::setSampleRate(float rate)
{
    std::lock_guard<std::mutex> lock(configMutex);
    sampleRate = rate;
}

void RedisDataThread::setNumChannels(int channels)
{
    std::lock_guard<std::mutex> lock(configMutex);
    numChannels = channels;
}

void RedisDataThread::setDataFormat(const String& format)
{
    std::lock_guard<std::mutex> lock(configMutex);
    dataFormat = format;

    // Auto-enable stream mode for BRANDBCI format
    if (format == "brandbci")
    {
        useStreamMode = true;
        LOGD("Auto-enabled stream mode for BRANDBCI format");
    }
}

bool RedisDataThread::hasPassword() const
{
    std::lock_guard<std::mutex> lock(configMutex);
    return redisPassword.isNotEmpty();
}

void RedisDataThread::clearPassword()
{
    std::lock_guard<std::mutex> lock(configMutex);
    redisPassword.clear();
    LOGD("Redis password cleared from memory");
}

bool RedisDataThread::reconnectToRedis()
{
    std::lock_guard<std::mutex> lock(configMutex);

    LOGD("Attempting to reconnect to Redis using stored credentials");

    // Use stored credentials for reconnection
    String password;
    if (redisPassword.isNotEmpty()) {
        // Temporarily create String for connection - will be cleared after use
        password = redisPassword.toStringForAuth();
    }

    bool success = connectToRedis(redisHost, redisPort, password);

    // Clear the temporary password string
    password = String();

    if (success) {
        LOGD("Reconnection to Redis successful");
    } else {
        LOGE("Reconnection to Redis failed");
    }

    return success;
}

String RedisDataThread::getConnectionStatus() const
{
    std::lock_guard<std::mutex> configLock(configMutex);
    if (connectionStatus.load())
    {
        return "Connected to " + redisHost + ":" + String(redisPort);
    }
    else
    {
        return "Disconnected";
    }
}

Array<String> RedisDataThread::getLatestRecords(int numRecords)
{
    Array<String> records;

#ifdef REDIS_ENABLED
    if (!connectionStatus.load() || !redisCtx.isValid())
    {
        LOGE("Cannot retrieve data: Redis not connected");
        return records;
    }

    LOGD("Retrieving latest ", numRecords, " records from Redis channel: ", redisChannel, " (mode: ", useStreamMode ? "STREAM" : "LIST", ")");

    RedisReplyRAII reply(nullptr);

    if (useStreamMode)
    {
        // For streams, use XREVRANGE to get latest entries
        reply = RedisReplyRAII((redisReply*)redisCommand(redisCtx.get(),
            "XREVRANGE %s + - COUNT %d", redisChannel.toRawUTF8(), numRecords));
    }
    else
    {
        // For lists, use LRANGE to get the latest records without removing them from the list
        // LRANGE key start stop - gets elements from start to stop (inclusive)
        // Use negative indices to get from the end: -1 is last element, -2 is second to last, etc.
        int startIndex = -(numRecords);
        int stopIndex = -1;

        reply = RedisReplyRAII((redisReply*)redisCommand(redisCtx.get(),
            "LRANGE %s %d %d", redisChannel.toRawUTF8(), startIndex, stopIndex));
    }

    if (!reply.isValid())
    {
        LOGE("Redis command returned null reply");
        handleRedisError("Redis command failed", "");
        return records;
    }

    LOGD("Redis reply type: ", reply->type, ", elements: ",
         (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_ARRAY)
    {
        if (useStreamMode)
        {
            // Parse stream entries: each entry is [id, [field1, value1, field2, value2, ...]]
            LOGD("Retrieved ", reply->elements, " stream entries from Redis");

            for (size_t i = 0; i < reply->elements; i++)
            {
                redisReply* entryReply = reply->element[i];
                if (entryReply->type == REDIS_REPLY_ARRAY && entryReply->elements >= 2)
                {
                    // Get the field-value pairs
                    redisReply* fieldsReply = entryReply->element[1];
                    if (fieldsReply->type == REDIS_REPLY_ARRAY)
                    {
                        // Look for brandbci_data field
                        for (size_t j = 0; j < fieldsReply->elements; j += 2)
                        {
                            if (j + 1 < fieldsReply->elements &&
                                fieldsReply->element[j]->type == REDIS_REPLY_STRING &&
                                StringUtils::StringView(fieldsReply->element[j]->str, fieldsReply->element[j]->len).equals("brandbci_data"))
                            {
                                // Optimize: Create String only once for the record
                                String record = StringUtils::createString(fieldsReply->element[j + 1]->str, fieldsReply->element[j + 1]->len);
                                records.add(std::move(record)); // Use move to avoid copy
                                LOGD("Stream record ", i, " length: ", fieldsReply->element[j + 1]->len, " bytes");
                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            // Parse list entries: each element is a string
            LOGD("Retrieved ", reply->elements, " list records from Redis");

            for (size_t i = 0; i < reply->elements; i++)
            {
                if (reply->element[i]->type == REDIS_REPLY_STRING)
                {
                    // Optimize: Use move semantics to avoid copy
                    String record = StringUtils::createString(reply->element[i]->str, reply->element[i]->len);
                    records.add(std::move(record));
                    LOGD("List record ", i, " length: ", reply->element[i]->len, " bytes");
                }
            }
        }
    }
    else if (reply->type == REDIS_REPLY_NIL)
    {
        LOGD("No data available in Redis channel");
    }
    else
    {
        LOGE("Unexpected Redis reply type: ", reply->type);
    }

#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
#endif

    LOGD("Returning ", records.size(), " records");
    return records;
}

bool RedisDataThread::startAcquisition()
{
    LOGD("=== STARTING REDIS ACQUISITION ===");
    LOGD("Connection status: ", connectionStatus.load());
    LOGD("Current acquiring status: ", isAcquiring.load());
    LOGD("Redis host: ", redisHost, ":", redisPort);
    LOGD("Redis channel: ", redisChannel);
    LOGD("Data format: ", dataFormat);
    LOGD("Expected channels: ", numChannels);
    LOGD("Sample rate: ", sampleRate);

    if (!isConnected())
    {
        LOGE("❌ Cannot start acquisition: not connected to Redis");
        LOGE("Connection status: ", connectionStatus.load());
        return false;
    }

    if (isAcquiring.load())
    {
        LOGC("⚠️ Acquisition already running");
        return true;
    }

    // Reset sample counter
    currentSampleNumber = 0;
    LOGD("Reset sample counter to 0");

    // Test Redis connection before starting
    LOGD("Testing Redis connection before starting acquisition...");
#ifdef REDIS_ENABLED
    if (redisCtx.isValid())
    {
        RedisReplyRAII testReply((redisReply*)redisCommand(redisCtx.get(), "PING"));
        if (testReply.isValid() && testReply->type != REDIS_REPLY_ERROR)
        {
            LOGD("✓ Redis PING test successful");
        }
        else
        {
            LOGE("❌ Redis PING test failed");
            return false;
        }

        // Check data availability based on mode
        if (useStreamMode)
        {
            // For streams, check if stream exists and has data
            RedisReplyRAII lenReply((redisReply*)redisCommand(redisCtx.get(), "XLEN %s", redisChannel.toRawUTF8()));
            if (lenReply.isValid() && lenReply->type == REDIS_REPLY_INTEGER)
            {
                LOGD("✓ Redis stream '", redisChannel, "' has ", lenReply->integer, " entries available");
            }
            else if (lenReply.isValid() && lenReply->type == REDIS_REPLY_ERROR)
            {
                LOGD("✓ Redis stream '", redisChannel, "' does not exist yet (will be created when data arrives)");
            }
            else
            {
                LOGE("❌ Cannot check Redis stream length");
            }
        }
        else
        {
            // For lists, use LLEN
            RedisReplyRAII lenReply((redisReply*)redisCommand(redisCtx.get(), "LLEN %s", redisChannel.toRawUTF8()));
            if (lenReply.isValid() && lenReply->type == REDIS_REPLY_INTEGER)
            {
                LOGD("✓ Redis channel '", redisChannel, "' has ", lenReply->integer, " samples available");
            }
            else
            {
                LOGE("❌ Cannot check Redis channel length");
            }
        }
    }
    else
    {
        LOGE("❌ Redis context is null");
        return false;
    }
#endif

    // Start the high-performance data processing thread
    startDataProcessingThread();

    isAcquiring = true;

    // Start the data thread - this is critical!
    LOGD("🚀 Starting data thread...");
    if (!isThreadRunning()) {
        startThread();
        LOGD("✅ Data thread started successfully");
    } else {
        LOGD("⚠️ Data thread was already running");
    }

    LOGD("✅ Redis data acquisition started successfully with multi-threading");
    LOGD("=== ACQUISITION ACTIVE ===");
    return true;
}

bool RedisDataThread::stopAcquisition()
{
    LOGD("=== STOPPING REDIS ACQUISITION ===");
    LOGD("Current acquiring status: ", isAcquiring.load());

    if (!isAcquiring.load())
    {
        LOGD("Acquisition already stopped");
        return true;
    }

    isAcquiring = false;

    // Stop the high-performance data processing thread
    stopDataProcessingThread();

    // Stop the data thread
    LOGD("🛑 Stopping data thread...");
    if (isThreadRunning()) {
        signalThreadShouldExit();
        waitForThreadToExit(5000); // Wait up to 5 seconds
        LOGD("✅ Data thread stopped successfully");
    } else {
        LOGD("⚠️ Data thread was not running");
    }

    LOGD("✅ Redis data acquisition stopped with multi-threading cleanup");
    LOGD("Final sample count: ", currentSampleNumber.load());
    LOGD("=== ACQUISITION STOPPED ===");
    return true;
}

void RedisDataThread::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                                     OwnedArray<EventChannel>* eventChannels,
                                     OwnedArray<SpikeChannel>* spikeChannels,
                                     OwnedArray<DataStream>* sourceStreams,
                                     OwnedArray<DeviceInfo>* devices,
                                     OwnedArray<ConfigurationObject>* configurationObjects)
{
    // Clear existing configuration
    sourceStreams->clear();
    continuousChannels->clear();

    // Create data stream
    DataStream* stream = new DataStream(DataStream::Settings{
        "Redis Stream",
        "Redis data source",
        "redis.stream.1",
        sampleRate,
        false // generates_timestamps
    });
    sourceStreams->add(stream);

    // Create continuous channels
    for (int ch = 0; ch < numChannels; ch++)
    {
        ContinuousChannel* chan = new ContinuousChannel(ContinuousChannel::Settings{
            ContinuousChannel::Type::ELECTRODE,
            "Redis_CH" + String(ch + 1),
            "Redis channel " + String(ch + 1),
            "redis.channel." + String(ch + 1),
            0.195f, // microvolts conversion factor
            stream
        });
        continuousChannels->add(chan);
    }
}

void RedisDataThread::resizeBuffers()
{
    // Clear existing buffers
    sourceBuffers.clear();

    // Create one DataBuffer for our single data stream
    // Buffer size: 10000 samples should be sufficient for Redis data
    const int bufferSize = 10000;

    DataBuffer* buffer = new DataBuffer(numChannels, bufferSize);
    sourceBuffers.add(buffer);

    LOGD("Redis DataThread buffers resized: ", numChannels, " channels, buffer size: ", bufferSize);
}

bool RedisDataThread::updateBuffer()
{
    try {
        static int64 lastLogTime = 0;
        static int64 samplesProcessed = 0;
        static int64 lastSampleCount = 0;
        static int64 callCount = 0;

        callCount++;

        // Log every call for the first 10 calls, then every 100 calls
        if (callCount <= 10 || callCount % 100 == 0) {
            LOGD("🔄 updateBuffer() called #", callCount, " - acquiring: ", isAcquiring.load(), ", connected: ", connectionStatus.load());
        }

    // Check acquisition state
    if (!isAcquiring.load())
    {
        if (callCount <= 5) LOGD("updateBuffer: Not acquiring, returning false");
        return false;
    }

    if (!connectionStatus.load())
    {
        if (callCount <= 5) LOGE("updateBuffer: Redis not connected");

        // Attempt automatic reconnection if enabled
        if (shouldAttemptReconnect())
        {
            LOGD("Attempting automatic reconnection...");
            if (attemptReconnection())
            {
                addError("Automatic reconnection successful", "AUTO_RECONNECT_SUCCESS", 1);
            }
            else
            {
                addError("Automatic reconnection failed", "AUTO_RECONNECT_FAILED", 3);
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    // Perform periodic health checks on connected state
    if (shouldPerformHealthCheck())
    {
        LOGD("Performing connection health check...");
        if (!performConnectionHealthCheck())
        {
            LOGD("Health check failed, scheduling reconnection");
            if (retryConfig.enableAutoReconnect)
            {
                updateConnectionState(ConnectionState::CONNECTION_FAILED);
                scheduleReconnection();
                return false;
            }
        }
        else
        {
            LOGD("Health check passed");
        }
    }

#ifdef REDIS_ENABLED
    // Periodic status logging (every 5 seconds)
    int64 currentTime = Time::getMillisecondCounterHiRes();
    if (currentTime - lastLogTime > 5000) // 5 seconds
    {
        int64 samplesDelta = samplesProcessed - lastSampleCount;
        double rate = samplesDelta / 5.0; // samples per second
        LOGD("Redis Status: processed=", samplesProcessed, " samples, rate=", rate, " samples/sec, acquiring=", isAcquiring.load(), ", connected=", connectionStatus.load());
        LOGD("  Mode: ", useStreamMode ? "STREAMS" : "LISTS", ", Active streams: ", activeStreams.size());
        lastLogTime = currentTime;
        lastSampleCount = samplesProcessed;
    }

    // Route to appropriate update method based on mode with performance tracking
    LOGD("updateBuffer: Using ", useStreamMode ? "STREAM" : "LIST", " mode");

    auto operationStart = std::chrono::high_resolution_clock::now();
    bool result = useStreamMode ? updateBufferFromStreams() : updateBufferFromList();
    auto operationEnd = std::chrono::high_resolution_clock::now();

    // Record operation performance
    float operationLatency = std::chrono::duration<float, std::milli>(operationEnd - operationStart).count();
    recordOperationLatency(operationLatency);

        if (result) {
            samplesProcessed++;
            recordOperationSuccess();
            perfMetrics.totalSamplesProcessed.fetch_add(1);
        } else {
            recordOperationFailure();
        }

        return result;

    } catch (const std::bad_alloc& e) {
        addError("Memory allocation failed in updateBuffer: " + String(e.what()), "UPDATE_BUFFER_MEMORY_ERROR", 4);
        recordOperationFailure();
        return false;
    } catch (const std::runtime_error& e) {
        addError("Runtime error in updateBuffer: " + String(e.what()), "UPDATE_BUFFER_RUNTIME_ERROR", 3);
        recordOperationFailure();
        return false;
    } catch (const std::exception& e) {
        addError("Exception in updateBuffer: " + String(e.what()), "UPDATE_BUFFER_EXCEPTION", 3);
        recordOperationFailure();
        return false;
    } catch (...) {
        addError("Unknown exception in updateBuffer", "UPDATE_BUFFER_UNKNOWN_EXCEPTION", 3);
        recordOperationFailure();
        return false;
    }
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::updateBufferFromList()
{
#ifdef REDIS_ENABLED
    // Log BLPOP command execution
    LOGD("Executing BLPOP command on channel: ", redisChannel);

    // Get data from Redis (blocking call with 1 second timeout)
    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(),
        "BLPOP %s 1", redisChannel.toRawUTF8()));

    if (!reply.isValid())
    {
        LOGE("BLPOP command returned null reply");
        handleRedisError("BLPOP command failed", "");
        return false;
    }

    LOGD("BLPOP reply type: ", reply->type, ", elements: ", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        LOGD("BLPOP timeout - no data available in Redis channel");
        return true; // Not an error
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2)
    {
        LOGD("BLPOP returned array with 2 elements - processing data");

        // Use high-performance multi-threaded pipeline
        LOGD("Data format: ", dataFormat, ", data length: ", reply->element[1]->len, " bytes");

        // Enqueue raw data for processing in dedicated thread
        success = enqueueRawData(reply->element[1]->str, reply->element[1]->len, dataFormat.toStdString());

        if (success) {
            LOGD("Successfully enqueued data for multi-threaded processing");
        } else {
            LOGE("Failed to enqueue data - queue may be full");
        }
        // Data processing is now handled by the dedicated thread
        // The success variable indicates whether the data was successfully enqueued
    }
    else
    {
        LOGE("Unexpected BLPOP reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
    }
    LOGD("updateBufferFromList returning: ", success);
    return success;
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::updateBufferFromStreams()
{
#ifdef REDIS_ENABLED
    // Check active streams with thread safety
    {
        std::lock_guard<std::mutex> lock(streamMutex);
        if (activeStreams.size() == 0)
        {
            LOGD("No active streams - discovering streams with pattern: ", streamPattern);
            Array<String> discoveredStreams = discoverStreams(streamPattern);
            LOGD("Stream discovery found ", discoveredStreams.size(), " streams");

            for (int i = 0; i < discoveredStreams.size(); i++)
            {
                LOGD("Subscribing to discovered stream: ", discoveredStreams[i]);
                subscribeToStream(discoveredStreams[i]);
            }

            if (activeStreams.size() == 0)
            {
                LOGD("No streams found matching pattern: ", streamPattern);
                LOGD("This could mean:");
                LOGD("  1. No streams exist in Redis with pattern: ", streamPattern);
                LOGD("  2. Redis connection issue");
                LOGD("  3. Stream discovery function failed");
                return true; // Not an error, just no data available
            }
            else
            {
                LOGD("Successfully subscribed to ", activeStreams.size(), " streams");
            }
        }
    }

    // Read from all active streams
    String streamNames = "";
    String streamIds = "";
    String xreadCommand;

    // Build XREAD command with thread safety
    {
        std::lock_guard<std::mutex> lock(streamMutex);

        // Validate that we have active streams
        if (activeStreams.size() == 0)
        {
            LOGD("No active streams available for XREAD");
            return true; // Not an error, just no data available
        }

        // Validate currentStreamId format
        if (currentStreamId.isEmpty() ||
            (!currentStreamId.contains("-") && currentStreamId != "0" && currentStreamId != "$"))
        {
            LOGE("Invalid currentStreamId format: '", currentStreamId, "'. Resetting to '$'");
            currentStreamId = "$";
        }

        // Build XREAD command with proper syntax
        xreadCommand = "XREAD BLOCK 1000 STREAMS";

        // Add stream names
        for (int i = 0; i < activeStreams.size(); i++)
        {
            xreadCommand += " ";
            xreadCommand += activeStreams[i];
            if (i > 0) {
                streamNames += " ";
            }
            streamNames += activeStreams[i];
        }

        // Add stream IDs (each stream has its own position)
        for (int i = 0; i < activeStreams.size(); i++)
        {
            String streamName = activeStreams[i];
            String streamId = currentStreamId; // Default for new streams

            // Check if we have a specific position for this stream
            if (streamPositions.find(streamName) != streamPositions.end())
            {
                streamId = streamPositions[streamName];
            }
            else
            {
                // Initialize new stream position
                streamPositions[streamName] = currentStreamId;
                LOGD("Initialized stream position for '", streamName, "' to '", currentStreamId, "'");
            }

            xreadCommand += " ";
            xreadCommand += streamId;
            if (i > 0) {
                streamIds += " ";
            }
            streamIds += streamId;
        }

        LOGD("Reading from streams: ", streamNames, " with IDs: ", streamIds);
        LOGD("Active streams count: ", activeStreams.size(), ", currentStreamId: ", currentStreamId);
        LOGD("Full XREAD command: ", xreadCommand);
    } // End of stream mutex lock

    // Use XREAD to read from multiple streams
    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(), xreadCommand.toRawUTF8()));

    if (!reply.isValid())
    {
        LOGE("XREAD command returned null reply");
        handleRedisError("XREAD command failed", "");
        return false;
    }

    LOGD("XREAD reply type: ", reply->type, ", elements: ", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        LOGD("XREAD timeout - no data available in streams");
        return true; // Not an error
    }
    else if (reply->type == REDIS_REPLY_ERROR)
    {
        // Redis returned an error - log the specific error message
        String errorMsg(reply->str, reply->len);
        LOGE("XREAD command failed with Redis error: ", errorMsg);
        LOGE("Command was: XREAD BLOCK 1000 STREAMS ", streamNames, " ", streamIds);

        // Check for common error conditions
        if (errorMsg.contains("WRONGTYPE"))
        {
            LOGE("Error: One of the specified keys is not a stream. Check that the stream names are correct.");
        }
        else if (errorMsg.contains("syntax"))
        {
            LOGE("Error: XREAD command syntax error. Check stream names and IDs format.");
        }

        return false;
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
    {
        LOGD("XREAD returned array with ", reply->elements, " stream(s) - processing data");

        // Process each stream that has data
        for (size_t streamIdx = 0; streamIdx < reply->elements; streamIdx++)
        {
            redisReply* streamReply = reply->element[streamIdx];
            if (streamReply->type == REDIS_REPLY_ARRAY && streamReply->elements == 2)
            {
                // Get stream name and entries - optimize string creation
                StringUtils::StringView streamNameView(streamReply->element[0]->str, streamReply->element[0]->len);
                String streamName = streamNameView.toString(); // Only create String when needed for map operations
                redisReply* entriesReply = streamReply->element[1];

                LOGD("Processing stream: ", streamName, " with ", entriesReply->elements, " entries");

                // Process each entry in the stream
                for (size_t entryIdx = 0; entryIdx < entriesReply->elements; entryIdx++)
                {
                    redisReply* entryReply = entriesReply->element[entryIdx];
                    if (entryReply->type == REDIS_REPLY_ARRAY && entryReply->elements >= 2)
                    {
                        // Update stream position for this specific stream
                        String entryId = String(entryReply->element[0]->str);
                        {
                            std::lock_guard<std::mutex> lock(streamMutex);
                            streamPositions[streamName] = entryId;
                        }
                        LOGD("Updated stream position for '", streamName, "' to '", entryId, "'");

                        // Get field-value pairs
                        redisReply* fieldsReply = entryReply->element[1];
                        if (fieldsReply->type == REDIS_REPLY_ARRAY)
                        {
                            success = processStreamEntry(fieldsReply);
                            if (success) break; // Process one entry at a time
                        }
                    }
                }

                if (success) break; // Process one stream at a time
            }
        }
    }
    else if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 0)
    {
        // Empty array response - no data available but not an error
        LOGD("XREAD returned empty array - no data available in streams");
        return true;
    }
    else
    {
        LOGE("Unexpected XREAD reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
        if (reply->type == REDIS_REPLY_ERROR && reply->str)
        {
            LOGE("Redis error message: ", String(reply->str, reply->len));
        }
    }
    LOGD("updateBufferFromStreams returning: ", success);
    return success;
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::parseJsonData(const String& jsonStr, Array<float>& channelData)
{
    LOGD("Parsing JSON data, length: ", jsonStr.length(), " characters");

    try
    {
        var jsonData = JSON::parse(jsonStr);
        LOGD("JSON parsing successful");

        if (!jsonData.isObject())
        {
            addError("Invalid JSON format: not an object", "JSON_NOT_OBJECT", 3);
            return false;
        }
        LOGD("JSON is valid object");

        // Check for required fields
        if (!jsonData.hasProperty("channels"))
        {
            addError("JSON missing 'channels' property", "JSON_MISSING_CHANNELS", 3);
            LOGD("Available properties: ", JSON::toString(jsonData));
            return false;
        }

        var channels = jsonData["channels"];
        if (!channels.isArray())
        {
            addError("Invalid JSON format: 'channels' is not an array", "JSON_CHANNELS_NOT_ARRAY", 3);
            return false;
        }

        int channelCount = channels.size();
        LOGD("Found channels array with ", channelCount, " elements (expected: ", numChannels, ")");

        if (channelCount != numChannels)
        {
            String errorMsg = String::formatted("Channel count mismatch: JSON has %d channels, expected %d",
                channelCount, numChannels);
            addError(errorMsg, "JSON_CHANNEL_COUNT_MISMATCH", 2);

            // Try to handle gracefully instead of failing completely
            if (channelCount > numChannels)
            {
                addError("Truncating extra channels to match expected count", "JSON_TRUNCATING", 1);
                channelCount = numChannels; // Truncate
            }
            else
            {
                addError("Will pad missing channels with zeros", "JSON_PADDING", 1);
                // Continue with available channels, pad later
            }
        }

        channelData.clear();
        channelData.ensureStorageAllocated(channelCount);

        // Parse channel values with detailed logging
        float minVal = std::numeric_limits<float>::max();
        float maxVal = std::numeric_limits<float>::lowest();
        int validChannels = 0;
        int invalidValues = 0;

        for (int i = 0; i < channelCount; i++)
        {
            if (channels[i].isDouble() || channels[i].isInt())
            {
                float value = (float)channels[i];

                // Validate value range
                if (std::isnan(value) || std::isinf(value))
                {
                    addError(String::formatted("Invalid value (NaN/Inf) at channels[%d]", i), "JSON_INVALID_VALUE", 2);
                    value = 0.0f; // Replace with zero
                    invalidValues++;
                }
                else if (std::abs(value) > 1000000.0f) // Sanity check for extremely large values
                {
                    addError(String::formatted("Extremely large value at channels[%d]: %f", i, value), "JSON_LARGE_VALUE", 1);
                }

                channelData.add(value);
                validChannels++;

                // Track min/max for logging
                minVal = std::min(minVal, value);
                maxVal = std::max(maxVal, value);
            }
            else
            {
                addError(String::formatted("Non-numeric value in channels array at index %d", i), "JSON_NON_NUMERIC", 2);
                channelData.add(0.0f); // Add zero for non-numeric values
                invalidValues++;
                validChannels++;
            }
        }

        // Pad with zeros if we have fewer channels than expected
        while (channelData.size() < numChannels)
        {
            channelData.add(0.0f);
        }

        if (invalidValues > 0)
        {
            addError(String::formatted("Replaced %d invalid values with zeros", invalidValues), "JSON_VALUES_REPLACED", 1);
        }

        LOGD("Successfully parsed ", validChannels, " channels, range: [", minVal, ", ", maxVal, "]");

        // Log timestamp if present
        if (jsonData.hasProperty("timestamp"))
        {
            var timestamp = jsonData["timestamp"];
            LOGD("JSON timestamp: ", (int64)timestamp);
        }
        else
        {
            LOGD("No timestamp in JSON data");
        }

        return true;
    }
    catch (const std::bad_alloc& e)
    {
        addError("Memory allocation failed during JSON parsing: " + String(e.what()), "JSON_MEMORY_ERROR", 4);
        return false;
    }
    catch (const std::runtime_error& e)
    {
        addError("Runtime error parsing JSON data: " + String(e.what()), "JSON_RUNTIME_ERROR", 3);
        return false;
    }
    catch (const std::logic_error& e)
    {
        addError("Logic error parsing JSON data: " + String(e.what()), "JSON_LOGIC_ERROR", 3);
        return false;
    }
    catch (const std::exception& e)
    {
        addError("Exception parsing JSON data: " + String(e.what()), "JSON_EXCEPTION", 3);
        return false;
    }
    catch (...)
    {
        addError("Unknown exception parsing JSON data", "JSON_UNKNOWN_EXCEPTION", 3);
        LOGD("JSON string that failed: ", jsonStr.substring(0, 200), "...");
        return false;
    }
}

bool RedisDataThread::parseBinaryData(const char* data, size_t length, Array<float>& channelData)
{
    // Expect binary data as array of floats
    if (length % sizeof(float) != 0)
    {
        LOGE("Binary data length not multiple of float size");
        return false;
    }

    int numSamples = length / sizeof(float);
    if (numSamples != numChannels)
    {
        LOGE("Binary data contains ", numSamples, " samples, expected ", numChannels);
        return false;
    }

    channelData.clear();
    const float* floatData = reinterpret_cast<const float*>(data);

    for (int i = 0; i < numSamples; i++)
    {
        channelData.add(floatData[i]);
    }

    return true;
}

#ifdef REDIS_ENABLED
bool RedisDataThread::processStreamEntry(redisReply* fieldsReply)
{
    // Parse field-value pairs from stream entry
    for (size_t i = 0; i < fieldsReply->elements; i += 2)
    {
        if (i + 1 < fieldsReply->elements)
        {
            // Optimize: Use string views for field name comparison
            StringUtils::StringView fieldNameView(fieldsReply->element[i]->str, fieldsReply->element[i]->len);

            // Only create String objects when necessary
            String fieldValue; // Will be created only if needed

            // Log with optimized string handling
            if (fieldsReply->element[i + 1]->len > 100) {
                String truncatedValue(fieldsReply->element[i + 1]->str, 100);
                LOGD("Stream field: ", fieldNameView.toString(), " = ", truncatedValue, "...");
            } else {
                LOGD("Stream field: ", fieldNameView.toString(), " = ",
                     String(fieldsReply->element[i + 1]->str, fieldsReply->element[i + 1]->len));
            }

            // Process different field types using string view
            if (fieldNameView.equals("brandbci_data") || fieldNameView.equals("data"))
            {
                // Create String only when needed for parsing
                fieldValue = String(fieldsReply->element[i + 1]->str, fieldsReply->element[i + 1]->len);
                Array<float> channelData;
                bool parseSuccess = false;

                if (dataFormat == "brandbci")
                {
                    parseSuccess = parseBrandBCIData(fieldValue, channelData);
                }
                else if (dataFormat == "json")
                {
                    parseSuccess = parseJsonData(fieldValue, channelData);
                }

                if (parseSuccess && channelData.size() == numChannels)
                {
                    // Add data to buffer
                    if (sourceBuffers.size() > 0)
                    {
                        DataBuffer* buffer = sourceBuffers[0];
                        if (buffer != nullptr)
                        {
                            // Prepare data for buffer (channel-major order)
                            int numSamples = 1; // One sample per stream entry
                            int64 sampleNumber = currentSampleNumber.load();
                            double timestamp = Time::getMillisecondCounterHiRes() / 1000.0;
                            uint64 eventCode = 0;

                            int written = buffer->addToBuffer(channelData.getRawDataPointer(),
                                                            &sampleNumber,
                                                            &timestamp,
                                                            &eventCode,
                                                            numSamples);

                            if (written > 0)
                            {
                                currentSampleNumber++;
                                LOGD("Added ", numChannels, " channels to buffer, sample #", currentSampleNumber.load());
                                return true;
                            }
                            else
                            {
                                LOGE("Failed to write to DataBuffer");
                            }
                        }
                    }
                }
                else if (!parseSuccess)
                {
                    LOGE("Failed to parse stream data in format: ", dataFormat);
                }
                else
                {
                    LOGE("Channel count mismatch: got ", channelData.size(), " channels, expected ", numChannels);
                }
            }
        }
    }

    return false;
}
#else
bool RedisDataThread::processStreamEntry(void* fieldsReply)
{
    // Redis not enabled - stub implementation
    return false;
}
#endif

bool RedisDataThread::parseBrandBCIData(const String& jsonStr, Array<float>& channelData)
{
    LOGD("Parsing BRANDBCI data, length: ", jsonStr.length(), " characters");

    try
    {
        var jsonData = JSON::parse(jsonStr);
        LOGD("BRANDBCI JSON parsing successful");

        if (!jsonData.isObject())
        {
            LOGE("Invalid BRANDBCI format: not an object");
            return false;
        }

        // Check for BRANDBCI structure: data.channels
        if (jsonData.hasProperty("data"))
        {
            var dataObj = jsonData["data"];
            if (dataObj.isObject() && dataObj.hasProperty("channels"))
            {
                var channels = dataObj["channels"];
                if (channels.isArray())
                {
                    channelData.clear();
                    channelData.ensureStorageAllocated(channels.size());

                    for (int i = 0; i < channels.size(); i++)
                    {
                        if (channels[i].isDouble() || channels[i].isInt())
                        {
                            channelData.add((float)channels[i]);
                        }
                        else
                        {
                            LOGE("Non-numeric value in BRANDBCI channels array at index ", i);
                            return false;
                        }
                    }

                    LOGD("Successfully parsed ", channelData.size(), " BRANDBCI channels");
                    return true;
                }
            }
        }

        // Fallback: try direct channels array (legacy format)
        if (jsonData.hasProperty("channels"))
        {
            return parseJsonData(jsonStr, channelData);
        }

        LOGE("BRANDBCI data missing required structure");
        return false;

    }
    catch (const std::bad_alloc& e)
    {
        addError("Memory allocation failed during BRANDBCI parsing: " + String(e.what()), "BRANDBCI_MEMORY_ERROR", 4);
        return false;
    }
    catch (const std::runtime_error& e)
    {
        addError("Runtime error parsing BRANDBCI data: " + String(e.what()), "BRANDBCI_RUNTIME_ERROR", 3);
        return false;
    }
    catch (const std::logic_error& e)
    {
        addError("Logic error parsing BRANDBCI data: " + String(e.what()), "BRANDBCI_LOGIC_ERROR", 3);
        return false;
    }
    catch (const std::exception& e)
    {
        addError("Exception parsing BRANDBCI data: " + String(e.what()), "BRANDBCI_EXCEPTION", 3);
        return false;
    }
    catch (...)
    {
        addError("Unknown exception parsing BRANDBCI data", "BRANDBCI_UNKNOWN_EXCEPTION", 3);
        return false;
    }
}





bool RedisDataThread::readFromStream(const String& streamName, String& data, String& newId)
{
#ifdef REDIS_ENABLED
    if (!redisCtx.isValid() || !connectionStatus.load()) {
        return false;
    }

    // Use XREAD to read from stream
    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(),
        "XREAD COUNT 1 BLOCK 1 STREAMS %s %s",
        streamName.toRawUTF8(), currentStreamId.toRawUTF8()));

    if (!reply.isValid()) {
        handleRedisError("XREAD command failed", "");
        return false;
    }

    bool success = false;

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
        // Parse stream response
        redisReply* streamReply = reply->element[0];
        if (streamReply->type == REDIS_REPLY_ARRAY && streamReply->elements >= 2) {
            redisReply* entriesReply = streamReply->element[1];
            if (entriesReply->type == REDIS_REPLY_ARRAY && entriesReply->elements > 0) {
                redisReply* entryReply = entriesReply->element[0];
                if (entryReply->type == REDIS_REPLY_ARRAY && entryReply->elements >= 2) {
                    // Get entry ID
                    newId = String(entryReply->element[0]->str);

                    // Get entry data (field-value pairs)
                    redisReply* fieldsReply = entryReply->element[1];
                    if (fieldsReply->type == REDIS_REPLY_ARRAY && fieldsReply->elements >= 2) {
                        // Assume first field contains our data
                        data = String(fieldsReply->element[1]->str);
                        success = true;
                    }
                }
            }
        }
    }
    return success;
#else
    return false;
#endif
}

// ============================================================================
// Enhanced Error Handling Implementation
// ============================================================================

void RedisDataThread::addError(const String& message, const String& code, int severity)
{
    std::lock_guard<std::mutex> lock(errorHistoryMutex);

    ErrorInfo error(message, code, severity);
    errorHistory.push_back(error);

    // Limit error history size
    if (errorHistory.size() > 100) {
        errorHistory.erase(errorHistory.begin());
    }

    // Log based on severity
    switch (severity) {
        case 1: // Info
            LOGD("Redis Info: ", message, " (", code, ")");
            break;
        case 2: // Warning
            LOGD("Redis Warning: ", message, " (", code, ")");
            break;
        case 3: // Error
            LOGE("Redis Error: ", message, " (", code, ")");
            break;
        case 4: // Critical
            LOGE("Redis Critical: ", message, " (", code, ")");
            break;
    }
}

void RedisDataThread::clearErrors()
{
    std::lock_guard<std::mutex> lock(errorHistoryMutex);
    errorHistory.clear();
}

std::vector<RedisDataThread::ErrorInfo> RedisDataThread::getRecentErrors(int maxCount) const
{
    std::lock_guard<std::mutex> lock(errorHistoryMutex);

    std::vector<ErrorInfo> result;
    int startIndex = std::max(0, static_cast<int>(errorHistory.size()) - maxCount);

    for (int i = startIndex; i < static_cast<int>(errorHistory.size()); i++) {
        result.push_back(errorHistory[i]);
    }

    return result;
}

String RedisDataThread::getLastErrorMessage() const
{
    std::lock_guard<std::mutex> lock(errorHistoryMutex);

    if (errorHistory.empty()) {
        return "No errors recorded";
    }

    const auto& lastError = errorHistory.back();
    return String::formatted("[%s] %s (%s)",
        lastError.timestamp.toString(true, true).toRawUTF8(),
        lastError.errorMessage.toRawUTF8(),
        lastError.errorCode.toRawUTF8());
}

String RedisDataThread::getConnectionStateString() const
{
    switch (currentConnectionState.load()) {
        case ConnectionState::DISCONNECTED: return "Disconnected";
        case ConnectionState::CONNECTING: return "Connecting...";
        case ConnectionState::CONNECTED: return "Connected";
        case ConnectionState::CONNECTION_FAILED: return "Connection Failed";
        case ConnectionState::RECONNECTING: return "Reconnecting...";
        case ConnectionState::AUTHENTICATION_FAILED: return "Authentication Failed";
        case ConnectionState::TIMEOUT: return "Connection Timeout";
        default: return "Unknown";
    }
}

void RedisDataThread::updateConnectionState(ConnectionState newState)
{
    ConnectionState oldState = currentConnectionState.exchange(newState);

    if (oldState != newState) {
        String stateMsg = String::formatted("Connection state changed: %s → %s",
            getConnectionStateString().toRawUTF8(),
            getConnectionStateString().toRawUTF8());

        addError(stateMsg, "STATE_CHANGE", 1); // Info level
    }
}

bool RedisDataThread::connectToRedisWithRetry()
{
    updateConnectionState(ConnectionState::CONNECTING);

    // Validate configuration first
    String configError;
    if (!validateConfiguration(configError)) {
        addError("Configuration validation failed: " + configError, "CONFIG_ERROR", 3);
        updateConnectionState(ConnectionState::CONNECTION_FAILED);
        return false;
    }

    // Attempt connection
    bool success = connectToRedis(redisHost, redisPort, redisPassword);

    if (success) {
        updateConnectionState(ConnectionState::CONNECTED);
        resetRetryState();
        addError("Successfully connected to Redis", "CONNECT_SUCCESS", 1);
    } else {
        updateConnectionState(ConnectionState::CONNECTION_FAILED);

        if (retryConfig.enableAutoReconnect && currentRetryCount.load() < retryConfig.maxRetries) {
            scheduleReconnection();
        }
    }

    return success;
}

void RedisDataThread::scheduleReconnection()
{
    int retryCount = currentRetryCount.fetch_add(1);

    // Calculate delay with exponential backoff
    int delay = retryConfig.retryDelayMs * static_cast<int>(std::pow(retryConfig.backoffMultiplier, retryCount));
    delay = std::min(delay, retryConfig.maxRetryDelayMs);

    nextRetryTime.store(Time::getMillisecondCounterHiRes() + delay);

    addError(String::formatted("Scheduling reconnection attempt %d/%d in %dms",
        retryCount + 1, retryConfig.maxRetries, delay), "RETRY_SCHEDULED", 2);
}

bool RedisDataThread::attemptReconnection()
{
    if (!shouldAttemptReconnect()) {
        return false;
    }

    updateConnectionState(ConnectionState::RECONNECTING);

    addError(String::formatted("Attempting reconnection %d/%d",
        currentRetryCount.load() + 1, retryConfig.maxRetries), "RECONNECT_ATTEMPT", 1);

    return connectToRedisWithRetry();
}

bool RedisDataThread::shouldAttemptReconnect() const
{
    if (!retryConfig.enableAutoReconnect) {
        return false;
    }

    if (currentRetryCount.load() >= retryConfig.maxRetries) {
        return false;
    }

    if (currentConnectionState.load() == ConnectionState::CONNECTED) {
        return false;
    }

    return Time::getMillisecondCounterHiRes() >= nextRetryTime.load();
}

void RedisDataThread::resetRetryState()
{
    currentRetryCount.store(0);
    nextRetryTime.store(0);
}

bool RedisDataThread::validateConfiguration(String& errorMessage) const
{
    // Validate host
    if (!isValidHost(redisHost)) {
        errorMessage = "Invalid host address: " + redisHost;
        return false;
    }

    // Validate port
    if (!isValidPort(redisPort)) {
        errorMessage = String::formatted("Invalid port number: %d (must be 1-65535)", redisPort);
        return false;
    }

    // Validate channel name
    if (!isValidChannelName(redisChannel)) {
        errorMessage = "Invalid channel name: " + redisChannel;
        return false;
    }

    // Validate sample rate
    if (sampleRate <= 0 || sampleRate > 100000) {
        errorMessage = String::formatted("Invalid sample rate: %.1f (must be > 0 and <= 100000)", sampleRate);
        return false;
    }

    // Validate number of channels
    if (numChannels <= 0 || numChannels > 1024) {
        errorMessage = String::formatted("Invalid number of channels: %d (must be > 0 and <= 1024)", numChannels);
        return false;
    }

    return true;
}

bool RedisDataThread::isValidHost(const String& host) const
{
    if (host.isEmpty()) {
        return false;
    }

    // Check for localhost variants
    if (host == "localhost" || host == "127.0.0.1" || host == "::1") {
        return true;
    }

    // Basic IP address validation (IPv4)
    StringArray parts = StringArray::fromTokens(host, ".", "");
    if (parts.size() == 4) {
        for (const String& part : parts) {
            int value = part.getIntValue();
            if (value < 0 || value > 255) {
                return false;
            }
        }
        return true;
    }

    // Basic hostname validation (allow alphanumeric, dots, hyphens)
    for (int i = 0; i < host.length(); i++) {
        juce_wchar c = host[i];
        if (!CharacterFunctions::isLetterOrDigit(c) && c != '.' && c != '-') {
            return false;
        }
    }

    return true;
}

bool RedisDataThread::isValidPort(int port) const
{
    return port > 0 && port <= 65535;
}

bool RedisDataThread::isValidChannelName(const String& channel) const
{
    if (channel.isEmpty()) {
        return false;
    }

    // Redis key names can contain most characters, but avoid problematic ones
    for (int i = 0; i < channel.length(); i++) {
        juce_wchar c = channel[i];
        if (c < 32 || c == 127) { // Control characters
            return false;
        }
    }

    return true;
}

bool RedisDataThread::executeRedisCommand(const String& command, String& result, String& errorMsg)
{
#ifdef REDIS_ENABLED
    if (!redisCtx.isValid()) {
        errorMsg = "Redis context is null";
        return false;
    }

    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(), command.toRawUTF8()));

    if (!reply.isValid()) {
        errorMsg = "Redis command failed: " + String(redisCtx.getErrorString());
        handleRedisError("executeRedisCommand", errorMsg);
        return false;
    }

    bool success = false;

    switch (reply->type) {
        case REDIS_REPLY_STRING:
            result = StringUtils::createString(reply->str, reply->len);
            success = true;
            break;

        case REDIS_REPLY_INTEGER:
            result = String(reply->integer);
            success = true;
            break;

        case REDIS_REPLY_STATUS:
            result = StringUtils::createString(reply->str, reply->len);
            success = true;
            break;

        case REDIS_REPLY_ERROR:
            errorMsg = StringUtils::createString(reply->str, reply->len);
            success = false;
            break;

        case REDIS_REPLY_NIL:
            result = "";
            success = true;
            break;

        default:
            errorMsg = "Unexpected reply type: " + String(reply->type);
            success = false;
            break;
    }

    if (!success) {
        handleRedisError("executeRedisCommand", errorMsg);
    }

    return success;
#else
    errorMsg = "Redis support not compiled";
    return false;
#endif
}

bool RedisDataThread::testRedisConnection()
{
    String result, errorMsg;
    bool success = executeRedisCommand("PING", result, errorMsg);

    if (success && result == "PONG") {
        addError("Redis connection test successful", "PING_SUCCESS", 1);
        return true;
    } else {
        addError("Redis connection test failed: " + errorMsg, "PING_FAILED", 3);
        return false;
    }
}

void RedisDataThread::handleRedisError(const String& operation, const String& details)
{
    String errorMsg = String::formatted("Redis operation '%s' failed", operation.toRawUTF8());
    if (details.isNotEmpty()) {
        errorMsg += ": " + details;
    }

    addError(errorMsg, "REDIS_OP_FAILED", 3);

    // Record operation failure for health monitoring
    recordOperationFailure();

    // Check if we should attempt reconnection
    if (currentConnectionState.load() == ConnectionState::CONNECTED) {
        updateConnectionState(ConnectionState::CONNECTION_FAILED);

        if (retryConfig.enableAutoReconnect) {
            scheduleReconnection();
        }
    }
}

// Connection health monitoring implementation
bool RedisDataThread::performConnectionHealthCheck()
{
#ifdef REDIS_ENABLED
    if (!redisCtx.isValid() || healthCheckInProgress.load()) {
        return false;
    }

    healthCheckInProgress.store(true);
    auto startTime = std::chrono::high_resolution_clock::now();

    // Perform PING command to test connection
    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(), "PING"));

    auto endTime = std::chrono::high_resolution_clock::now();
    float latency = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    bool success = false;
    if (reply.isValid()) {
        if (reply->type == REDIS_REPLY_STATUS &&
            String(reply->str) == "PONG") {
            success = true;
            recordOperationSuccess();
            perfMetrics.connectionLatency.store(latency);
        }
    }

    if (!success) {
        recordOperationFailure();
        addError("Health check failed - PING command unsuccessful", "HEALTH_CHECK_FAILED", 2);
    }

    lastHealthCheck.store(Time::getMillisecondCounterHiRes());
    healthCheckInProgress.store(false);

    return success;
#else
    return false;
#endif
}

bool RedisDataThread::shouldPerformHealthCheck() const
{
    if (!healthConfig.enableHealthChecks ||
        currentConnectionState.load() != ConnectionState::CONNECTED) {
        return false;
    }

    int64 currentTime = Time::getMillisecondCounterHiRes();
    int64 timeSinceLastCheck = currentTime - lastHealthCheck.load();

    return timeSinceLastCheck >= healthConfig.healthCheckIntervalMs;
}

void RedisDataThread::updateConnectionMetrics()
{
    // Update connection stability based on recent performance
    int failures = perfMetrics.consecutiveFailures.load();
    float stability = 1.0f;

    if (failures > 0) {
        stability = std::max(0.0f, 1.0f - (failures * 0.2f));
    }

    perfMetrics.connectionStability.store(stability);
}

void RedisDataThread::recordOperationLatency(float latencyMs)
{
    // Update average latency with exponential moving average
    float currentAvg = perfMetrics.avgLatency.load();
    float newAvg = (currentAvg * 0.9f) + (latencyMs * 0.1f);
    perfMetrics.avgLatency.store(newAvg);

    // Update max latency
    float currentMax = perfMetrics.maxLatency.load();
    if (latencyMs > currentMax) {
        perfMetrics.maxLatency.store(latencyMs);
    }
}

void RedisDataThread::recordOperationSuccess()
{
    perfMetrics.consecutiveFailures.store(0);
    perfMetrics.lastSuccessfulOperation.store(Time::getMillisecondCounterHiRes());
    updateConnectionMetrics();
}

void RedisDataThread::recordOperationFailure()
{
    int failures = perfMetrics.consecutiveFailures.fetch_add(1);
    updateConnectionMetrics();

    // Log warning if consecutive failures are increasing
    if (failures >= healthConfig.maxConsecutiveFailures) {
        addError(String::formatted("High consecutive failure count: %d", failures + 1),
                "HIGH_FAILURE_RATE", 2);
    }
}

// High-performance buffer management implementation
void RedisDataThread::initializeBufferPool()
{
    LOGD("Initializing high-performance buffer pool with ", BUFFER_POOL_SIZE, " packets");

    // Pre-allocate all data packets
    for (size_t i = 0; i < BUFFER_POOL_SIZE; ++i) {
        bufferPool[i].channelData.reserve(numChannels);
        bufferPool[i].reset();
    }

    // Pre-allocate working buffers
    workingBuffer.reserve(numChannels);
    sampleNumberBuffer.reserve(1);
    timestampBuffer.reserve(1);
    eventCodeBuffer.reserve(1);

    LOGD("Buffer pool initialized successfully");
}

RedisDataThread::DataPacket* RedisDataThread::acquireDataPacket()
{
    std::lock_guard<std::mutex> lock(poolMutex);

    // Find an available packet using round-robin
    for (size_t attempts = 0; attempts < BUFFER_POOL_SIZE; ++attempts) {
        size_t index = (poolIndex.fetch_add(1) % BUFFER_POOL_SIZE);
        DataPacket* packet = &bufferPool[index];

        if (!packet->inUse) {
            packet->inUse = true;
            packet->channelData.clear();
            packet->channelData.reserve(numChannels);
            return packet;
        }
    }

    // All packets in use - this indicates a performance bottleneck
    perfMetrics.droppedSamples.fetch_add(1);
    addError("Buffer pool exhausted - dropping data packet", "BUFFER_POOL_FULL", 2);
    return nullptr;
}

void RedisDataThread::releaseDataPacket(DataPacket* packet)
{
    if (packet) {
        std::lock_guard<std::mutex> lock(poolMutex);
        packet->reset();
    }
}

bool RedisDataThread::parseJsonDataOptimized(const char* jsonStr, size_t length, DataPacket* packet)
{
    if (!packet) return false;

    auto parseStart = std::chrono::high_resolution_clock::now();

    try {
        // Optimize: Only create String when necessary, use CharPointer directly when possible
        var jsonData;

        // For small JSON strings, use direct parsing to avoid string copy
        if (length < 4096) {
            // Create a null-terminated copy only for small strings
            std::vector<char> buffer(length + 1);
            memcpy(buffer.data(), jsonStr, length);
            buffer[length] = '\0';
            jsonData = JSON::parse(String(buffer.data()));
        } else {
            // For larger strings, create String object
            String jsonString(jsonStr, length);
            jsonData = JSON::parse(jsonString);
        }

        if (!jsonData.isObject() || !jsonData.hasProperty("channels")) {
            return false;
        }

        var channels = jsonData["channels"];
        if (!channels.isArray()) {
            return false;
        }

        // Pre-allocate the exact size needed
        int channelCount = channels.size();
        packet->channelData.resize(std::min(channelCount, numChannels));

        // Fast conversion with minimal error checking for performance
        for (int i = 0; i < packet->channelData.size(); ++i) {
            var value = channels[i];
            packet->channelData[i] = (float)value;
        }

        // Pad with zeros if needed
        while (packet->channelData.size() < numChannels) {
            packet->channelData.push_back(0.0f);
        }

        // Extract timestamp if present
        if (jsonData.hasProperty("timestamp")) {
            packet->timestamp = (double)jsonData["timestamp"];
        } else {
            packet->timestamp = Time::getMillisecondCounterHiRes() / 1000.0;
        }

        packet->sampleNumber = currentSampleNumber.load();
        packet->eventCode = 0;

        auto parseEnd = std::chrono::high_resolution_clock::now();
        float parseLatency = std::chrono::duration<float, std::milli>(parseEnd - parseStart).count();
        recordOperationLatency(parseLatency);

        return true;

    } catch (const std::bad_alloc& e) {
        addError("Memory allocation failed during optimized JSON parsing: " + String(e.what()), "JSON_OPT_MEMORY_ERROR", 4);
        return false;
    } catch (const std::runtime_error& e) {
        addError("Runtime error in optimized JSON parsing: " + String(e.what()), "JSON_OPT_RUNTIME_ERROR", 3);
        return false;
    } catch (const std::logic_error& e) {
        addError("Logic error in optimized JSON parsing: " + String(e.what()), "JSON_OPT_LOGIC_ERROR", 3);
        return false;
    } catch (const std::exception& e) {
        addError("Exception in optimized JSON parsing: " + String(e.what()), "JSON_OPT_EXCEPTION", 3);
        return false;
    } catch (...) {
        addError("Unknown exception in optimized JSON parsing", "JSON_OPT_UNKNOWN_EXCEPTION", 3);
        return false;
    }
}

bool RedisDataThread::processDataPacketToBuffer(DataPacket* packet)
{
    if (!packet || packet->channelData.empty()) {
        return false;
    }

    DataBuffer* buffer = getBufferAddress(0);
    if (!buffer) {
        addError("DataBuffer is null", "BUFFER_NULL", 3);
        return false;
    }

    // Prepare data for efficient buffer write
    sampleNumberBuffer.clear();
    timestampBuffer.clear();
    eventCodeBuffer.clear();

    sampleNumberBuffer.push_back(packet->sampleNumber);
    timestampBuffer.push_back(packet->timestamp);
    eventCodeBuffer.push_back(packet->eventCode);

    // Write to buffer in one operation
    int written = buffer->addToBuffer(packet->channelData.data(),
                                    sampleNumberBuffer.data(),
                                    timestampBuffer.data(),
                                    eventCodeBuffer.data(),
                                    1);

    if (written > 0) {
        currentSampleNumber++;
        return true;
    } else {
        perfMetrics.droppedSamples.fetch_add(1);
        addError("Failed to write to DataBuffer", "BUFFER_WRITE_FAILED", 3);
        return false;
    }
}

// High-performance multi-threading implementation
void RedisDataThread::startDataProcessingThread()
{
    if (dataProcessingThread && dataProcessingThread->joinable()) {
        LOGD("Data processing thread already running");
        return;
    }

    LOGD("Starting high-performance data processing thread");
    shouldProcessData.store(true);

    dataProcessingThread = std::make_unique<std::thread>([this]() {
        dataProcessingLoop();
    });

    LOGD("Data processing thread started successfully");
}

void RedisDataThread::stopDataProcessingThread()
{
    if (!dataProcessingThread) {
        return;
    }

    LOGD("Stopping data processing thread");
    shouldProcessData.store(false);

    // Wake up the processing thread
    {
        std::lock_guard<std::mutex> lock(dataQueueMutex);
        dataAvailableCondition.notify_all();
    }

    if (dataProcessingThread->joinable()) {
        dataProcessingThread->join();
    }

    dataProcessingThread.reset();
    LOGD("Data processing thread stopped successfully");
}

void RedisDataThread::dataProcessingLoop()
{
    LOGD("Data processing loop started");

    try {
        while (shouldProcessData.load()) {
            RawDataPacket* rawPacket = nullptr;

            try {
                // Wait for data or stop signal
                {
                    std::unique_lock<std::mutex> lock(dataQueueMutex);
                    dataAvailableCondition.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                        return rawQueueCount.load() > 0 || !shouldProcessData.load();
                    });
                }

                if (!shouldProcessData.load()) {
                    break;
                }

                // Process available data packets
                while (dequeueRawData(rawPacket) && rawPacket) {
            auto processStart = std::chrono::high_resolution_clock::now();

            // Acquire a processed data packet
            DataPacket* packet = acquireDataPacket();
            if (!packet) {
                LOGE("Failed to acquire data packet in processing thread");
                releaseRawDataPacket(rawPacket);
                continue;
            }

            bool success = false;

            // Parse data based on format
            if (rawPacket->dataFormat == "json") {
                success = parseJsonDataOptimized(rawPacket->rawData.data(),
                                               rawPacket->dataLength, packet);
            } else {
                // For other formats, fall back to original parsing
                // TODO: Implement optimized binary and brandbci parsing
                LOGD("Using fallback parsing for format: ", rawPacket->dataFormat);
                releaseDataPacket(packet);
                releaseRawDataPacket(rawPacket);
                continue;
            }

            if (success && packet->channelData.size() == numChannels) {
                // Process to buffer
                success = processDataPacketToBuffer(packet);

                if (success) {
                    auto processEnd = std::chrono::high_resolution_clock::now();
                    float processLatency = std::chrono::duration<float, std::milli>(
                        processEnd - processStart).count();

                    // Record total latency including network time
                    auto totalLatency = std::chrono::duration<float, std::milli>(
                        std::chrono::steady_clock::now() - rawPacket->receiveTime).count();
                    recordOperationLatency(totalLatency);

                    recordOperationSuccess();
                } else {
                    recordOperationFailure();
                }
            } else {
                recordOperationFailure();
                if (!success) {
                    LOGE("Data parsing failed in processing thread");
                } else {
                    LOGE("Channel count mismatch in processing thread: got ",
                         packet->channelData.size(), " expected ", numChannels);
                }
            }

                    // Release packets back to pools
                    releaseDataPacket(packet);
                    releaseRawDataPacket(rawPacket);
                }
            } catch (const std::exception& e) {
                addError("Exception in data processing loop iteration: " + String(e.what()), "DATA_PROCESSING_EXCEPTION", 3);
                recordOperationFailure();
                // Continue processing other packets
            } catch (...) {
                addError("Unknown exception in data processing loop iteration", "DATA_PROCESSING_UNKNOWN_EXCEPTION", 3);
                recordOperationFailure();
                // Continue processing other packets
            }
        }

    } catch (const std::exception& e) {
        addError("Fatal exception in data processing loop: " + String(e.what()), "DATA_PROCESSING_FATAL_EXCEPTION", 4);
        LOGE("Data processing loop terminated due to exception: ", e.what());
    } catch (...) {
        addError("Fatal unknown exception in data processing loop", "DATA_PROCESSING_FATAL_UNKNOWN_EXCEPTION", 4);
        LOGE("Data processing loop terminated due to unknown exception");
    }

    LOGD("Data processing loop ended");
}

bool RedisDataThread::enqueueRawData(const char* data, size_t length, const std::string& format)
{
    if (rawQueueCount.load() >= RAW_QUEUE_SIZE) {
        perfMetrics.droppedSamples.fetch_add(1);
        addError("Raw data queue full - dropping packet", "RAW_QUEUE_FULL", 2);
        return false;
    }

    size_t writeIndex = rawQueueWriteIndex.fetch_add(1) % RAW_QUEUE_SIZE;
    RawDataPacket* packet = &rawDataQueue[writeIndex];

    if (packet->inUse) {
        // This shouldn't happen if queue size is adequate
        perfMetrics.droppedSamples.fetch_add(1);
        return false;
    }

    // Copy data efficiently
    packet->rawData.assign(data, data + length);
    packet->dataLength = length;
    packet->dataFormat = format;
    packet->receiveTime = std::chrono::steady_clock::now();
    packet->inUse = true;

    rawQueueCount.fetch_add(1);

    // Notify processing thread
    {
        std::lock_guard<std::mutex> lock(dataQueueMutex);
        dataAvailableCondition.notify_one();
    }

    return true;
}

bool RedisDataThread::dequeueRawData(RawDataPacket*& packet)
{
    if (rawQueueCount.load() == 0) {
        packet = nullptr;
        return false;
    }

    size_t readIndex = rawQueueReadIndex.fetch_add(1) % RAW_QUEUE_SIZE;
    packet = &rawDataQueue[readIndex];

    if (!packet->inUse) {
        packet = nullptr;
        return false;
    }

    rawQueueCount.fetch_sub(1);
    return true;
}

void RedisDataThread::releaseRawDataPacket(RawDataPacket* packet)
{
    if (packet) {
        packet->reset();
    }
}



// Stream management methods
void RedisDataThread::setStreamMode(bool useStreams)
{
    std::lock_guard<std::mutex> lock(configMutex);
    // Redis data is written using XADD (streams), so always use stream mode
    if (!useStreams)
    {
        LOGE("Attempted to disable stream mode, but Redis data uses XADD (streams). Keeping stream mode enabled.");
    }
    useStreamMode = true;  // Always use stream mode
    LOGD("Stream mode: ENABLED (Redis uses XADD for streams)");

    if (dataFormat == "json")
    {
        // Auto-switch to brandbci format for better stream support
        dataFormat = "brandbci";
        LOGD("Auto-switched data format to 'brandbci' for stream mode");
    }
}

void RedisDataThread::setStreamPattern(const String& pattern)
{
    std::lock_guard<std::mutex> lock(configMutex);
    streamPattern = pattern;
    LOGD("Stream pattern set to: ", pattern);
}

Array<String> RedisDataThread::discoverStreams(const String& pattern)
{
    Array<String> streams;

#ifdef REDIS_ENABLED
    if (!redisCtx.isValid() || !connectionStatus.load())
    {
        LOGE("Cannot discover streams - Redis not connected");
        return streams;
    }

    LOGD("Discovering streams with pattern: ", pattern);

    // Use KEYS command to find streams matching pattern
    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(), "KEYS %s", pattern.toRawUTF8()));

    if (reply.isValid() && reply->type == REDIS_REPLY_ARRAY)
    {
        LOGD("Found ", reply->elements, " potential streams");

        for (size_t i = 0; i < reply->elements; i++)
        {
            if (reply->element[i]->type == REDIS_REPLY_STRING)
            {
                // Optimize: Use string view for initial processing
                StringUtils::StringView streamNameView(reply->element[i]->str, reply->element[i]->len);

                // Create String only when needed for Redis command
                String streamName = streamNameView.toString();

                // Verify it's actually a stream
                RedisReplyRAII typeReply((redisReply*)redisCommand(redisCtx.get(), "TYPE %s", streamName.toRawUTF8()));

                if (typeReply.isValid() && typeReply->type == REDIS_REPLY_STATUS &&
                    StringUtils::StringView(typeReply->str, typeReply->len).equals("stream"))
                {
                    streams.add(streamName);
                    LOGD("Discovered stream: ", streamName);
                }
            }
        }
    }
    else
    {
        LOGE("Failed to discover streams - KEYS command failed");
    }

    LOGD("Stream discovery complete: found ", streams.size(), " streams");
#endif

    return streams;
}

bool RedisDataThread::subscribeToStream(const String& streamName)
{
#ifdef REDIS_ENABLED
    if (!redisCtx.isValid() || !connectionStatus.load())
    {
        LOGE("Cannot subscribe to stream - Redis not connected");
        return false;
    }

    // Check if stream exists
    RedisReplyRAII reply((redisReply*)redisCommand(redisCtx.get(), "EXISTS %s", streamName.toRawUTF8()));
    bool exists = (reply.isValid() && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);

    if (exists)
    {
        // Add to active streams if not already present
        {
            std::lock_guard<std::mutex> lock(streamMutex);
            bool alreadyActive = false;
            for (int i = 0; i < activeStreams.size(); i++)
            {
                if (activeStreams[i] == streamName)
                {
                    alreadyActive = true;
                    break;
                }
            }

            if (!alreadyActive)
            {
                activeStreams.add(streamName);
                LOGD("Subscribed to stream: ", streamName, " (total active: ", activeStreams.size(), ")");
            }
            else
            {
                LOGD("Already subscribed to stream: ", streamName);
            }
        }

        return true;
    }
    else
    {
        LOGE("Cannot subscribe to non-existent stream: ", streamName);
        return false;
    }
#else
    return false;
#endif
}

void RedisDataThread::unsubscribeFromStream(const String& streamName)
{
    std::lock_guard<std::mutex> lock(streamMutex);
    for (int i = 0; i < activeStreams.size(); i++)
    {
        if (activeStreams[i] == streamName)
        {
            activeStreams.remove(i);
            LOGD("Unsubscribed from stream: ", streamName, " (remaining active: ", activeStreams.size(), ")");
            break;
        }
    }
}

Array<String> RedisDataThread::getActiveStreams() const
{
    return activeStreams;
}
