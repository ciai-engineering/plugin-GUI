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

#ifdef REDIS_ENABLED
#include <hiredis/hiredis.h>
#endif

#include <atomic>
#include <memory>

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

    /** Redis connection management */
    bool connectToRedis(const String& host, int port, const String& password = "");
    void disconnectFromRedis();
    bool isConnected() const;

    /** Configuration setters */
    void setRedisChannel(const String& channel);
    void setSampleRate(float sampleRate);
    void setNumChannels(int numChannels);
    void setDataFormat(const String& format);

    /** Configuration getters */
    String getRedisHost() const { return redisHost; }
    int getRedisPort() const { return redisPort; }
    String getRedisChannel() const { return redisChannel; }
    float getSampleRate() const { return sampleRate; }
    int getNumChannels() const { return numChannels; }
    String getDataFormat() const { return dataFormat; }
    String getConnectionStatus() const;

private:
#ifdef REDIS_ENABLED
    redisContext* redisContext;
#else
    void* redisContext; // placeholder when Redis is not available
#endif

    // Connection parameters
    String redisHost;
    int redisPort;
    String redisPassword;
    String redisChannel;

    // Data configuration
    float sampleRate;
    int numChannels;
    String dataFormat; // "json", "binary"

    // State management
    std::atomic<bool> isAcquiring;
    std::atomic<bool> connectionStatus;

    // Sample counting
    std::atomic<int64> currentSampleNumber;

    // Data parsing methods
    bool parseJsonData(const String& jsonStr, Array<float>& channelData);
    bool parseBinaryData(const char* data, size_t length, Array<float>& channelData);

    // Error handling
    void handleRedisError(const String& operation);
    bool attemptReconnection();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThread);
};

#endif // __REDISDATATHREAD_H_INCLUDED__
