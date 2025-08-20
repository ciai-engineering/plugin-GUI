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

#include "RedisConnectionTestPopup.h"

RedisConnectionTestPopup::RedisConnectionTestPopup(bool success, const String& host, const String& port, const String& channel, const String& errorMessage)
    : connectionSuccess(success)
    , hostName(host)
    , portNumber(port)
    , channelName(channel)
    , errorDetails(errorMessage)
{
    setSize(450, 300);
    setupUI();
}

RedisConnectionTestPopup::~RedisConnectionTestPopup()
{
}

void RedisConnectionTestPopup::setupUI()
{
    // Icon label (status symbol)
    iconLabel = std::make_unique<Label>("Icon Label", connectionSuccess ? "OK" : "X");
    iconLabel->setBounds(20, 20, 40, 40);
    iconLabel->setFont(FontOptions("Inter", "Regular", 24));
    iconLabel->setJustificationType(Justification::centred);
    addAndMakeVisible(iconLabel.get());

    // Title label
    String titleText = connectionSuccess ? "Connection Test" : "Connection Test";
    titleLabel = std::make_unique<Label>("Title Label", titleText);
    titleLabel->setBounds(70, 20, getWidth() - 90, 30);
    titleLabel->setFont(FontOptions("Inter", "Bold", 16));
    titleLabel->setJustificationType(Justification::left);
    addAndMakeVisible(titleLabel.get());

    // Status subtitle
    String statusText = connectionSuccess ? "Successfully connected to Redis server!" : "Connection failed";
    auto statusLabel = std::make_unique<Label>("Status Label", statusText);
    statusLabel->setBounds(70, 50, getWidth() - 90, 20);
    statusLabel->setFont(FontOptions("Inter", "Regular", 12));
    statusLabel->setColour(Label::textColourId, connectionSuccess ? Colours::green : Colours::red);
    statusLabel->setJustificationType(Justification::left);
    addAndMakeVisible(statusLabel.get());

    // Details text editor (read-only)
    detailsTextEditor = std::make_unique<TextEditor>("Details Text Editor");
    detailsTextEditor->setBounds(20, 85, getWidth() - 40, getHeight() - 125);
    detailsTextEditor->setMultiLine(true);
    detailsTextEditor->setReadOnly(true);
    detailsTextEditor->setScrollbarsShown(true);
    detailsTextEditor->setFont(FontOptions("Inter", "Regular", 11));
    detailsTextEditor->setText(formatConnectionDetails());
    detailsTextEditor->setColour(TextEditor::backgroundColourId, findColour(ThemeColours::widgetBackground));
    detailsTextEditor->setColour(TextEditor::outlineColourId, findColour(ThemeColours::defaultText).withAlpha(0.3f));
    addAndMakeVisible(detailsTextEditor.get());
}

void RedisConnectionTestPopup::paint(Graphics& g)
{
    g.fillAll(findColour(ThemeColours::componentBackground));
    
    g.setColour(findColour(ThemeColours::defaultText).withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void RedisConnectionTestPopup::resized()
{
    if (iconLabel != nullptr)
        iconLabel->setBounds(20, 20, 40, 40);
    
    if (titleLabel != nullptr)
        titleLabel->setBounds(70, 20, getWidth() - 90, 30);
    
    if (detailsTextEditor != nullptr)
        detailsTextEditor->setBounds(20, 85, getWidth() - 40, getHeight() - 125);
}

String RedisConnectionTestPopup::formatConnectionDetails()
{
    String details;
    
    if (connectionSuccess)
    {
        details += "Connection Details:\n";
        details += String::repeatedString("=", 30) + "\n\n";
        details += "Host: " + hostName + "\n";
        details += "Port: " + portNumber + "\n";
        details += "Channel: " + channelName + "\n\n";
        details += "Status: Connected successfully\n";
        details += "Redis server is responding normally.\n\n";
        details += "You can now start data acquisition or view data from this Redis channel.";
    }
    else
    {
        details += "Connection Failed:\n";
        details += String::repeatedString("=", 30) + "\n\n";
        details += "Host: " + hostName + "\n";
        details += "Port: " + portNumber + "\n";
        details += "Channel: " + channelName + "\n\n";
        details += "Error Details:\n";
        details += errorDetails + "\n\n";
        details += "Troubleshooting Tips:\n";
        details += "* Verify Redis server is running\n";
        details += "* Check host and port settings\n";
        details += "* Ensure network connectivity\n";
        details += "* Check firewall settings\n";
        details += "* Verify authentication credentials";
    }
    
    return details;
}
