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
        // Debug: Log the record data for analysis
        LOGD("Processing record ", i + 1, " (length: ", dataRecords[i].length(), " chars)");
        LOGD("Record preview: ", dataRecords[i].substring(0, 100), "...");

        // Try to detect if this is an Open Ephys format record (JSON with specific fields)
        var jsonData;
        Result parseResult = JSON::parse(dataRecords[i], jsonData);

        if (parseResult.wasOk())
        {
            LOGD("JSON parse successful for record ", i + 1);
            if (jsonData.isObject())
            {
                LOGD("Record ", i + 1, " is a JSON object");
                if (jsonData.hasProperty("data_shape") || jsonData.hasProperty("n_samples") || jsonData.hasProperty("data_dtype"))
                {
                    LOGD("Open Ephys format detected for record ", i + 1);
                    displayText += formatOpenEphysRecord(dataRecords[i], i + 1);
                }
                else
                {
                    LOGD("JSON object but not Open Ephys format for record ", i + 1);
                    if (format == "json")
                    {
                        displayText += formatJsonRecord(dataRecords[i], i + 1);
                    }
                    else
                    {
                        displayText += "Record " + String(i + 1) + " (JSON but not Open Ephys):\n";
                        displayText += dataRecords[i] + "\n\n";
                    }
                }
            }
            else
            {
                LOGD("JSON parse successful but not an object for record ", i + 1);
                displayText += "Record " + String(i + 1) + " (JSON but not object):\n";
                displayText += dataRecords[i] + "\n\n";
            }
        }
        else
        {
            LOGD("JSON parse failed for record ", i + 1, ": ", parseResult.getErrorMessage());
            // Not JSON, use format-specific handlers
            if (format == "binary")
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
    formatted += "Record " + String(recordIndex) + " (" + format.toUpperCase() + "):\n";
    formatted += String::repeatedString("-", 30) + "\n";

    // Get raw binary data correctly
    const char* data = binaryStr.getCharPointer().getAddress();
    size_t length = binaryStr.getNumBytesAsUTF8();

    formatted += "Raw " + format.toUpperCase() + " data (" + String(length) + " bytes):\n";

    // Show hex representation for debugging
    formatted += "Hex data: ";
    for (size_t i = 0; i < jmin(length, (size_t)50); i++) // Show first 50 bytes
    {
        if (i > 0 && i % 16 == 0) formatted += "\n          ";
        formatted += String::formatted("%02x ", (unsigned char)data[i]);
    }
    if (length > 50) formatted += "...";
    formatted += "\n\n";

    // Try to interpret as float array if length is appropriate
    if (length >= sizeof(float) && length % sizeof(float) == 0)
    {
        int numFloats = length / sizeof(float);
        const float* floatData = reinterpret_cast<const float*>(data);

        formatted += "Float values (" + String(numFloats) + "): [";
        int displayCount = jmin(numFloats, 10); // Show first 10 values

        for (int i = 0; i < displayCount; i++)
        {
            if (i > 0) formatted += ", ";

            // Check for reasonable float values (avoid displaying garbage)
            float value = floatData[i];
            if (std::isfinite(value) && std::abs(value) < 1e6)
            {
                formatted += String(value, 3);
            }
            else
            {
                formatted += String::formatted("0x%08x", *(uint32*)&value);
            }
        }

        if (numFloats > displayCount)
        {
            formatted += ", ..., ";
            float lastValue = floatData[numFloats-1];
            if (std::isfinite(lastValue) && std::abs(lastValue) < 1e6)
            {
                formatted += String(lastValue, 3);
            }
            else
            {
                formatted += String::formatted("0x%08x", *(uint32*)&lastValue);
            }
        }
        formatted += "]\n";
    }
    else
    {
        formatted += "Data length (" + String(length) + " bytes) not suitable for float array interpretation\n";
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

    // Get raw binary data correctly
    const char* data = brandBCIStr.getCharPointer().getAddress();
    size_t length = brandBCIStr.getNumBytesAsUTF8();

    formatted += "Raw BRANDBCI data (" + String(length) + " bytes):\n";

    // First try to parse as JSON (for JSON-wrapped BRANDBCI)
    var jsonData;
    Result parseResult = JSON::parse(brandBCIStr, jsonData);

    if (parseResult.wasOk())
    {
        // Handle BRANDBCI JSON format
        formatted += "JSON format detected:\n";
        if (jsonData.hasProperty("data"))
        {
            var dataArray = jsonData["data"];
            if (dataArray.isArray())
            {
                formatted += "Data array (" + String(dataArray.size()) + " channels): [";
                int displayCount = jmin(dataArray.size(), 10);
                for (int i = 0; i < displayCount; i++)
                {
                    if (i > 0) formatted += ", ";
                    formatted += String((double)dataArray[i], 3);
                }
                if (dataArray.size() > displayCount)
                {
                    formatted += ", ..., " + String((double)dataArray[dataArray.size()-1], 3);
                }
                formatted += "]\n";
            }
            else
            {
                formatted += "Data: " + dataArray.toString() + "\n";
            }
        }
        if (jsonData.hasProperty("timestamp"))
        {
            formatted += "Timestamp: " + String((int64)jsonData["timestamp"]) + "\n";
        }
    }
    else
    {
        // Handle binary BRANDBCI format
        formatted += "Binary format detected:\n";

        // Show hex representation
        formatted += "Hex data: ";
        for (size_t i = 0; i < jmin(length, (size_t)50); i++)
        {
            if (i > 0 && i % 16 == 0) formatted += "\n          ";
            formatted += String::formatted("%02x ", (unsigned char)data[i]);
        }
        if (length > 50) formatted += "...";
        formatted += "\n\n";

        // Try to interpret as float array
        if (length >= sizeof(float) && length % sizeof(float) == 0)
        {
            int numFloats = length / sizeof(float);
            const float* floatData = reinterpret_cast<const float*>(data);

            formatted += "Float values (" + String(numFloats) + "): [";
            int displayCount = jmin(numFloats, 10);

            for (int i = 0; i < displayCount; i++)
            {
                if (i > 0) formatted += ", ";

                float value = floatData[i];
                if (std::isfinite(value) && std::abs(value) < 1e6)
                {
                    formatted += String(value, 3);
                }
                else
                {
                    formatted += String::formatted("0x%08x", *(uint32*)&value);
                }
            }

            if (numFloats > displayCount)
            {
                formatted += ", ..., ";
                float lastValue = floatData[numFloats-1];
                if (std::isfinite(lastValue) && std::abs(lastValue) < 1e6)
                {
                    formatted += String(lastValue, 3);
                }
                else
                {
                    formatted += String::formatted("0x%08x", *(uint32*)&lastValue);
                }
            }
            formatted += "]\n";
        }
        else
        {
            formatted += "Data length not suitable for float array interpretation\n";
        }
    }

    formatted += "\n";
    return formatted;
}

String RedisDataDisplayPopup::formatOpenEphysRecord(const String& jsonStr, int recordIndex)
{
    String formatted;
    formatted += "Record " + String(recordIndex) + " (Open Ephys Format):\n";
    formatted += String::repeatedString("-", 30) + "\n";

    var jsonData;
    Result parseResult = JSON::parse(jsonStr, jsonData);

    if (!parseResult.wasOk())
    {
        formatted += "Error parsing Open Ephys JSON data\n";
        return formatted;
    }

    // Display metadata
    if (jsonData.hasProperty("run"))
        formatted += "Run: " + String((int)jsonData["run"]) + "\n";

    if (jsonData.hasProperty("timestamp"))
        formatted += "Timestamp: " + String((double)jsonData["timestamp"], 6) + "\n";

    if (jsonData.hasProperty("sample_rate"))
        formatted += "Sample Rate: " + String((int)jsonData["sample_rate"]) + " Hz\n";

    if (jsonData.hasProperty("n_channels"))
        formatted += "Channels: " + String((int)jsonData["n_channels"]) + "\n";

    if (jsonData.hasProperty("n_samples"))
        formatted += "Samples: " + String((int)jsonData["n_samples"]) + "\n";

    if (jsonData.hasProperty("data_shape"))
        formatted += "Data Shape: " + String(jsonData["data_shape"]) + "\n";

    if (jsonData.hasProperty("data_dtype"))
        formatted += "Data Type: " + String(jsonData["data_dtype"]) + "\n";

    if (jsonData.hasProperty("data_bytes"))
        formatted += "Binary Data: " + String((int)jsonData["data_bytes"]) + " bytes\n";

    // Process binary data if available
    if (jsonData.hasProperty("_binary_b64") || jsonData.hasProperty("_binary_data"))
    {
        MemoryBlock decoded;
        bool hasDecoded = false;
        String sourceType;

        if (jsonData.hasProperty("_binary_b64"))
        {
            sourceType = "Base64";
            String b64 = jsonData["_binary_b64"];
            MemoryOutputStream mos(decoded, false);
            hasDecoded = Base64::convertFromBase64(mos, b64);
        }
        else
        {
            // Legacy path: raw bytes stuffed into a String (may be corrupted by UTF-8)
            sourceType = "raw-string (legacy)";
            String raw = jsonData["_binary_data"];
            decoded.append(raw.getCharPointer().getAddress(), raw.getNumBytesAsUTF8());
            hasDecoded = true; // best-effort
        }

        formatted += "\nBinary Data Analysis (" + sourceType + "):\n";

        if (!hasDecoded || decoded.getSize() == 0)
        {
            formatted += "No binary payload available\n\n";
            return formatted;
        }

        const char* binaryData = static_cast<const char*>(decoded.getData());
        size_t binaryLength = decoded.getSize();

        formatted += "Raw data (" + String((int)binaryLength) + " bytes): ";

        // Show hex preview
        for (size_t i = 0; i < jmin(binaryLength, (size_t)32); i++)
        {
            if (i > 0 && i % 16 == 0) formatted += "\n                                ";
            formatted += String::formatted("%02x ", (unsigned char)binaryData[i]);
        }
        if (binaryLength > 32) formatted += "...";
        formatted += "\n";

        // Try to interpret as float32 data
        String dataType = jsonData.hasProperty("data_dtype") ? String(jsonData["data_dtype"]) : "unknown";
        if (dataType == "float32" && binaryLength >= sizeof(float) && binaryLength % sizeof(float) == 0)
        {
            int numFloats = binaryLength / sizeof(float);
            const float* floatData = reinterpret_cast<const float*>(binaryData);

            formatted += "\nFloat32 Values (" + String(numFloats) + " total): [";
            int displayCount = jmin(numFloats, 10); // Show first 10 values

            for (int i = 0; i < displayCount; i++)
            {
                if (i > 0) formatted += ", ";
                float value = floatData[i];
                if (std::isfinite(value))
                {
                    formatted += String(value, 3);
                }
                else
                {
                    formatted += "NaN/Inf";
                }
            }

            if (numFloats > displayCount)
            {
                formatted += ", ..., ";
                float lastValue = floatData[numFloats-1];
                if (std::isfinite(lastValue))
                {
                    formatted += String(lastValue, 3);
                }
                else
                {
                    formatted += "NaN/Inf";
                }
            }
            formatted += "]\n";

            // Calculate and show statistics
            if (numFloats > 0)
            {
                float minVal = std::numeric_limits<float>::max();
                float maxVal = std::numeric_limits<float>::lowest();
                double sum = 0.0;
                int validCount = 0;

                for (int i = 0; i < numFloats; i++)
                {
                    float val = floatData[i];
                    if (std::isfinite(val))
                    {
                        minVal = jmin(minVal, val);
                        maxVal = jmax(maxVal, val);
                        sum += val;
                        validCount++;
                    }
                }

                if (validCount > 0)
                {
                    formatted += "Statistics: Min=" + String(minVal, 3) +
                               ", Max=" + String(maxVal, 3) +
                               ", Mean=" + String(sum / validCount, 3) +
                               ", Valid=" + String(validCount) + "/" + String(numFloats) + "\n";
                }
            }
        }
        else
        {
            formatted += "\nData type '" + dataType + "' or length not suitable for float32 interpretation\n";
        }
    }

    formatted += "\n";
    return formatted;
}
