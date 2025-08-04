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

#include "RedisDataThreadStub.h"

RedisDataThreadStub::RedisDataThreadStub(SourceNode* sn)
    : DataThread(sn)
{
    LOGE("RedisDataThread: hiredis library not found. Please install hiredis and recompile.");
}

RedisDataThreadStub::~RedisDataThreadStub()
{
}

std::unique_ptr<GenericEditor> RedisDataThreadStub::createEditor(SourceNode* sn)
{
    std::unique_ptr<RedisDataThreadStubEditor> editor = std::make_unique<RedisDataThreadStubEditor>(sn);
    return std::move(editor);
}

bool RedisDataThreadStub::startAcquisition()
{
    LOGE("RedisDataThread: Cannot start acquisition - hiredis library not available");
    return false;
}

bool RedisDataThreadStub::stopAcquisition()
{
    return true;
}

bool RedisDataThreadStub::foundInputSource()
{
    return false;
}

bool RedisDataThreadStub::isReady()
{
    return false;
}

bool RedisDataThreadStub::updateBuffer()
{
    return false;
}

void RedisDataThreadStub::updateSettings(OwnedArray<ContinuousChannel>* continuousChannels,
                                         OwnedArray<EventChannel>* eventChannels,
                                         OwnedArray<SpikeChannel>* spikeChannels,
                                         OwnedArray<DataStream>* sourceStreams,
                                         OwnedArray<DeviceInfo>* devices,
                                         OwnedArray<ConfigurationObject>* configurationObjects)
{
    // Create empty configuration
    sourceStreams->clear();
    continuousChannels->clear();
    eventChannels->clear();
    spikeChannels->clear();
    devices->clear();
    configurationObjects->clear();
}

// Stub Editor Implementation

RedisDataThreadStubEditor::RedisDataThreadStubEditor(GenericProcessor* parentNode)
    : GenericEditor(parentNode)
{
    desiredWidth = 300;

    errorLabel = std::make_unique<Label>("Error Label", 
        "Redis DataThread requires the hiredis library.\n\n"
        "Please install hiredis and recompile:\n"
        "Ubuntu/Debian: sudo apt-get install libhiredis-dev\n"
        "macOS: brew install hiredis\n"
        "Windows: Download from https://github.com/redis/hiredis");
    
    errorLabel->setBounds(10, 30, 280, 120);
    errorLabel->setFont(Font("Small Text", 11, Font::plain));
    errorLabel->setColour(Label::textColourId, Colours::orange);
    errorLabel->setJustificationType(Justification::topLeft);
    addAndMakeVisible(errorLabel.get());
}

RedisDataThreadStubEditor::~RedisDataThreadStubEditor()
{
}

void RedisDataThreadStubEditor::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);

    g.setColour(Colours::red);
    g.setFont(Font("Small Text", 13, Font::bold));
    g.drawText("Redis DataThread (DISABLED)", 8, 5, 250, 20, Justification::left, false);
}

void RedisDataThreadStubEditor::resized()
{
    // Components are positioned with absolute coordinates
}
