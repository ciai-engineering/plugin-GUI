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

#include "RedisDataDisplayPopup.h"

RedisDataDisplayPopup::RedisDataDisplayPopup(const Array<String>& records, const String& dataFormat)
    : dataRecords(records)
    , format(dataFormat)
{
    setSize(600, 400);
    setupUI();
}

RedisDataDisplayPopup::~RedisDataDisplayPopup()
{
}

void RedisDataDisplayPopup::setupUI()
{
    // Title label
    titleLabel = std::make_unique<Label>("Title Label", "Latest Redis Data Records");
    titleLabel->setBounds(10, 10, getWidth() - 20, 25);
    titleLabel->setFont(FontOptions("Small Text", "Bold", 16));
    titleLabel->setJustificationType(Justification::centred);
    addAndMakeVisible(titleLabel.get());

    // Data text editor (read-only)
    dataTextEditor = std::make_unique<TextEditor>("Data Text Editor");
    dataTextEditor->setBounds(10, 45, getWidth() - 20, getHeight() - 55);
    dataTextEditor->setMultiLine(true);
    dataTextEditor->setReadOnly(true);
    dataTextEditor->setScrollbarsShown(true);
    dataTextEditor->setFont(FontOptions("Courier New", "Regular", 12));
    dataTextEditor->setText(formatDataForDisplay());
    addAndMakeVisible(dataTextEditor.get());
}

void RedisDataDisplayPopup::paint(Graphics& g)
{
    g.fillAll(findColour(ThemeColours::componentBackground));
    
    g.setColour(findColour(ThemeColours::defaultText));
    g.drawRect(getLocalBounds(), 1);
}

void RedisDataDisplayPopup::resized()
{
    if (titleLabel != nullptr)
        titleLabel->setBounds(10, 10, getWidth() - 20, 25);
    
    if (dataTextEditor != nullptr)
        dataTextEditor->setBounds(10, 45, getWidth() - 20, getHeight() - 55);
}

String RedisDataDisplayPopup::formatDataForDisplay()
{
    String displayText;
    
    if (dataRecords.isEmpty())
    {
        displayText = "No data records available.\n\n";
        displayText += "This could mean:\n";
        displayText += "- Redis channel is empty\n";
        displayText += "- Not connected to Redis\n";
        displayText += "- Channel name is incorrect\n";
        return displayText;
    }

    displayText += "Data Format: " + format.toUpperCase() + "\n";
    displayText += "Number of Records: " + String(dataRecords.size()) + "\n";
    displayText += "Channel: Latest " + String(dataRecords.size()) + " records\n";
    displayText += String::repeatedString("=", 60) + "\n\n";

    for (int i = 0; i < dataRecords.size(); i++)
    {
        if (format == "json")
        {
            displayText += formatJsonRecord(dataRecords[i], i + 1);
        }
        else if (format == "binary")
        {
            displayText += formatBinaryRecord(dataRecords[i], i + 1);
        }
        else if (format == "brandbci")
        {
            displayText += formatBrandBCIRecord(dataRecords[i], i + 1);
        }
        else
        {
            displayText += "Record " + String(i + 1) + ":\n";
            displayText += dataRecords[i] + "\n\n";
        }
    }

    return displayText;
}

String RedisDataDisplayPopup::formatJsonRecord(const String& jsonStr, int recordIndex)
{
    String formatted;
    formatted += "Record " + String(recordIndex) + " (JSON):\n";
    formatted += String::repeatedString("-", 30) + "\n";
    
    // Try to parse and pretty-print JSON
    var jsonData;
    Result parseResult = JSON::parse(jsonStr, jsonData);
    
    if (parseResult.wasOk())
    {
        if (jsonData.hasProperty("channels"))
        {
            var channels = jsonData["channels"];
            if (channels.isArray())
            {
                formatted += "Channels (" + String(channels.size()) + "): [";
                for (int i = 0; i < channels.size(); i++)
                {
                    if (i > 0) formatted += ", ";
                    formatted += String((double)channels[i], 3);
                    if (i >= 5 && channels.size() > 8) // Show first 5 and last 2 if many channels
                    {
                        formatted += ", ..., ";
                        formatted += String((double)channels[channels.size()-2], 3) + ", ";
                        formatted += String((double)channels[channels.size()-1], 3);
                        break;
                    }
                }
                formatted += "]\n";
            }
        }
        
        if (jsonData.hasProperty("timestamp"))
        {
            formatted += "Timestamp: " + String((int64)jsonData["timestamp"]) + "\n";
        }
    }
    else
    {
        formatted += "Raw JSON (parse error): " + jsonStr + "\n";
    }
    
    formatted += "\n";
    return formatted;
}

String RedisDataDisplayPopup::formatBinaryRecord(const String& binaryStr, int recordIndex)
{
    String formatted;
    formatted += "Record " + String(recordIndex) + " (Binary):\n";
    formatted += String::repeatedString("-", 30) + "\n";
    
    // Convert binary data to float array for display
    const char* data = binaryStr.toRawUTF8();
    size_t length = binaryStr.length();
    
    if (length % sizeof(float) == 0)
    {
        int numFloats = length / sizeof(float);
        const float* floatData = reinterpret_cast<const float*>(data);
        
        formatted += "Float values (" + String(numFloats) + "): [";
        for (int i = 0; i < numFloats; i++)
        {
            if (i > 0) formatted += ", ";
            formatted += String(floatData[i], 3);
            if (i >= 5 && numFloats > 8) // Show first 5 and last 2 if many values
            {
                formatted += ", ..., ";
                formatted += String(floatData[numFloats-2], 3) + ", ";
                formatted += String(floatData[numFloats-1], 3);
                break;
            }
        }
        formatted += "]\n";
    }
    else
    {
        formatted += "Binary data (" + String(length) + " bytes): ";
        formatted += "Invalid float array (length not multiple of 4)\n";
        formatted += "Raw hex: ";
        for (size_t i = 0; i < jmin(length, (size_t)32); i++)
        {
            formatted += String::toHexString((uint8)data[i]).paddedLeft('0', 2) + " ";
        }
        if (length > 32) formatted += "...";
        formatted += "\n";
    }
    
    formatted += "\n";
    return formatted;
}

String RedisDataDisplayPopup::formatBrandBCIRecord(const String& brandBCIStr, int recordIndex)
{
    String formatted;
    formatted += "Record " + String(recordIndex) + " (BRANDBCI):\n";
    formatted += String::repeatedString("-", 30) + "\n";

    // Try to parse as JSON first (BRANDBCI often uses JSON wrapper)
    var jsonData;
    Result parseResult = JSON::parse(brandBCIStr, jsonData);

    if (parseResult.wasOk())
    {
        // Handle BRANDBCI JSON format
        if (jsonData.hasProperty("data"))
        {
            var data = jsonData["data"];
            if (data.isArray())
            {
                formatted += "Data array (" + String(data.size()) + " channels): [";
                for (int i = 0; i < data.size(); i++)
                {
                    if (i > 0) formatted += ", ";
                    formatted += String((double)data[i], 3);
                    if (i >= 5 && data.size() > 8) // Show first 5 and last 2 if many channels
                    {
                        formatted += ", ..., ";
                        formatted += String((double)data[data.size()-2], 3) + ", ";
                        formatted += String((double)data[data.size()-1], 3);
                        break;
                    }
                }
                formatted += "]\n";
            }
            else
            {
                formatted += "Data: " + data.toString() + "\n";
            }
        }

        if (jsonData.hasProperty("timestamp"))
        {
            formatted += "Timestamp: " + String((int64)jsonData["timestamp"]) + "\n";
        }

        if (jsonData.hasProperty("sample_rate"))
        {
            formatted += "Sample Rate: " + String((double)jsonData["sample_rate"]) + " Hz\n";
        }

        if (jsonData.hasProperty("channels"))
        {
            formatted += "Channel Count: " + String((int)jsonData["channels"]) + "\n";
        }
    }
    else
    {
        // If not JSON, treat as raw BRANDBCI data
        formatted += "Raw BRANDBCI data (" + String(brandBCIStr.length()) + " bytes):\n";

        // Try to interpret as binary float array
        const char* data = brandBCIStr.toRawUTF8();
        size_t length = brandBCIStr.length();

        if (length >= sizeof(float) && length % sizeof(float) == 0)
        {
            int numFloats = length / sizeof(float);
            const float* floatData = reinterpret_cast<const float*>(data);

            formatted += "Float values (" + String(numFloats) + "): [";
            for (int i = 0; i < numFloats; i++)
            {
                if (i > 0) formatted += ", ";
                formatted += String(floatData[i], 3);
                if (i >= 5 && numFloats > 8) // Show first 5 and last 2 if many values
                {
                    formatted += ", ..., ";
                    formatted += String(floatData[numFloats-2], 3) + ", ";
                    formatted += String(floatData[numFloats-1], 3);
                    break;
                }
            }
            formatted += "]\n";
        }
        else
        {
            // Show as hex if not interpretable as floats
            formatted += "Hex data: ";
            for (size_t i = 0; i < jmin(length, (size_t)32); i++)
            {
                formatted += String::toHexString((uint8)data[i]).paddedLeft('0', 2) + " ";
            }
            if (length > 32) formatted += "...";
            formatted += "\n";
        }
    }

    formatted += "\n";
    return formatted;
}
