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

#ifndef __REDISDATATHREADSTUB_H_INCLUDED__
#define __REDISDATATHREADSTUB_H_INCLUDED__

#include <DataThreadHeaders.h>
#include <EditorHeaders.h>

/**
 * Stub implementation of RedisDataThread when hiredis is not available
 * 
 * This provides a placeholder that shows an error message when the user
 * tries to use the Redis plugin without the required dependencies.
 */
class TESTABLE RedisDataThreadStub : public DataThread
{
public:
    /** Constructor */
    RedisDataThreadStub(SourceNode* sn);

    /** Destructor */
    ~RedisDataThreadStub();

    /** Creates the custom editor for this DataThread */
    std::unique_ptr<GenericEditor> createEditor(SourceNode* sn) override;

    /** Always returns false - Redis not available */
    bool startAcquisition() override;

    /** Always returns true */
    bool stopAcquisition() override;

    /** Always returns false - Redis not available */
    bool foundInputSource() override;

    /** Always returns false - Redis not available */
    bool isReady() override;

    /** Always returns false - no data available */
    bool updateBuffer() override;

    /** Creates empty configuration */
    void updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                       OwnedArray<EventChannel>* eventChannels,
                       OwnedArray<SpikeChannel>* spikeChannels,
                       OwnedArray<DataStream>* sourceStreams,
                       OwnedArray<DeviceInfo>* devices,
                       OwnedArray<ConfigurationObject>* configurationObjects) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThreadStub);
};

/**
 * Stub editor that shows an error message
 */
class TESTABLE RedisDataThreadStubEditor : public GenericEditor
{
public:
    /** Constructor */
    RedisDataThreadStubEditor(GenericProcessor* parentNode);

    /** Destructor */
    ~RedisDataThreadStubEditor();

    /** Component interface */
    void paint(Graphics& g) override;
    void resized() override;

private:
    std::unique_ptr<Label> errorLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThreadStubEditor);
};

#endif // __REDISDATATHREADSTUB_H_INCLUDED__
