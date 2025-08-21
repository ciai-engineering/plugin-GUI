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

#ifndef REDISCONNECTIONTESTPOPUP_H_INCLUDED
#define REDISCONNECTIONTESTPOPUP_H_INCLUDED

#include <ProcessorHeaders.h>

/**
 * Popup component for displaying Redis connection test results
 */
class RedisConnectionTestPopup : public Component, public Button::Listener
{
public:
    /** Constructor for successful connection */
    RedisConnectionTestPopup(bool success, const String& host, const String& port, const String& channel, const String& errorMessage = "");
    
    /** Destructor */
    ~RedisConnectionTestPopup();

    /** Component paint method */
    void paint(Graphics& g) override;

    /** Component resized method */
    void resized() override;

    /** Button listener method */
    void buttonClicked(Button* button) override;

private:
    /** Setup the UI components */
    void setupUI();

    /** Format the connection details for display */
    String formatConnectionDetails();

    // UI Components
    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> iconLabel;
    std::unique_ptr<Label> statusLabel;
    std::unique_ptr<TextEditor> detailsTextEditor;
    std::unique_ptr<TextButton> okButton;

    // Connection data
    bool connectionSuccess;
    String hostName;
    String portNumber;
    String channelName;
    String errorDetails;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisConnectionTestPopup)
};

#endif // REDISCONNECTIONTESTPOPUP_H_INCLUDED
