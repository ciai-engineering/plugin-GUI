/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2022 Open Ephys

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

#ifndef REDISCHANNELSCANPOPUP_H_INCLUDED
#define REDISCHANNELSCANPOPUP_H_INCLUDED

#include <ProcessorHeaders.h>

/**
 * Popup component for displaying Redis channel scan results
 */
class RedisChannelScanPopup : public Component, public Button::Listener
{
public:
    /** Constructor */
    RedisChannelScanPopup(bool success, const Array<String>& channels, const String& errorMessage = "");
    
    /** Destructor */
    ~RedisChannelScanPopup();

    /** Component paint method */
    void paint(Graphics& g) override;

    /** Component resized method */
    void resized() override;

    /** Button listener method */
    void buttonClicked(Button* button) override;

private:
    /** Setup the UI components */
    void setupUI();

    /** Format the channel list for display */
    String formatChannelList();

    // UI Components
    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> iconLabel;
    std::unique_ptr<TextEditor> channelListTextEditor;
    std::unique_ptr<TextButton> okButton;

    // Scan data
    bool scanSuccess;
    Array<String> foundChannels;
    String errorDetails;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisChannelScanPopup)
};

#endif // REDISCHANNELSCANPOPUP_H_INCLUDED
