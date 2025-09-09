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

#ifndef __REDISDATADISPLAYPOPUP_H_INCLUDED__
#define __REDISDATADISPLAYPOPUP_H_INCLUDED__

#include <ProcessorHeaders.h>

/**
 * Popup component that displays the latest Redis data records
 * 
 * Shows the retrieved data in a scrollable text area with proper formatting
 * for both JSON and binary data formats.
 */
class TESTABLE RedisDataDisplayPopup : public Component
{
public:
    /** Constructor */
    RedisDataDisplayPopup(const Array<String>& records, const String& dataFormat,
                          const String& configuredDataType, int configuredChannels);

    /** Destructor */
    ~RedisDataDisplayPopup();

    /** Component interface */
    void paint(Graphics& g) override;
    void resized() override;

private:
    // UI components
    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<TextEditor> dataTextEditor;
    std::unique_ptr<Viewport> scrollViewport;

    // Data
    Array<String> dataRecords;
    String format;
    // Configuration for consistent decoding
    String configuredDataType;
    int configuredChannels {0};

    // Helper methods
    void setupUI();
    String formatDataForDisplay();
    String formatJsonRecord(const String& jsonStr, int recordIndex);
    String formatBinaryRecord(const String& binaryStr, int recordIndex);
    String formatBrandBCIRecord(const String& brandBCIStr, int recordIndex);
    String formatOpenEphysRecord(const String& jsonStr, int recordIndex);

    // Data type formatting methods
    bool formatFloat32Data(String& output, const char* data, size_t length);
    bool formatFloat64Data(String& output, const char* data, size_t length);
    bool formatInt16Data(String& output, const char* data, size_t length);
    bool formatInt32Data(String& output, const char* data, size_t length);
    bool formatUInt16Data(String& output, const char* data, size_t length);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataDisplayPopup);
};

#endif // __REDISDATADISPLAYPOPUP_H_INCLUDED__
