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

#include "RedisChannelScanPopup.h"

RedisChannelScanPopup::RedisChannelScanPopup(bool success, const Array<String>& channels, const String& errorMessage)
    : scanSuccess(success)
    , foundChannels(channels)
    , errorDetails(errorMessage)
{
    setSize(500, 350);
    setupUI();
}

RedisChannelScanPopup::~RedisChannelScanPopup()
{
}

void RedisChannelScanPopup::setupUI()
{
    // Icon label (status symbol) - using ASCII characters for compatibility
    iconLabel = std::make_unique<Label>("Icon Label", scanSuccess ? "OK" : "X");
    iconLabel->setBounds(20, 20, 40, 40);
    iconLabel->setFont(FontOptions("Inter", "Bold", 20));
    iconLabel->setJustificationType(Justification::centred);
    iconLabel->setColour(Label::textColourId, scanSuccess ? Colours::green : Colours::red);
    addAndMakeVisible(iconLabel.get());

    // Title label
    String titleText = scanSuccess ? 
        "Channel Scan Results" : 
        "Channel Scan Failed";
    
    titleLabel = std::make_unique<Label>("Title Label", titleText);
    titleLabel->setBounds(70, 20, 400, 30);
    titleLabel->setFont(FontOptions("Inter", "Bold", 16));
    titleLabel->setColour(Label::textColourId, 
        scanSuccess ? findColour(ThemeColours::defaultText) : Colours::red);
    addAndMakeVisible(titleLabel.get());

    // Channel list text editor
    channelListTextEditor = std::make_unique<TextEditor>("Channel List");
    channelListTextEditor->setBounds(20, 70, 460, 220);
    channelListTextEditor->setMultiLine(true);
    channelListTextEditor->setReadOnly(true);
    channelListTextEditor->setScrollbarsShown(true);
    channelListTextEditor->setFont(FontOptions("Fira Code", "Regular", 12));
    channelListTextEditor->setText(formatChannelList());
    // Consistent colors with other popups
    channelListTextEditor->setColour(TextEditor::backgroundColourId, findColour(ThemeColours::widgetBackground));
    channelListTextEditor->setColour(TextEditor::outlineColourId, findColour(ThemeColours::defaultText).withAlpha(0.3f));
    addAndMakeVisible(channelListTextEditor.get());

    // OK button
    okButton = std::make_unique<TextButton>("OK");
    okButton->setBounds(400, 310, 80, 25);
    okButton->addListener(this);
    addAndMakeVisible(okButton.get());
}

String RedisChannelScanPopup::formatChannelList()
{
    if (!scanSuccess)
    {
        return "Failed to scan Redis channels:\n\n" + errorDetails;
    }

    if (foundChannels.isEmpty())
    {
        return "No channels found in Redis database.\n\n"
               "Possible reasons:\n"
               "• No data streams are currently active\n"
               "• Data is stored in a different database (0-15)\n"
               "• Redis server is empty\n\n"
               "Make sure your data source is writing to Redis and try again.";
    }

    String result = "Found " + String(foundChannels.size()) + " available channels:\n\n";
    
    for (int i = 0; i < foundChannels.size(); i++)
    {
        result += String(i + 1) + ". " + foundChannels[i] + "\n";
    }
    
    result += "\nThese channels are now available in the dropdown list.";
    
    return result;
}

void RedisChannelScanPopup::paint(Graphics& g)
{
    // Background - consistent with other popups
    g.fillAll(findColour(ThemeColours::componentBackground));

    // Border - consistent with other popups
    g.setColour(findColour(ThemeColours::defaultText).withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void RedisChannelScanPopup::resized()
{
    // Components are positioned in setupUI()
}

void RedisChannelScanPopup::buttonClicked(Button* button)
{
    if (button == okButton.get())
    {
        // Close the popup properly using CallOutBox's exitModalState
        if (auto* callOutBox = findParentComponentOfClass<CallOutBox>())
        {
            callOutBox->exitModalState(0);
        }
    }
}
