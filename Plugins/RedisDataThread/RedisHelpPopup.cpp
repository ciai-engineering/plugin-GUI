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

#include "RedisHelpPopup.h"

RedisHelpPopup::RedisHelpPopup()
{
    setSize(650, 500);
    setupUI();
}

RedisHelpPopup::~RedisHelpPopup()
{
}

void RedisHelpPopup::setupUI()
{
    // Icon label (help symbol)
    iconLabel = std::make_unique<Label>("Icon Label", "?");
    iconLabel->setBounds(20, 20, 40, 40);
    iconLabel->setFont(FontOptions("Inter", "Regular", 24));
    iconLabel->setJustificationType(Justification::centred);
    addAndMakeVisible(iconLabel.get());

    // Title label
    titleLabel = std::make_unique<Label>("Title Label", "Redis Configuration Help");
    titleLabel->setBounds(70, 20, getWidth() - 90, 30);
    titleLabel->setFont(FontOptions("Inter", "Bold", 16));
    titleLabel->setJustificationType(Justification::left);
    addAndMakeVisible(titleLabel.get());

    // Help text editor (read-only)
    helpTextEditor = std::make_unique<TextEditor>("Help Text Editor");
    helpTextEditor->setBounds(20, 65, getWidth() - 40, getHeight() - 85);
    helpTextEditor->setMultiLine(true);
    helpTextEditor->setReadOnly(true);
    helpTextEditor->setScrollbarsShown(true);
    helpTextEditor->setFont(FontOptions("Inter", "Regular", 11));
    helpTextEditor->setText(getHelpText());
    helpTextEditor->setColour(TextEditor::backgroundColourId, findColour(ThemeColours::widgetBackground));
    helpTextEditor->setColour(TextEditor::outlineColourId, findColour(ThemeColours::defaultText).withAlpha(0.3f));
    addAndMakeVisible(helpTextEditor.get());
}

void RedisHelpPopup::paint(Graphics& g)
{
    g.fillAll(findColour(ThemeColours::componentBackground));
    
    g.setColour(findColour(ThemeColours::defaultText).withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void RedisHelpPopup::resized()
{
    if (iconLabel != nullptr)
        iconLabel->setBounds(20, 20, 40, 40);
    
    if (titleLabel != nullptr)
        titleLabel->setBounds(70, 20, getWidth() - 90, 30);
    
    if (helpTextEditor != nullptr)
        helpTextEditor->setBounds(20, 65, getWidth() - 40, getHeight() - 85);
}

String RedisHelpPopup::getHelpText()
{
    String helpText =
        "CONNECTION SETTINGS:\n"
        "  * Host: Redis server address (localhost for local server)\n"
        "  * Port: Redis server port (default: 6379)\n"
        "  * Password: Authentication password (optional)\n\n"

        "STREAM SETTINGS:\n"
        "  * Channel: Redis channel/stream name for data\n"
        "  * Stream Mode: Enable for Redis Streams (XREAD), disable for Lists (BLPOP)\n"
        "  * Always Latest: Always read newest data (enable for monitoring, disable for recording)\n\n"

        "DATA FORMAT:\n"
        "  * Sample Rate: Expected sampling frequency in Hz\n"
        "  * Channels: Number of data channels per sample\n"
        "  * Format: Data encoding format\n"
        "    - BRANDBCI: Native BRANDBCI format (recommended)\n"
        "    - JSON: Human-readable, flexible\n"
        "    - Binary: Highest performance\n\n"

        "ADVANCED SETTINGS:\n"
        "  * Buffer Size: Internal buffer size (100-100000 samples)\n"
        "  * OpenEphys Format: Enable native Open Ephys integration\n"
        "  * Data Validation: Enable real-time data checking\n\n"

        "PRESETS:\n"
        "  * Default (32ch, 30kHz): Standard neural recording configuration\n"
        "  * High Density (96ch, 30kHz): High-channel count neural recording\n"
        "  * Low Frequency (32ch, 1kHz): LFP and slow signal recording\n"
        "  * Testing (8ch, 1kHz): Development and testing configuration\n\n"

        "PERFORMANCE TIPS:\n"
        "  * Use Binary format for highest performance\n"
        "  * Enable Stream Mode for real-time applications\n"
        "  * Adjust buffer size based on latency requirements\n"
        "  * Disable Data Validation for maximum speed\n\n"

        "TROUBLESHOOTING:\n"
        "  * Red fields indicate validation errors\n"
        "  * Use Test button to verify connection\n"
        "  * Check Redis server is running and accessible\n"
        "  * Verify network connectivity and firewall settings";

    return helpText;
}
