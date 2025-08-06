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

#ifndef __BRANDREDISDATATHREAD_H_INCLUDED__
#define __BRANDREDISDATATHREAD_H_INCLUDED__

#include "RedisDataThread.h"
#include <memory>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

/**
 * Enhanced Redis DataThread for BRANDBCI integration
 * 
 * This plugin extends the basic Redis functionality to support:
 * - Redis streams (XREAD operations)
 * - BRANDBCI-specific data formats
 * - Multi-stream management
 * - Node status monitoring
 * - Advanced error handling and reconnection
 */

struct StreamInfo {
    String streamName;
    String lastId;
    String dataFormat;
    float sampleRate;
    int channelCount;
    int64 lastActivity;
    bool isActive;
    
    StreamInfo() : lastId("0-0"), dataFormat("brandbci"), 
                   sampleRate(30000.0f), channelCount(96), 
                   lastActivity(0), isActive(false) {}
};

struct NodeState {
    String nodeId;
    String nickname;
    String status;
    int64 lastUpdate;
    int errorCount;
    
    NodeState() : status("UNKNOWN"), lastUpdate(0), errorCount(0) {}
};

struct BrandMetadata {
    String nodeNickname;
    String nodeStatus;
    String sessionId;
    String electrodeConfig;
    float amplifierGain;
    int64 timestamp;
    
    BrandMetadata() : amplifierGain(1000.0f), timestamp(0) {}
};

class BrandStreamManager;
class BrandDataParser;
class StatusMonitor;

class TESTABLE BrandRedisDataThread : public RedisDataThread
{
public:
    /** Constructor */
    BrandRedisDataThread(SourceNode* sn);

    /** Destructor */
    ~BrandRedisDataThread();

    /** Creates the custom editor for this DataThread */
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;

    /** Initializes data transfer */
    bool startAcquisition() override;

    /** Stops data transfer */
    bool stopAcquisition() override;

    /** Returns true if the data source is connected and ready */
    bool foundInputSource() override;

    /** Updates buffer with new data from Redis streams */
    bool updateBuffer() override;

    /** Stream management methods */
    Array<String> discoverStreams(const String& pattern = "*neural*");
    bool subscribeToStream(const String& streamName);
    bool unsubscribeFromStream(const String& streamName);
    Array<StreamInfo> getActiveStreams() const;

    /** BRANDBCI-specific methods */
    bool parseBrandData(const String& data, Array<float>& channelData, BrandMetadata& metadata);
    Array<NodeState> getNodeStates() const;
    String getSupergraphInfo() const;
    bool isNodeHealthy(const String& nodeId) const;

    /** Configuration methods */
    void setStreamPattern(const String& pattern);
    void setAutoDiscovery(bool enabled);
    void setMaxStreams(int maxStreams);
    void setStreamBufferSize(int bufferSize);

    /** Status and monitoring */
    String getConnectionStatus() const override;
    String getStreamStatus() const;
    int getActiveStreamCount() const;
    float getDataRate() const; // samples per second across all streams

private:
    // Enhanced connection management
    std::unique_ptr<BrandStreamManager> streamManager;
    std::unique_ptr<BrandDataParser> dataParser;
    std::unique_ptr<StatusMonitor> statusMonitor;

    // Stream configuration
    String streamPattern;
    bool autoDiscovery;
    int maxStreams;
    int streamBufferSize;

    // Active streams tracking
    std::unordered_map<String, StreamInfo> activeStreams;
    std::mutex streamsMutex;

    // Node status tracking
    std::unordered_map<String, NodeState> nodeStates;
    std::mutex nodeStatesMutex;

    // Performance monitoring
    std::atomic<int64> totalSamplesReceived;
    std::atomic<int64> lastRateCalculation;
    std::atomic<float> currentDataRate;

    // Threading
    std::atomic<bool> shouldStop;
    std::thread streamReaderThread;
    std::thread statusMonitorThread;

    // Internal methods
    void streamReaderLoop();
    void statusMonitorLoop();
    bool processStreamData(const String& streamName, const String& data);
    void updateStreamActivity(const String& streamName);
    void handleStreamError(const String& streamName, const String& error);
    void calculateDataRate();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrandRedisDataThread);
};

/**
 * Stream Manager for handling multiple Redis streams
 */
class BrandStreamManager
{
public:
    BrandStreamManager(redisContext* ctx);
    ~BrandStreamManager();

    Array<String> discoverStreams(const String& pattern);
    bool subscribeToStream(const String& streamName, const String& startId = "0-0");
    bool readStreamData(const String& streamName, String& data, String& newId);
    void unsubscribeFromStream(const String& streamName);
    bool isStreamActive(const String& streamName) const;

private:
    redisContext* redisCtx;
    std::unordered_map<String, String> streamPositions;
    std::mutex positionsMutex;
};

/**
 * Data Parser for BRANDBCI-specific formats
 */
class BrandDataParser
{
public:
    BrandDataParser();
    ~BrandDataParser();

    bool parseBrandFormat(const String& data, Array<float>& channels, BrandMetadata& metadata);
    bool parseJsonFormat(const String& data, Array<float>& channels);
    bool parseBinaryFormat(const char* data, size_t length, Array<float>& channels);
    bool validateDataFormat(const String& data);
    BrandMetadata extractMetadata(const String& data);

private:
    bool parseJsonObject(const var& jsonObj, Array<float>& channels, BrandMetadata& metadata);
    bool validateChannelData(const Array<float>& channels);
};

/**
 * Status Monitor for BRANDBCI node health
 */
class StatusMonitor
{
public:
    StatusMonitor(redisContext* ctx);
    ~StatusMonitor();

    void startMonitoring();
    void stopMonitoring();
    Array<NodeState> getNodeStates() const;
    String getSupergraphInfo() const;
    bool isNodeHealthy(const String& nodeId) const;

private:
    redisContext* redisCtx;
    std::atomic<bool> isMonitoring;
    std::thread monitorThread;
    std::unordered_map<String, NodeState> nodeStates;
    mutable std::mutex nodeStatesMutex;
    String supergraphData;
    mutable std::mutex supergraphMutex;

    void monitorLoop();
    void updateNodeStatus(const String& nodeId, const String& status);
    void updateSupergraph();
};

#endif // __BRANDREDISDATATHREAD_H_INCLUDED__
