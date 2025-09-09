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
    , redisCtx(nullptr)
    , redisHost("localhost")
    , redisPort(6379)
    , redisChannelName("neural_data_primary")  // Renamed for clarity
    , sampleRate(30000.0f)
    , numDataChannels(32)          // More reasonable default
    , maxDataChannels(1024)        // Set maximum limit
    , dataFormat("brandbci")
    , autoDetectChannels(true)     // Enable auto-detection by default
    , bufferSize(50000)            // Default buffer size: 50000 samples (increased for large data blocks)
    , useStreamMode(true)
    , currentStreamId("0")  // Start from beginning for sequential reading by default
    , alwaysReadLatest(false)  // Sequential reading by default (no data loss)
    , isAcquiring(false)
    , connectionStatus(false)
    , currentSampleNumber(0)
    , dataType("int16")  // Default data type

    , enableDataValidation(true)   // Enable validation by default
    , selectedDataField("data")    // Default field name
    , array2DProcessing(Array2DProcessing::FIRST_ROW)
{
    LOGD("🚀 RedisDataThread constructor called");
    LOGD("Default configuration:");
    LOGD("  - Host: ", redisHost, ":", redisPort);
    LOGD("  - Channel Name: ", redisChannelName);
    LOGD("  - Format: ", dataFormat);
    LOGD("  - Data Channels: ", numDataChannels, " (max: ", maxDataChannels, ")");
    LOGD("  - Auto-detect channels: ", autoDetectChannels ? "enabled" : "disabled");
    LOGD("  - Stream mode: ", useStreamMode ? "ENABLED" : "DISABLED");

    // Validate initial configuration
    if (!validateConfiguration()) {
        LOGD("Initial configuration validation failed - using safe defaults");
    }
    LOGD("  - Sample Rate: ", sampleRate);
    LOGD("  - Stream Mode: ", useStreamMode ? "ENABLED" : "DISABLED");

    LOGD("  - Data Validation: ", enableDataValidation ? "ENABLED" : "DISABLED");
    LOGD("  - Redis context: ", (redisCtx ? "initialized" : "null"));
    LOGD("  - Acquiring: ", isAcquiring.load());
    LOGD("  - Connected: ", connectionStatus.load());

#ifdef REDIS_ENABLED
    LOGD("✓ Redis support is ENABLED");
#else
    LOGE("❌ Redis support is DISABLED - recompile with REDIS_ENABLED=1");
#endif

    // Note: Parameters removed for compact UI design
    // All configuration is now handled through the configuration popup
}

RedisDataThread::~RedisDataThread()
{
    if (isAcquiring.load())
    {
        stopAcquisition();
    }
    disconnectFromRedis();
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
    LOGD("Attempting to connect to Redis server: ", host, ":", port);

    // Disconnect existing connection
    if (connectionStatus.load())
    {
        LOGD("Disconnecting existing Redis connection");
        disconnectFromRedis();
    }

    // Create new connection
    LOGD("Creating Redis connection context");
    redisCtx = redisConnect(host.toRawUTF8(), port);

    if (redisCtx == nullptr || redisCtx->err)
    {
        if (redisCtx)
        {
            LOGE("Redis connection failed: ", redisCtx->errstr, " (error code: ", redisCtx->err, ")");
            redisFree(redisCtx);
            redisCtx = nullptr;
        }
        else
        {
            LOGE("Redis connection failed: Cannot allocate redis context");
        }
        connectionStatus = false;
        LOGE("Redis connection status set to FALSE");
        return false;
    }

    LOGD("Redis context created successfully");

    // Test connection with PING
    LOGD("Testing Redis connection with PING command");
    redisReply* pingReply = (redisReply*)redisCommand(redisCtx, "PING");
    if (pingReply == nullptr || pingReply->type == REDIS_REPLY_ERROR)
    {
        LOGE("Redis PING failed - connection not working");
        if (pingReply) freeReplyObject(pingReply);
        disconnectFromRedis();
        return false;
    }
    LOGD("Redis PING successful: ", (pingReply->str ? pingReply->str : "PONG"));
    freeReplyObject(pingReply);

    // Authenticate if password is provided
    if (password.isNotEmpty())
    {
        LOGD("Authenticating with Redis using provided password");
        redisReply* reply = (redisReply*)redisCommand(redisCtx, "AUTH %s", password.toRawUTF8());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
        {
            LOGE("Redis authentication failed: ", (reply && reply->str) ? reply->str : "unknown error");
            if (reply) freeReplyObject(reply);
            disconnectFromRedis();
            return false;
        }
        LOGD("Redis authentication successful");
        freeReplyObject(reply);
    }
    else
    {
        LOGD("No password provided - skipping authentication");
    }

    // Test channel access based on mode
    LOGD("Testing access to Redis channel: ", redisChannelName, " (mode: ", useStreamMode ? "STREAM" : "LIST", ")");

    redisReply* testReply = nullptr;
    if (useStreamMode)
    {
        // For stream mode, check if key exists and is a stream
        testReply = (redisReply*)redisCommand(redisCtx, "TYPE %s", redisChannelName.toRawUTF8());
        if (testReply && testReply->type == REDIS_REPLY_STATUS)
        {
            String keyType(testReply->str);
            if (keyType == "stream")
            {
                LOGD("Redis stream '", redisChannelName, "' found and accessible");
            }
            else if (keyType == "none")
            {
                LOGD("Redis stream '", redisChannelName, "' does not exist yet (will be created when data arrives)");
            }
            else
            {
                LOGE("Redis key '", redisChannelName, "' exists but is not a stream (type: ", keyType, ")");
                freeReplyObject(testReply);
                disconnectFromRedis();
                return false;
            }
        }
        else
        {
            LOGE("Cannot check Redis key type for '", redisChannelName, "'");
            if (testReply) freeReplyObject(testReply);
            disconnectFromRedis();
            return false;
        }
    }
    else
    {
        // For list mode, use LLEN
        testReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", redisChannelName.toRawUTF8());
        if (testReply == nullptr || testReply->type == REDIS_REPLY_ERROR)
        {
            LOGE("Cannot access Redis channel '", redisChannelName, "': ", (testReply && testReply->str) ? testReply->str : "unknown error");
            if (testReply) freeReplyObject(testReply);
            disconnectFromRedis();
            return false;
        }
        else
        {
            int queueLength = (testReply->type == REDIS_REPLY_INTEGER) ? testReply->integer : -1;
            LOGD("Redis channel '", redisChannelName, "' accessible, current length: ", queueLength);
        }
    }

    if (testReply) freeReplyObject(testReply);

    // Save connection parameters
    redisHost = host;
    redisPort = port;
    redisPassword = password;
    connectionStatus = true;

    LOGD("✓ Successfully connected to Redis server: ", host, ":", port);
    LOGD("✓ Redis connection status set to TRUE");
    LOGD("✓ Channel: ", redisChannelName, ", Format: ", dataFormat, ", Channels: ", numDataChannels, ", Sample Rate: ", sampleRate);
    return true;
#else
    LOGE("Redis support not compiled. Install hiredis library and recompile.");
    LOGE("To enable Redis: install libhiredis-dev and recompile with REDIS_ENABLED=1");
    return false;
#endif
}

void RedisDataThread::disconnectFromRedis()
{
#ifdef REDIS_ENABLED
    if (redisCtx)
    {
        redisFree(redisCtx);
        redisCtx = nullptr;
    }
#endif
    connectionStatus = false;
}

bool RedisDataThread::isConnected() const
{
    return connectionStatus.load();
}

void RedisDataThread::setRedisHost(const String& host)
{
    if (redisHost != host)
    {
        redisHost = host;
        LOGD("Redis host changed to: ", host);
    }
}

void RedisDataThread::setRedisPort(int port)
{
    if (redisPort != port)
    {
        redisPort = port;
        LOGD("Redis port changed to: ", port);
    }
}

void RedisDataThread::setRedisPassword(const String& password)
{
    if (redisPassword != password)
    {
        redisPassword = password;
        LOGD("Redis password ", password.isEmpty() ? "cleared" : "updated");
    }
}

void RedisDataThread::setRedisChannel(const String& channel)
{
    if (redisChannelName != channel)
    {
        redisChannelName = channel;
        LOGD("Redis channel changed to: ", channel);
    }
}

void RedisDataThread::setSampleRate(float rate)
{
    if (sampleRate != rate)
    {
        sampleRate = rate;
        LOGD("Sample rate changed to: ", rate, " Hz");

        // Sample rate change affects the data stream configuration
        // Note: We don't need to resize buffers for sample rate changes,
        // but downstream processors need to be notified
    }
}

void RedisDataThread::setNumChannels(int channels)
{
    if (numDataChannels != channels)
    {
        numDataChannels = channels;
        LOGD("Number of channels changed to: ", channels);

        // Resize buffers to match new channel count
        resizeBuffers();
    }
}

void RedisDataThread::setDataFormat(const String& format)
{
    if (dataFormat != format)
    {
        dataFormat = format;
        LOGD("Data format changed to: ", format);

        // Auto-enable stream mode for BRANDBCI format
        if (format == "brandbci")
        {
            useStreamMode = true;
            LOGD("Auto-enabled stream mode for BRANDBCI format");
        }
    }
}

void RedisDataThread::setBufferSize(int size)
{
    if (validateBufferSize(size) && bufferSize != size)
    {
        bufferSize = size;
        LOGD("Buffer size changed to: ", size, " samples");

        // Resize buffers to match new buffer size
        resizeBuffers();
    }
    else if (!validateBufferSize(size))
    {
        LOGE("Invalid buffer size: ", size, ". Must be between 100 and 100000 samples.");
    }
}

String RedisDataThread::getConnectionStatus() const
{
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
    if (!connectionStatus.load() || redisCtx == nullptr)
    {
        LOGE("Cannot retrieve data: Redis not connected");
        return records;
    }

    // Limit the number of records to prevent memory issues
    const int maxSafeRecords = 100;
    if (numRecords > maxSafeRecords)
    {
        LOGD("Requested ", numRecords, " records, limiting to ", maxSafeRecords, " for memory safety");
        numRecords = maxSafeRecords;
    }

    LOGD("Retrieving latest ", numRecords, " records from Redis channel: ", redisChannelName, " (mode: ", useStreamMode ? "STREAM" : "LIST", ")");

    redisReply* reply = nullptr;

    if (useStreamMode)
    {
        // For streams, use XREVRANGE to get latest entries
        reply = (redisReply*)redisCommand(redisCtx,
            "XREVRANGE %s + - COUNT %d", redisChannelName.toRawUTF8(), numRecords);
    }
    else
    {
        // For lists, use LRANGE to get the latest records without removing them from the list
        // LRANGE key start stop - gets elements from start to stop (inclusive)
        // Use negative indices to get from the end: -1 is last element, -2 is second to last, etc.
        int startIndex = -(numRecords);
        int stopIndex = -1;

        reply = (redisReply*)redisCommand(redisCtx,
            "LRANGE %s %d %d", redisChannelName.toRawUTF8(), startIndex, stopIndex);
    }

    if (reply == nullptr)
    {
        LOGE("Redis command returned null reply");
        handleRedisError("Redis command failed");
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
                        // Check if this is Open Ephys format by looking for specific fields
                        bool hasOpenEphysFields = false;
                        String availableFields = "";

                        for (size_t j = 0; j < fieldsReply->elements; j += 2)
                        {
                            if (j + 1 < fieldsReply->elements)
                            {
                                String fieldName(fieldsReply->element[j]->str);
                                availableFields += fieldName + " ";

                                if (fieldName == "data_shape" || fieldName == "data_dtype" || fieldName == "n_samples")
                                {
                                    hasOpenEphysFields = true;
                                }
                            }
                        }

                        LOGD("Stream record ", i, " available fields: ", availableFields);
                        LOGD("Open Ephys format detected: ", hasOpenEphysFields ? "YES" : "NO");

                        // Hard lock: ignore Open Ephys records in View Data
                        if (hasOpenEphysFields)
                        {
                            LOGD("Ignoring Open Ephys-like record in hard lock mode");
                        }
                        else
                        {
                            // Look for legacy BRANDBCI or simple data fields
                            String targetField = selectedDataField.isNotEmpty() ? selectedDataField : "data";
                            bool foundData = false;

                            for (size_t j = 0; j < fieldsReply->elements; j += 2)
                            {
                                if (j + 1 < fieldsReply->elements &&
                                    fieldsReply->element[j]->type == REDIS_REPLY_STRING)
                                {
                                    String fieldName(fieldsReply->element[j]->str);

                                    // Only support the user-selected neural field (binary)
                                    if (fieldName == selectedDataField)
                                    {
                                        // Encode binary safely as Base64 JSON for accurate length & decoding in popup
                                        const char* bin = fieldsReply->element[j + 1]->str;
                                        size_t binLen = fieldsReply->element[j + 1]->len;

                                        var jsonObj = var(new DynamicObject());
                                        jsonObj.getDynamicObject()->setProperty("field", fieldName);
                                        jsonObj.getDynamicObject()->setProperty("data_dtype", dataType);
                                        jsonObj.getDynamicObject()->setProperty("n_channels", numDataChannels);
                                        jsonObj.getDynamicObject()->setProperty("n_samples", 1);
                                        jsonObj.getDynamicObject()->setProperty("data_bytes", (int)binLen);

                                        MemoryBlock mb(bin, binLen);
                                        String b64 = Base64::toBase64(mb.getData(), mb.getSize());
                                        jsonObj.getDynamicObject()->setProperty("_binary_b64", b64);

                                        String jsonRecord = JSON::toString(jsonObj);
                                        records.add(jsonRecord);
                                        LOGD("Record ", i, " field: ", fieldName, " (base64-wrapped), bytes=", (int)binLen, ", dtype=", dataType);
                                        foundData = true;
                                        break;
                                    }
                                }
                            }

                            // If no data field found, skip this record to avoid empty entries
                            if (!foundData)
                            {
                                LOGD("Skipping record ", i, " - no valid data field found (available fields: ", availableFields, ")");
                                // Don't add empty records to avoid "JSON but not object" entries in View Data
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
                    String record(reply->element[i]->str, reply->element[i]->len);
                    records.add(record);
                    LOGD("List record ", i, " length: ", record.length(), " bytes");
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

    freeReplyObject(reply);

#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
#endif

    LOGD("Returning ", records.size(), " records");
    return records;
}

Array<String> RedisDataThread::getAvailableChannels()
{
    Array<String> channels;

#ifdef REDIS_ENABLED
    if (!connectionStatus.load() || redisCtx == nullptr)
    {
        LOGD("Cannot get channels: not connected to Redis");
        return channels;
    }

    LOGD("Scanning Redis for available channels/streams...");

    // First, get current database info
    redisReply* dbSizeReply = (redisReply*)redisCommand(redisCtx, "DBSIZE");
    if (dbSizeReply && dbSizeReply->type == REDIS_REPLY_INTEGER)
    {
        LOGD("Current database has ", dbSizeReply->integer, " keys");
    }
    if (dbSizeReply) freeReplyObject(dbSizeReply);

    // Scan all databases (0-15 are typical)
    for (int db = 0; db < 16; db++)
    {
        // Select database
        redisReply* selectReply = (redisReply*)redisCommand(redisCtx, "SELECT %d", db);
        if (!selectReply || selectReply->type == REDIS_REPLY_ERROR)
        {
            if (selectReply) freeReplyObject(selectReply);
            continue; // Skip this database
        }
        freeReplyObject(selectReply);

        // Get all keys in this database
        redisReply* reply = (redisReply*)redisCommand(redisCtx, "KEYS *");

        if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY)
        {
            if (reply) freeReplyObject(reply);
            continue;
        }

        if (reply->elements > 0)
        {
            LOGD("Database ", db, " has ", reply->elements, " keys");

            for (size_t i = 0; i < reply->elements; i++)
            {
                if (reply->element[i]->type == REDIS_REPLY_STRING)
                {
                    String keyName = String(reply->element[i]->str);

                    // Check if this key is a stream or list
                    redisReply* typeReply = (redisReply*)redisCommand(redisCtx, "TYPE %s", keyName.toRawUTF8());
                    if (typeReply && (typeReply->type == REDIS_REPLY_STRING || typeReply->type == REDIS_REPLY_STATUS))
                    {
                        String keyType = String(typeReply->str);
                        String dbPrefix = (db == 0) ? "" : "db" + String(db) + ":";

                        if (keyType == "stream")
                        {
                            // It's a stream, check if it has data
                            redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "XLEN %s", keyName.toRawUTF8());
                            if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
                            {
                                if (lenReply->integer > 0)
                                {
                                    channels.add(dbPrefix + keyName + " (stream, " + String(lenReply->integer) + " entries)");
                                    LOGD("Found stream in db", db, ": ", keyName, " with ", lenReply->integer, " entries");
                                }
                                else
                                {
                                    channels.add(dbPrefix + keyName + " (stream, empty)");
                                    LOGD("Found empty stream in db", db, ": ", keyName);
                                }
                            }
                            if (lenReply) freeReplyObject(lenReply);
                        }
                        else if (keyType == "list")
                        {
                            // It's a list, check if it has data
                            redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", keyName.toRawUTF8());
                            if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
                            {
                                if (lenReply->integer > 0)
                                {
                                    channels.add(dbPrefix + keyName + " (list, " + String(lenReply->integer) + " items)");
                                    LOGD("Found list in db", db, ": ", keyName, " with ", lenReply->integer, " items");
                                }
                                else
                                {
                                    channels.add(dbPrefix + keyName + " (list, empty)");
                                    LOGD("Found empty list in db", db, ": ", keyName);
                                }
                            }
                            if (lenReply) freeReplyObject(lenReply);
                        }
                    }
                    if (typeReply) freeReplyObject(typeReply);
                }
            }
        }

        freeReplyObject(reply);
    }

    // Return to database 0 (default)
    redisReply* selectDefaultReply = (redisReply*)redisCommand(redisCtx, "SELECT 0");
    if (selectDefaultReply) freeReplyObject(selectDefaultReply);

    // Sort channels alphabetically
    channels.sort();

    LOGD("Found ", channels.size(), " total available channels/streams across all databases");

#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
#endif

    return channels;
}

bool RedisDataThread::startAcquisition()
{
    LOGD("=== STARTING REDIS ACQUISITION ===");
    LOGD("Connection status: ", connectionStatus.load());
    LOGD("Current acquiring status: ", isAcquiring.load());
    LOGD("Redis host: ", redisHost, ":", redisPort);
    LOGD("Redis channel: ", redisChannelName);
    LOGD("Data format: ", dataFormat);
    LOGD("Expected channels: ", numDataChannels);
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

    // Reset sample counter and stream position
    currentSampleNumber = 0;

    // Set stream position based on alwaysReadLatest mode
    if (alwaysReadLatest)
    {
        currentStreamId = "$";  // Start from latest data to avoid processing historical backlog
        LOGD("Reset sample counter to 0 and stream position to latest ($) for always-read-latest mode");
    }
    else
    {
        currentStreamId = "0";  // Start from beginning to process all available data
        LOGD("Reset sample counter to 0 and stream position to beginning (0) for sequential mode");
    }

    // Test Redis connection before starting
    LOGD("Testing Redis connection before starting acquisition...");
#ifdef REDIS_ENABLED
    if (redisCtx)
    {
        redisReply* testReply = (redisReply*)redisCommand(redisCtx, "PING");
        if (testReply && testReply->type != REDIS_REPLY_ERROR)
        {
            LOGD("✓ Redis PING test successful");
            freeReplyObject(testReply);
        }
        else
        {
            LOGE("❌ Redis PING test failed");
            if (testReply) freeReplyObject(testReply);
            return false;
        }

        // Check data availability based on mode
        if (useStreamMode)
        {
            // For streams, check if stream exists and has data
            redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "XLEN %s", redisChannelName.toRawUTF8());
            if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
            {
                LOGD("✓ Redis stream '", redisChannelName, "' has ", lenReply->integer, " entries available");
                freeReplyObject(lenReply);
            }
            else if (lenReply && lenReply->type == REDIS_REPLY_ERROR)
            {
                LOGD("✓ Redis stream '", redisChannelName, "' does not exist yet (will be created when data arrives)");
                freeReplyObject(lenReply);
            }
            else
            {
                LOGE("❌ Cannot check Redis stream length");
                if (lenReply) freeReplyObject(lenReply);
            }
        }
        else
        {
            // For lists, use LLEN
            redisReply* lenReply = (redisReply*)redisCommand(redisCtx, "LLEN %s", redisChannelName.toRawUTF8());
            if (lenReply && lenReply->type == REDIS_REPLY_INTEGER)
            {
                LOGD("✓ Redis channel '", redisChannelName, "' has ", lenReply->integer, " samples available");
                freeReplyObject(lenReply);
            }
            else
            {
                LOGE("❌ Cannot check Redis channel length");
                if (lenReply) freeReplyObject(lenReply);
            }
        }
    }
    else
    {
        LOGE("❌ Redis context is null");
        return false;
    }
#endif

    isAcquiring = true;

    // Start the data thread - this is critical!
    LOGD("🚀 Starting data thread...");
    if (!isThreadRunning()) {
        startThread();
        LOGD("✅ Data thread started successfully");
    } else {
        LOGD("⚠️ Data thread was already running");
    }

    LOGD("✅ Redis data acquisition started successfully");
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

    // Stop the data thread
    LOGD("🛑 Stopping data thread...");
    if (isThreadRunning()) {
        signalThreadShouldExit();
        waitForThreadToExit(5000); // Wait up to 5 seconds
        LOGD("✅ Data thread stopped successfully");
    } else {
        LOGD("⚠️ Data thread was not running");
    }

    LOGD("✅ Redis data acquisition stopped");
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
    for (int ch = 0; ch < numDataChannels; ch++)
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
    // Use configurable buffer size
    DataBuffer* buffer = new DataBuffer(numDataChannels, bufferSize);
    sourceBuffers.add(buffer);

    LOGD("Redis DataThread buffers resized: ", numDataChannels, " channels, buffer size: ", bufferSize);

    // Calculate and log latency information
    if (sampleRate > 0) {
        float latencyMs = (bufferSize / sampleRate) * 1000.0f;
        LOGD("Buffer latency: ", latencyMs, " ms at ", sampleRate, " Hz sample rate");
    }
}

bool RedisDataThread::updateBuffer()
{
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
        if (callCount <= 5) LOGE("updateBuffer: Redis not connected, returning false");
        return false;
    }

#ifdef REDIS_ENABLED
    // Periodic status logging (every 5 seconds)
    int64 currentTime = Time::getMillisecondCounterHiRes();
    if (currentTime - lastLogTime > 5000) // 5 seconds
    {
        int64 samplesDelta = samplesProcessed - lastSampleCount;
        double rate = samplesDelta / 5.0; // samples per second
        LOGD("Redis Status: processed=", samplesProcessed, " samples, rate=", rate, " samples/sec, acquiring=", isAcquiring.load(), ", connected=", connectionStatus.load());
        LOGD("  Mode: ", useStreamMode ? "STREAMS" : "LISTS", ", Stream: ", redisChannelName);
        lastLogTime = currentTime;
        lastSampleCount = samplesProcessed;
    }

    // Route to appropriate update method based on mode
    LOGD("updateBuffer: Using ", useStreamMode ? "STREAM" : "LIST", " mode");
    bool result = useStreamMode ? updateBufferFromStream() : updateBufferFromList();
    if (result) {
        samplesProcessed++;
    }
    return result;

    return false; // This should not be reached
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::updateBufferFromList()
{
#ifdef REDIS_ENABLED
    // Log BLPOP command execution
    LOGD("Executing BLPOP command on channel: ", redisChannelName);

    // Get data from Redis (blocking call with 1 second timeout)
    redisReply* reply = (redisReply*)redisCommand(redisCtx,
        "BLPOP %s 1", redisChannelName.toRawUTF8());

    if (reply == nullptr)
    {
        LOGE("BLPOP command returned null reply");
        handleRedisError("BLPOP command failed");
        return false;
    }

    LOGD("BLPOP reply type: ", reply->type, ", elements: ", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        LOGD("BLPOP timeout - no data available in Redis channel");
        freeReplyObject(reply);
        return true; // Not an error
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2)
    {
        LOGD("BLPOP returned array with 2 elements - processing data");

        // Parse data based on format
        Array<float> channelData;
        String dataStr(reply->element[1]->str, reply->element[1]->len);

        LOGD("Data format: ", dataFormat, ", data length: ", dataStr.length(), " bytes");
        LOGD("Raw data preview (first 100 chars): ", dataStr.substring(0, 100));

        // Hard lock: list mode not supported for non-stream formats anymore
        success = false;
        LOGE("List mode parsing disabled in hard lock configuration");

        if (success && channelData.size() == numDataChannels)
        {
            LOGD("Channel data validated: ", channelData.size(), " channels (expected: ", numDataChannels, ")");

            // Log sample data range
            float minVal = *std::min_element(channelData.begin(), channelData.end());
            float maxVal = *std::max_element(channelData.begin(), channelData.end());
            LOGD("Sample data range: [", minVal, ", ", maxVal, "] µV");

            // Prepare timestamp and sample number
            int64 sampleNumber = currentSampleNumber.fetch_add(1);
            double timestamp = Time::getMillisecondCounterHiRes() / 1000.0;
            uint64 eventCode = 0;

            LOGD("Adding to buffer: sampleNumber=", sampleNumber, ", timestamp=", timestamp);

            // Add to DataBuffer
            DataBuffer* buffer = getBufferAddress(0);
            if (buffer)
            {
                LOGD("DataBuffer found, attempting to add data");
                int written = buffer->addToBuffer(channelData.getRawDataPointer(),
                                                &sampleNumber,
                                                &timestamp,
                                                &eventCode,
                                                1);
                success = (written > 0);
                LOGD("Buffer write result: written=", written, ", success=", success);

                if (success)
                {
                    currentSampleNumber++;
                    if (currentSampleNumber.load() % 1000 == 0) // Log every 1000 samples
                    {
                        LOGD("Successfully processed ", currentSampleNumber.load(), " samples total");
                    }
                }
                else
                {
                    LOGE("Failed to write to DataBuffer");
                }
            }
            else
            {
                LOGE("DataBuffer is null - cannot add data to buffer");
                success = false;
            }
        }
        else if (!success)
        {
            LOGE("Data parsing failed for format: ", dataFormat);
        }
        else
        {
            LOGE("Channel count mismatch: got ", channelData.size(), " channels, expected ", numDataChannels);
        }
    }
    else
    {
        LOGE("Unexpected BLPOP reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
    }

    freeReplyObject(reply);
    LOGD("updateBufferFromList returning: ", success);
    return success;
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}

bool RedisDataThread::updateBufferFromStream()
{
#ifdef REDIS_ENABLED
    if (redisChannelName.isEmpty())
    {
        LOGE("Stream name is empty - cannot read from stream");
        return false;
    }

    redisReply* reply = nullptr;

    if (alwaysReadLatest)
    {
        // For always-read-latest mode, use XREAD with $ to wait for new data
        LOGD("Waiting for new data from stream: ", redisChannelName, " (mode: LATEST, waiting for new data)");
        reply = (redisReply*)redisCommand(redisCtx,
            "XREAD BLOCK 1000 STREAMS %s $",
            redisChannelName.toRawUTF8());
    }
    else
    {
        // For sequential mode, use XREAD with the last known ID
        LOGD("Reading from stream: ", redisChannelName, " with ID: ", currentStreamId, " (mode: SEQUENTIAL)");
        reply = (redisReply*)redisCommand(redisCtx,
            "XREAD BLOCK 1000 STREAMS %s %s",
            redisChannelName.toRawUTF8(),
            currentStreamId.toRawUTF8());
    }

    if (reply == nullptr)
    {
        LOGE("Redis command returned null reply");
        handleRedisError("Redis command failed");
        return false;
    }

    LOGD("Redis reply type: ", reply->type, ", elements: ", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));

    if (reply->type == REDIS_REPLY_NIL)
    {
        // Timeout, no data available
        LOGD("Redis timeout - no data available in stream");
        freeReplyObject(reply);
        return true; // Not an error
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
    {
        // Both modes now use XREAD, so use the same processing logic
        LOGD("XREAD returned array with ", reply->elements, " stream(s) - processing data");

        // Process the stream data
        redisReply* streamReply = reply->element[0];
        if (streamReply->type == REDIS_REPLY_ARRAY && streamReply->elements == 2)
        {
            // Get stream name and entries
            String receivedStreamName(streamReply->element[0]->str);
            redisReply* entriesReply = streamReply->element[1];

            LOGD("Processing stream: ", receivedStreamName, " with ", entriesReply->elements, " entries");

            // Process each entry in the stream
            for (size_t entryIdx = 0; entryIdx < entriesReply->elements; entryIdx++)
            {
                redisReply* entryReply = entriesReply->element[entryIdx];
                if (entryReply->type == REDIS_REPLY_ARRAY && entryReply->elements >= 2)
                {
                    // Update stream position only if not in always-read-latest mode
                    if (!alwaysReadLatest)
                    {
                        currentStreamId = String(entryReply->element[0]->str);
                        LOGD("Updated stream position to: ", currentStreamId);
                    }
                    else
                    {
                        LOGD("Processing new data in always-latest mode (not updating position)");
                    }

                    // Get field-value pairs
                    redisReply* fieldsReply = entryReply->element[1];
                    if (fieldsReply->type == REDIS_REPLY_ARRAY)
                    {
                        success = processStreamEntry(fieldsReply);
                        if (success) break; // Process one entry at a time
                    }
                }
            }
        }
    }
    else
    {
        LOGE("Unexpected XREAD reply format: type=", reply->type, ", elements=", (reply->type == REDIS_REPLY_ARRAY ? reply->elements : 0));
    }

    freeReplyObject(reply);
    LOGD("updateBufferFromStream returning: ", success);
    return success;
#else
    LOGE("Redis not compiled - REDIS_ENABLED not defined");
    return false;
#endif
}


bool RedisDataThread::processStreamEntry(redisReply* fieldsReply)
{
#ifdef REDIS_ENABLED
    // Hard lock: only support neural_data_simulator-style binary field selected by configuration.
    // Ignore Open Ephys and other formats.
    return processLegacyStreamEntry(fieldsReply);
#else
    return false;
#endif
}

bool RedisDataThread::processLegacyStreamEntry(redisReply* fieldsReply)
{
#ifdef REDIS_ENABLED
    // Legacy processing logic (original implementation)
    for (size_t i = 0; i < fieldsReply->elements; i += 2)
    {
        if (i + 1 < fieldsReply->elements)
        {
            String fieldName(fieldsReply->element[i]->str);
            redisReply* fieldReply = fieldsReply->element[i + 1];

            LOGD("Legacy stream field: ", fieldName);

            // Hard lock: only handle the configured binary field for neural_data_simulator format
            if (fieldName == selectedDataField)
            {
                Array<float> channelData;
                bool parseSuccess = parseBinarySpikeRates(fieldReply, channelData);

                if (parseSuccess && channelData.size() == numDataChannels)
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
                                LOGD("Added ", numDataChannels, " channels from spike_rates to buffer, sample #", currentSampleNumber.load());
                                return true;
                            }
                            else
                            {
                                LOGE("Failed to write spike_rates to DataBuffer");
                            }
                        }
                    }
                }
                else if (!parseSuccess)
                {
                    LOGE("Failed to parse binary spike_rates data");
                }
                else
                {
                    LOGE("Channel count mismatch in spike_rates: got ", channelData.size(), " channels, expected ", numDataChannels);
                }
            }
            // In hard lock mode, ignore all text-based fields
            else if (fieldName == "brandbci_data" || fieldName == "data")
            {
                LOGD("Ignoring text-based field '", fieldName, "' in hard lock mode");
                // Skip to next field
            }
        }
    }

    return false;
#else
    return false;
#endif
}

bool RedisDataThread::parseBrandBCIData(const String& jsonStr, Array<float>& channelData)
{
    // Hard lock cleanup: JSON/BRANDBCI parsing removed
    return false;
}

bool RedisDataThread::parseBinarySpikeRates(redisReply* fieldReply, Array<float>& channelData)
{
#ifdef REDIS_ENABLED
    if (fieldReply == nullptr || fieldReply->type != REDIS_REPLY_STRING)
    {
        LOGE("Invalid field reply for spike_rates parsing");
        return false;
    }

    // Get binary data from Redis reply
    const char* binaryData = fieldReply->str;
    size_t dataLength = fieldReply->len;

    LOGD("Parsing binary spike_rates data, length: ", dataLength, " bytes");

    if (dataLength == 0)
    {
        LOGE("Empty spike_rates data");
        return false;
    }

    // Decode according to configured dataType. One sample per record, N = numDataChannels.
    auto bytesPerElementFor = [](const String& dtype) -> int {
        if (dtype == "float32" || dtype == "<f4") return sizeof(float);
        if (dtype == "float64" || dtype == "<f8") return sizeof(double);
        if (dtype == "int16"  || dtype == "<i2") return sizeof(int16);
        if (dtype == "int32"  || dtype == "<i4") return sizeof(int32);
        if (dtype == "uint16" || dtype == "<u2") return sizeof(uint16);
        // Fallback to int16
        return (int)sizeof(int16);
    };

    const int bpe = bytesPerElementFor(dataType);
    if (dataLength % bpe != 0)
    {
        LOGE("spike_rates byte-length (", (int)dataLength, ") not a multiple of element size (", bpe, ") for dtype '", dataType, "'");
        return false;
    }

    const int elements = (int)(dataLength / bpe);
    if (elements != numDataChannels)
    {
        LOGE("Channel count mismatch in spike_rates: bytes=", (int)dataLength,
             ", dtype=", dataType, " (", bpe, "/ch), decoded elements=", elements,
             ", expected ", numDataChannels);
        return false;
    }

    channelData.clear();
    channelData.ensureStorageAllocated(elements);

    if (dataType == "float32" || dataType == "<f4")
    {
        const float* ptr = reinterpret_cast<const float*>(binaryData);
        for (int i = 0; i < elements; ++i) channelData.add(ptr[i]);
    }
    else if (dataType == "float64" || dataType == "<f8")
    {
        const double* ptr = reinterpret_cast<const double*>(binaryData);
        for (int i = 0; i < elements; ++i) channelData.add((float)ptr[i]);
    }
    else if (dataType == "int16" || dataType == "<i2")
    {
        const int16* ptr = reinterpret_cast<const int16*>(binaryData);
        for (int i = 0; i < elements; ++i) channelData.add((float)ptr[i]);
    }
    else if (dataType == "int32" || dataType == "<i4")
    {
        const int32* ptr = reinterpret_cast<const int32*>(binaryData);
        for (int i = 0; i < elements; ++i) channelData.add((float)ptr[i]);
    }
    else if (dataType == "uint16" || dataType == "<u2")
    {
        const uint16* ptr = reinterpret_cast<const uint16*>(binaryData);
        for (int i = 0; i < elements; ++i) channelData.add((float)ptr[i]);
    }
    else
    {
        LOGE("Unsupported configured dataType for spike_rates: ", dataType);
        return false;
    }

    LOGD("Successfully parsed ", channelData.size(), " spike rate channels using dtype '", dataType, "'");

    // Log some sample values for debugging
    if (channelData.size() > 0)
    {
        float avgRate = 0.0f;
        int activeChannels = 0;
        for (int i = 0; i < channelData.size(); i++)
        {
            avgRate += channelData[i];
            if (channelData[i] > 0) activeChannels++;
        }
        avgRate /= channelData.size();

        LOGD("Spike rates - Average: ", avgRate, ", Active channels: ", activeChannels, "/", channelData.size());

        // Log first few values
        String sampleValues = "First 10 values: ";
        for (int i = 0; i < jmin(10, channelData.size()); i++)
        {
            sampleValues += String(channelData[i]) + " ";
        }
        LOGD(sampleValues);
    }

    return true;
#else
    return false;
#endif
}

void RedisDataThread::handleRedisError(const String& operation)
{
#ifdef REDIS_ENABLED
    if (redisCtx && redisCtx->err)
    {
        LOGE("Redis error in ", operation, ": ", redisCtx->errstr);

        // Try to reconnect on connection errors
        if (redisCtx->err == REDIS_ERR_IO || redisCtx->err == REDIS_ERR_EOF)
        {
            attemptReconnection();
        }
    }
    else
    {
        LOGE("Redis error in ", operation, ": unknown error");
    }
#endif
}

bool RedisDataThread::attemptReconnection()
{
    LOGD("Attempting to reconnect to Redis...");

    // Disconnect and try to reconnect
    disconnectFromRedis();

    if (connectToRedis(redisHost, redisPort, redisPassword))
    {
        // Reset stream position to latest data after reconnection
        // This ensures we don't process stale data that accumulated during disconnection
        if (useStreamMode)
        {
            currentStreamId = "$";
            LOGD("Reset stream position to latest ($) after reconnection");
        }

        LOGD("Redis reconnection successful");
        return true;
    }
    else
    {
        LOGE("Redis reconnection failed");
        return false;
    }
}

// Stream management methods
void RedisDataThread::setStreamMode(bool useStreams)
{
    if (useStreamMode != useStreams)
    {
        useStreamMode = useStreams;
        LOGD("Stream mode changed to: ", useStreams ? "ENABLED" : "DISABLED");

        if (useStreams && dataFormat == "json")
        {
            // Auto-switch to brandbci format for better stream support
            dataFormat = "brandbci";
            LOGD("Auto-switched data format to 'brandbci' for stream mode");
        }
    }
}

void RedisDataThread::setAlwaysReadLatest(bool alwaysLatest)
{
    if (alwaysReadLatest != alwaysLatest)
    {
        alwaysReadLatest = alwaysLatest;
        LOGD("Always read latest mode changed to: ", alwaysLatest ? "ENABLED" : "DISABLED");

        // Reset stream position when switching modes
        if (alwaysLatest)
        {
            currentStreamId = "$";
            LOGD("Reset stream position to latest ($) for always-read-latest mode");
        }
    }
}







Array<int> RedisDataThread::parseDataShape(const String& shapeStr)
{
    Array<int> shape;

    try
    {
        // Parse JSON format shape information, e.g., "[1000, 96]"
        var shapeJson = JSON::parse(shapeStr);
        if (shapeJson.isArray())
        {
            for (int i = 0; i < shapeJson.size(); i++)
            {
                if (shapeJson[i].isInt())
                {
                    int dimension = (int)shapeJson[i];
                    if (dimension > 0)
                    {
                        shape.add(dimension);
                    }
                    else
                    {
                        LOGE("Invalid dimension value: ", dimension);
                        return Array<int>(); // Return empty array for error
                    }
                }
            }
        }
        else
        {
            LOGE("Data shape is not a JSON array: ", shapeStr);
        }
    }
    catch (const std::exception& e)
    {
        LOGE("Failed to parse data shape JSON: ", shapeStr, " - ", e.what());
    }
    catch (...)
    {
        LOGE("Unknown error parsing data shape: ", shapeStr);
    }

    // Validate shape reasonableness
    if (shape.size() == 2)
    {
        LOGD("Parsed data shape: [", shape[0], ", ", shape[1], "]");
    }
    else
    {
        LOGE("Invalid shape dimensions: expected 2D, got ", shape.size(), "D");
        return Array<int>(); // Return empty array for error
    }

    return shape;
}

size_t RedisDataThread::calculateExpectedDataSize(const String& dtype, const Array<int>& shape)
{
    int totalElements = 1;
    for (int dim : shape)
    {
        totalElements *= dim;
    }

    size_t elementSize = 0;
    if (dtype == "float32" || dtype == "<f4")
    {
        elementSize = sizeof(float);
    }
    else if (dtype == "float64" || dtype == "<f8")
    {
        elementSize = sizeof(double);
    }
    else if (dtype == "int16" || dtype == "<i2")
    {
        elementSize = sizeof(int16);
    }
    else if (dtype == "int32" || dtype == "<i4")
    {
        elementSize = sizeof(int32);
    }
    else if (dtype == "uint16" || dtype == "<u2")
    {
        elementSize = sizeof(uint16);
    }

    return totalElements * elementSize;
}

bool RedisDataThread::decodeBinaryData(const char* data, size_t length, const String& dtype,
                                     const Array<int>& shape, Array<float>& output)
{
    output.clear();

    // Calculate expected number of elements
    int totalElements = 1;
    for (int dim : shape)
    {
        totalElements *= dim;
    }

    LOGD("Decoding binary data: type=", dtype, ", elements=", totalElements, ", bytes=", length);

    // Route to appropriate decoder based on data type
    if (dtype == "float32" || dtype == "<f4")
    {
        return decodeFloat32(data, length, totalElements, output);
    }
    else if (dtype == "float64" || dtype == "<f8")
    {
        return decodeFloat64(data, length, totalElements, output);
    }
    else if (dtype == "int16" || dtype == "<i2")
    {
        return decodeInt16(data, length, totalElements, output);
    }
    else if (dtype == "int32" || dtype == "<i4")
    {
        return decodeInt32(data, length, totalElements, output);
    }
    else if (dtype == "uint16" || dtype == "<u2")
    {
        return decodeUInt16(data, length, totalElements, output);
    }
    else
    {
        LOGE("Unsupported data type: ", dtype);
        return false;
    }
}

bool RedisDataThread::decodeFloat32(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    // Validate data length
    if (length != expectedElements * sizeof(float))
    {
        LOGE("Float32 data length mismatch: expected ",
             expectedElements * sizeof(float), " bytes, got ", length);
        return false;
    }

    // Direct copy (most efficient for float32)
    const float* floatData = reinterpret_cast<const float*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(floatData[i]);
    }

    LOGD("Decoded ", expectedElements, " float32 values");
    return true;
}

bool RedisDataThread::decodeFloat64(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(double))
    {
        LOGE("Float64 data length mismatch: expected ",
             expectedElements * sizeof(double), " bytes, got ", length);
        return false;
    }

    const double* doubleData = reinterpret_cast<const double*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(doubleData[i]));
    }

    LOGD("Decoded ", expectedElements, " float64 values (converted to float32)");
    return true;
}

bool RedisDataThread::decodeInt16(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(int16))
    {
        LOGE("Int16 data length mismatch: expected ",
             expectedElements * sizeof(int16), " bytes, got ", length);
        return false;
    }

    const int16* intData = reinterpret_cast<const int16*>(data);
    output.ensureStorageAllocated(expectedElements);

    // Optional: apply scale factor (configurable)
    float scaleFactor = 1.0f; // Can be made configurable later

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(intData[i]) * scaleFactor);
    }

    LOGD("Decoded ", expectedElements, " int16 values with scale factor ", scaleFactor);
    return true;
}

bool RedisDataThread::decodeInt32(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(int32))
    {
        LOGE("Int32 data length mismatch: expected ",
             expectedElements * sizeof(int32), " bytes, got ", length);
        return false;
    }

    const int32* intData = reinterpret_cast<const int32*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(intData[i]));
    }

    LOGD("Decoded ", expectedElements, " int32 values");
    return true;
}

bool RedisDataThread::decodeUInt16(const char* data, size_t length, int expectedElements, Array<float>& output)
{
    if (length != expectedElements * sizeof(uint16))
    {
        LOGE("UInt16 data length mismatch: expected ",
             expectedElements * sizeof(uint16), " bytes, got ", length);
        return false;
    }

    const uint16* uintData = reinterpret_cast<const uint16*>(data);
    output.ensureStorageAllocated(expectedElements);

    for (int i = 0; i < expectedElements; i++)
    {
        output.add(static_cast<float>(uintData[i]));
    }

    LOGD("Decoded ", expectedElements, " uint16 values");
    return true;
}

bool RedisDataThread::addMultiSampleDataToBuffer(const Array<float>& channelData,
                                                int nChannels, int nSamples,
                                                double baseTimestamp, int sampleRate)
{
    // Validate data size
    if (channelData.size() != nChannels * nSamples)
    {
        LOGE("Channel data size mismatch: expected ", nChannels * nSamples,
             ", got ", channelData.size());
        return false;
    }

    if (sourceBuffers.size() == 0)
    {
        LOGE("No source buffers available");
        return false;
    }

    DataBuffer* buffer = sourceBuffers[0];
    if (!buffer)
    {
        LOGE("DataBuffer is null - cannot add data");
        return false;
    }

    LOGD("Processing ", nSamples, " samples with ", nChannels, " channels each");

    // For large data blocks, process in chunks to avoid buffer overflow
    const int MAX_CHUNK_SIZE = 5000; // Process max 5000 samples at a time

    if (nSamples > MAX_CHUNK_SIZE)
    {
        LOGD("Large data block detected (", nSamples, " samples). Processing in chunks of ", MAX_CHUNK_SIZE);

        int processedSamples = 0;
        while (processedSamples < nSamples)
        {
            int chunkSize = jmin(MAX_CHUNK_SIZE, nSamples - processedSamples);

            // Extract chunk data
            Array<float> chunkData;
            chunkData.ensureStorageAllocated(nChannels * chunkSize);

            for (int sample = 0; sample < chunkSize; sample++)
            {
                for (int ch = 0; ch < nChannels; ch++)
                {
                    int sourceIndex = (processedSamples + sample) * nChannels + ch;
                    chunkData.add(channelData[sourceIndex]);
                }
            }

            // Process chunk recursively
            double chunkTimestamp = baseTimestamp + (processedSamples / (double)sampleRate);
            if (!addMultiSampleDataToBuffer(chunkData, nChannels, chunkSize, chunkTimestamp, sampleRate))
            {
                LOGE("Failed to process chunk starting at sample ", processedSamples);
                return false;
            }

            processedSamples += chunkSize;
            LOGD("Processed chunk: ", processedSamples, "/", nSamples, " samples");
        }

        LOGD("Successfully processed all ", nSamples, " samples in chunks");
        return true;
    }

    // Process samples one by one for smaller blocks
    for (int sample = 0; sample < nSamples; sample++)
    {
        // Extract current sample's channel data
        Array<float> sampleData;
        sampleData.ensureStorageAllocated(nChannels);

        // Assume data format is [samples, channels] (row-major)
        for (int ch = 0; ch < nChannels; ch++)
        {
            int index = sample * nChannels + ch;
            sampleData.add(channelData[index]);
        }

        // Calculate timestamp (assuming uniform sampling)
        int64 sampleNumber = currentSampleNumber.load() + sample;
        double timestamp = baseTimestamp + (sample / (double)sampleRate);
        uint64 eventCode = 0;

        // Add to buffer
        int written = buffer->addToBuffer(sampleData.getRawDataPointer(),
                                        &sampleNumber,
                                        &timestamp,
                                        &eventCode,
                                        1);

        if (written <= 0)
        {
            LOGE("Failed to write sample ", sample, " to buffer");
            return false;
        }

        // Log progress every 100 samples
        if (sample % 100 == 0 && sample > 0)
        {
            LOGD("Processed ", sample, "/", nSamples, " samples");
        }
    }

    // Update sample counter
    currentSampleNumber += nSamples;

    LOGD("Successfully added ", nSamples, " samples to buffer. Total samples: ",
         currentSampleNumber.load());
    return true;
}

bool RedisDataThread::validateConfiguration() const
{
    // Validate host
    if (redisHost.isEmpty()) {
        LOGE("Configuration validation failed: Redis host cannot be empty");
        return false;
    }

    // Validate port
    if (redisPort <= 0 || redisPort > 65535) {
        LOGE("Configuration validation failed: Redis port must be between 1 and 65535, got: ", redisPort);
        return false;
    }

    // Validate channel name
    if (redisChannelName.isEmpty()) {
        LOGE("Configuration validation failed: Redis channel name cannot be empty");
        return false;
    }

    // Validate sample rate
    if (sampleRate <= 0.0f || sampleRate > 1000000.0f) {
        LOGE("Configuration validation failed: Sample rate must be between 0 and 1MHz, got: ", sampleRate);
        return false;
    }

    // Validate channel configuration
    if (!validateChannelConfiguration(numDataChannels)) {
        return false;
    }

    // Validate buffer size
    if (!validateBufferSize(bufferSize)) {
        return false;
    }

    // Validate stream name (use channel name as stream name in stream mode)
    if (useStreamMode && redisChannelName.isEmpty()) {
        LOGE("Configuration validation failed: Channel name cannot be empty in stream mode");
        return false;
    }

    // Validate data format
    if (dataFormat != "json" && dataFormat != "binary" && dataFormat != "brandbci") {
        LOGE("Configuration validation failed: Unsupported data format: ", dataFormat);
        return false;
    }

    LOGD("✓ Configuration validation passed");
    return true;
}

bool RedisDataThread::validateChannelConfiguration(int channels) const
{
    if (channels <= 0) {
        LOGE("Channel validation failed: Number of channels must be positive, got: ", channels);
        return false;
    }

    if (channels > maxDataChannels) {
        LOGE("Channel validation failed: Number of channels (", channels,
             ") exceeds maximum (", maxDataChannels, ")");
        return false;
    }

    // Check for reasonable channel counts based on common hardware
    if (channels > 512) {
        LOGD("Warning: Large number of channels (", channels, ") - ensure this is correct");
    }

    return true;
}

bool RedisDataThread::validateBufferSize(int size) const
{
    // Minimum buffer size: 100 samples (for very low latency applications)
    const int MIN_BUFFER_SIZE = 100;
    // Maximum buffer size: 100000 samples (for high throughput applications)
    const int MAX_BUFFER_SIZE = 100000;

    if (size < MIN_BUFFER_SIZE) {
        LOGE("Buffer size validation failed: Size (", size,
             ") is too small. Minimum is ", MIN_BUFFER_SIZE, " samples.");
        return false;
    }

    if (size > MAX_BUFFER_SIZE) {
        LOGE("Buffer size validation failed: Size (", size,
             ") is too large. Maximum is ", MAX_BUFFER_SIZE, " samples.");
        return false;
    }

    // Warn for very small buffer sizes that might cause performance issues
    if (size < 500) {
        LOGD("Warning: Small buffer size (", size, " samples) may cause high CPU usage");
    }

    // Warn for very large buffer sizes that might cause high latency
    if (size > 50000) {
        LOGD("Warning: Large buffer size (", size, " samples) will cause high latency");
    }

    // Recommend buffer size for large data blocks
    if (size < 30000) {
        LOGD("Info: For processing large data blocks (>10k samples), consider buffer size >= 30000");
    }

    return true;
}

void RedisDataThread::saveConfigurationToXml(XmlElement* parentElement)
{
    XmlElement* mainNode = parentElement->createNewChildElement("REDIS_DATA_THREAD");

    // Save connection settings
    mainNode->setAttribute("redisHost", redisHost);
    mainNode->setAttribute("redisPort", redisPort);
    mainNode->setAttribute("redisPassword", redisPassword);

    // Save stream settings
    mainNode->setAttribute("redisChannelName", redisChannelName);
    mainNode->setAttribute("useStreamMode", useStreamMode);

    // Save format settings
    mainNode->setAttribute("dataFormat", dataFormat);
    mainNode->setAttribute("sampleRate", sampleRate);
    mainNode->setAttribute("numDataChannels", numDataChannels);

    // Save advanced settings
    mainNode->setAttribute("bufferSize", bufferSize);
    mainNode->setAttribute("enableDataValidation", enableDataValidation);

    // Save field discovery settings
    mainNode->setAttribute("selectedDataField", selectedDataField);
    String processingMethod;
    switch (array2DProcessing)
    {
        case Array2DProcessing::FIRST_ROW: processingMethod = "first_row"; break;
        case Array2DProcessing::SUM: processingMethod = "sum"; break;
        case Array2DProcessing::MEAN: processingMethod = "mean"; break;
    }
    mainNode->setAttribute("array2DProcessing", processingMethod);

    // Save data type setting
    mainNode->setAttribute("dataType", dataType);

    LOGD("Redis configuration saved to XML");
}

void RedisDataThread::loadConfigurationFromXml(XmlElement* customParamsXml)
{
    if (customParamsXml == nullptr) return;

    // Find the Redis data thread node
    XmlElement* mainNode = customParamsXml->getChildByName("REDIS_DATA_THREAD");
    if (mainNode == nullptr) return;

    // Load connection settings
    redisHost = mainNode->getStringAttribute("redisHost", "localhost");
    redisPort = mainNode->getIntAttribute("redisPort", 6379);
    redisPassword = mainNode->getStringAttribute("redisPassword", "");

    // Load stream settings
    redisChannelName = mainNode->getStringAttribute("redisChannelName", "neural_data_primary");
    useStreamMode = mainNode->getBoolAttribute("useStreamMode", true);

    // Load format settings (hard lock: force brandbci)
    dataFormat = "brandbci";
    sampleRate = mainNode->getDoubleAttribute("sampleRate", 30000.0f);
    numDataChannels = mainNode->getIntAttribute("numDataChannels", 32);

    // Load advanced settings
    bufferSize = mainNode->getIntAttribute("bufferSize", 50000);
    enableDataValidation = mainNode->getBoolAttribute("enableDataValidation", true);

    // Load field discovery settings
    selectedDataField = mainNode->getStringAttribute("selectedDataField", "data");
    String processingMethod = mainNode->getStringAttribute("array2DProcessing", "first_row");
    if (processingMethod == "sum")
    {
        array2DProcessing = Array2DProcessing::SUM;
    }
    else if (processingMethod == "mean")
    {
        array2DProcessing = Array2DProcessing::MEAN;
    }
    else
    {
        array2DProcessing = Array2DProcessing::FIRST_ROW;
    }

    // Load data type setting
    dataType = mainNode->getStringAttribute("dataType", "int16");

    // Validate loaded configuration
    if (!validateConfiguration()) {
        LOGD("Loaded configuration validation failed - using safe defaults");
    }

    LOGD("Redis configuration loaded from XML:");
    LOGD("  - Host: ", redisHost, ":", redisPort);
    LOGD("  - Channel: ", redisChannelName);
    LOGD("  - Format: ", dataFormat);
    LOGD("  - Channels: ", numDataChannels);
    LOGD("  - Sample Rate: ", sampleRate);
}

// Field Discovery Implementation
Array<RedisDataThread::FieldInfo> RedisDataThread::discoverDataFields()
{
    Array<FieldInfo> fields;

    if (!isConnected())
    {
        LOGE("Cannot discover fields: not connected to Redis");
        return fields;
    }

    LOGD("Starting field discovery for channel: ", redisChannelName);

    try
    {
        // Get sample data from Redis with timeout protection
        String sampleData = getSampleDataFromRedis(3);
        if (sampleData.isEmpty())
        {
            LOGE("No sample data available for field discovery - channel may be empty or inactive");
            return fields;
        }

        // Analyze the sample data structure with error handling
        fields = analyzeFieldStructure(sampleData);

        if (fields.isEmpty())
        {
            LOGD("Field discovery completed but no suitable fields found");
        }
        else
        {
            LOGD("Field discovery completed. Found ", fields.size(), " fields");
            for (const auto& field : fields)
            {
                LOGD("  - ", field.fieldName, ": ", field.getDisplayName());
            }
        }
    }
    catch (const std::exception& e)
    {
        LOGE("Exception during field discovery: ", e.what());
    }
    catch (...)
    {
        LOGE("Unknown exception during field discovery");
    }

    return fields;
}

String RedisDataThread::getSampleDataFromRedis(int numSamples)
{
#ifdef REDIS_ENABLED
    if (!redisCtx || !isConnected())
    {
        LOGE("Redis not connected");
        return String();
    }

    String sampleData;

    if (useStreamMode)
    {
        // Use XREVRANGE to get recent entries from stream
        String command = "XREVRANGE " + redisChannelName + " + - COUNT " + String(numSamples);
        redisReply* reply = (redisReply*)redisCommand(redisCtx, command.toRawUTF8());

        if (!reply)
        {
            LOGE("Failed to execute XREVRANGE command");
            return String();
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
        {
            // Process the first entry for field discovery
            if (reply->element[0]->type == REDIS_REPLY_ARRAY && reply->element[0]->elements >= 2)
            {
                redisReply* entryData = reply->element[0]->element[1];
                if (entryData->type == REDIS_REPLY_ARRAY)
                {
                    // Convert field-value pairs to JSON for analysis
                    var jsonObj = var(new DynamicObject());

                    for (size_t i = 0; i < entryData->elements; i += 2)
                    {
                        if (i + 1 < entryData->elements)
                        {
                            String fieldName(entryData->element[i]->str);
                            redisReply* valueReply = entryData->element[i + 1];

                            // Handle binary data safely
                            if (fieldName == "data" && valueReply->len > 0)
                            {
                                // For binary data fields, create a placeholder
                                jsonObj.getDynamicObject()->setProperty(fieldName, "binary_data_placeholder");
                            }
                            else
                            {
                                // For text fields, convert safely
                                String fieldValue(valueReply->str, valueReply->len);
                                // Check if the string contains valid UTF-8
                                if (fieldValue.isNotEmpty() && fieldValue.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,[]{}:\"- "))
                                {
                                    jsonObj.getDynamicObject()->setProperty(fieldName, fieldValue);
                                }
                                else
                                {
                                    jsonObj.getDynamicObject()->setProperty(fieldName, "binary_data");
                                }
                            }
                        }
                    }

                    sampleData = JSON::toString(jsonObj);
                }
            }
        }

        freeReplyObject(reply);
    }
    else
    {
        // Use LRANGE to get recent list entries
        String command = "LRANGE " + redisChannelName + " -" + String(numSamples) + " -1";
        redisReply* reply = (redisReply*)redisCommand(redisCtx, command.toRawUTF8());

        if (!reply)
        {
            LOGE("Failed to execute LRANGE command");
            return String();
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0)
        {
            // Use the most recent entry
            sampleData = String(reply->element[reply->elements - 1]->str,
                              reply->element[reply->elements - 1]->len);
        }

        freeReplyObject(reply);
    }

    LOGD("Retrieved sample data: ", sampleData.substring(0, 200), "...");
    return sampleData;
#else
    LOGE("Redis not compiled - cannot get sample data");
    return String();
#endif
}

Array<RedisDataThread::FieldInfo> RedisDataThread::analyzeFieldStructure(const String& sampleData)
{
    Array<FieldInfo> fields;

    try
    {
        // Handle different data formats
        if (sampleData.isEmpty())
        {
            LOGD("Empty sample data for field analysis");
            return fields;
        }

        // Try to parse as JSON first
        var jsonData = JSON::parse(sampleData);
        if (!jsonData.isObject())
        {
            LOGD("Sample data is not a JSON object, trying alternative parsing");

            // For BRANDBCI format, create a default field entry
            if (dataFormat == "brandbci")
            {
                FieldInfo defaultField;
                defaultField.fieldName = "data";
                defaultField.dataType = FieldDataType::ARRAY_1D;
                defaultField.dimensions.add(numDataChannels);
                defaultField.isSuitableForNeural = true;
                defaultField.sampleData = "BRANDBCI format data (" + String(numDataChannels) + " channels)";
                fields.add(defaultField);
                return fields;
            }

            return fields;
        }

        DynamicObject* obj = jsonData.getDynamicObject();
        if (!obj)
        {
            LOGE("Failed to get DynamicObject from JSON");
            return fields;
        }

        // Analyze each field in the JSON object
        for (auto& property : obj->getProperties())
        {
            FieldInfo fieldInfo;
            fieldInfo.fieldName = property.name.toString();

            var value = property.value;

            // Analyze field type and dimensions
            if (value.isArray())
            {
                Array<var>* arrayValue = value.getArray();
                if (arrayValue && arrayValue->size() > 0)
                {
                    // Check if it's a 1D or 2D array
                    var firstElement = (*arrayValue)[0];
                    if (firstElement.isArray())
                    {
                        // 2D array
                        fieldInfo.dataType = FieldDataType::ARRAY_2D;
                        fieldInfo.dimensions.add(arrayValue->size()); // rows
                        Array<var>* firstRow = firstElement.getArray();
                        if (firstRow)
                        {
                            fieldInfo.dimensions.add(firstRow->size()); // columns
                        }
                        fieldInfo.isSuitableForNeural = true;
                    }
                    else if (firstElement.isDouble() || firstElement.isInt())
                    {
                        // 1D array of numbers
                        fieldInfo.dataType = FieldDataType::ARRAY_1D;
                        fieldInfo.dimensions.add(arrayValue->size());
                        fieldInfo.isSuitableForNeural = true;
                    }
                    else
                    {
                        // Array of non-numeric data
                        fieldInfo.dataType = FieldDataType::UNKNOWN;
                        fieldInfo.isSuitableForNeural = false;
                    }

                    // Generate sample data preview with safe handling
                    if (fieldInfo.isSuitableForNeural)
                    {
                        String preview = "Sample: [";
                        int previewCount = std::min(3, arrayValue->size()); // Reduced count for cleaner display
                        bool hasValidData = false;

                        for (int i = 0; i < previewCount; i++)
                        {
                            try
                            {
                                if (fieldInfo.dataType == FieldDataType::ARRAY_2D)
                                {
                                    Array<var>* row = (*arrayValue)[i].getArray();
                                    if (row && row->size() > 0)
                                    {
                                        double value = (double)(*row)[0];
                                        if (std::isfinite(value)) // Check for valid numeric value
                                        {
                                            if (hasValidData) preview += ", ";
                                            preview += String(value, 2); // Limit decimal places
                                            if (row->size() > 1) preview += "...";
                                            hasValidData = true;
                                        }
                                    }
                                }
                                else
                                {
                                    double value = (double)(*arrayValue)[i];
                                    if (std::isfinite(value)) // Check for valid numeric value
                                    {
                                        if (hasValidData) preview += ", ";
                                        preview += String(value, 2); // Limit decimal places
                                        hasValidData = true;
                                    }
                                }
                            }
                            catch (...)
                            {
                                // Skip invalid data entries
                                continue;
                            }
                        }

                        if (!hasValidData)
                        {
                            preview = "Binary data detected";
                        }
                        else
                        {
                            if (arrayValue->size() > previewCount) preview += "...";
                            preview += "]";
                        }
                        fieldInfo.sampleData = preview;
                    }
                }
            }
            else if (value.isDouble() || value.isInt())
            {
                // Scalar value
                fieldInfo.dataType = FieldDataType::SCALAR;
                fieldInfo.isSuitableForNeural = false;
                fieldInfo.sampleData = "Value: " + String((double)value);
            }
            else
            {
                // String or other type - check for binary data placeholders
                String valueStr = value.toString();
                if (valueStr == "binary_data_placeholder" || valueStr == "binary_data")
                {
                    // This is a binary data field, assume it's suitable for neural data
                    fieldInfo.dataType = FieldDataType::ARRAY_1D;
                    fieldInfo.dimensions.add(numDataChannels); // Use configured channel count
                    fieldInfo.isSuitableForNeural = true;
                    fieldInfo.sampleData = "Binary array data (" + String(numDataChannels) + " channels)";
                }
                else
                {
                    fieldInfo.dataType = FieldDataType::UNKNOWN;
                    fieldInfo.isSuitableForNeural = false;
                    fieldInfo.sampleData = "Type: " + valueStr.substring(0, 50);
                }
            }

            fields.add(fieldInfo);
        }
    }
    catch (const std::exception& e)
    {
        LOGE("Exception analyzing field structure: ", e.what());
    }
    catch (...)
    {
        LOGE("Unknown exception analyzing field structure");
    }

    return fields;
}

void RedisDataThread::setSelectedDataField(const String& fieldName)
{
    selectedDataField = fieldName;
    LOGD("Selected data field set to: ", selectedDataField);
}

void RedisDataThread::setArray2DProcessing(Array2DProcessing method)
{
    array2DProcessing = method;
    String methodName;
    switch (method)
    {
        case Array2DProcessing::FIRST_ROW: methodName = "First Row"; break;
        case Array2DProcessing::SUM: methodName = "Sum"; break;
        case Array2DProcessing::MEAN: methodName = "Mean"; break;
    }
    LOGD("2D array processing method set to: ", methodName);
}

