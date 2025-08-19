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

#include "RedisConfigurationPanel.h"
#include "RedisDataThread.h"
#include <CoreServicesHeader.h>

RedisConfigurationPanel::RedisConfigurationPanel(RedisDataThread* thread)
    : dataThread(thread)
{
    // Create all UI groups
    createConnectionGroup();
    createStreamGroup();
    createFormatGroup();
    createAdvancedGroup();
    createControlButtons();
    setupTooltips();
    setupPresets();
    
    // Update UI with current settings
    updateFromThread();

    // Start timer for status updates
    startTimer(2000); // Update every 2 seconds

    // Set initial size
    setSize(400, 600);
}

RedisConfigurationPanel::~RedisConfigurationPanel()
{
    stopTimer();
}

void RedisConfigurationPanel::paint(Graphics& g)
{
    // Background
    g.fillAll(findColour(ThemeColours::widgetBackground));
    
    // Title
    g.setColour(findColour(ThemeColours::defaultText));
    g.setFont(FontOptions("Inter", "Bold", 16));
    g.drawText("Redis Configuration", 10, 5, getWidth() - 20, 25, Justification::centred);
}

void RedisConfigurationPanel::resized()
{
    int yPos = 35;
    int groupHeight = 120;
    int margin = 10;
    int groupSpacing = 15;
    
    // Connection group
    if (connectionGroup)
    {
        connectionGroup->setBounds(margin, yPos, getWidth() - 2 * margin, groupHeight);
        yPos += groupHeight + groupSpacing;
    }
    
    // Stream group
    if (streamGroup)
    {
        streamGroup->setBounds(margin, yPos, getWidth() - 2 * margin, groupHeight);
        yPos += groupHeight + groupSpacing;
    }
    
    // Format group
    if (formatGroup)
    {
        formatGroup->setBounds(margin, yPos, getWidth() - 2 * margin, groupHeight);
        yPos += groupHeight + groupSpacing;
    }
    
    // Advanced group (smaller)
    if (advancedGroup)
    {
        advancedGroup->setBounds(margin, yPos, getWidth() - 2 * margin, 100);
        yPos += 100 + groupSpacing;
    }
    
    // Control buttons row 1
    if (presetCombo)
        presetCombo->setBounds(margin, yPos, 120, 25);
    if (testConnectionButton)
        testConnectionButton->setBounds(margin + 130, yPos, 60, 25);
    if (resetButton)
        resetButton->setBounds(margin + 200, yPos, 60, 25);
    if (helpButton)
        helpButton->setBounds(margin + 270, yPos, 50, 25);

    yPos += 30;

    // Control buttons row 2
    if (savePresetButton)
        savePresetButton->setBounds(margin, yPos, 100, 25);

    yPos += 35;

    // Status displays
    if (validationStatus)
        validationStatus->setBounds(margin, yPos, getWidth() - 2 * margin, 20);

    yPos += 25;

    if (connectionStatus)
        connectionStatus->setBounds(margin, yPos, getWidth() - 2 * margin, 20);

    yPos += 25;

    if (performanceHints)
        performanceHints->setBounds(margin, yPos, getWidth() - 2 * margin, 20);
}

void RedisConfigurationPanel::createConnectionGroup()
{
    connectionGroup = std::make_unique<GroupComponent>("Connection", "Connection Settings");
    addAndMakeVisible(connectionGroup.get());
    
    int yOffset = 25;
    int rowHeight = 25;
    int labelWidth = 80;
    int editorWidth = 120;
    int tooltipWidth = 150;
    
    // Host
    hostLabel = std::make_unique<Label>("Host Label", "Host:");
    hostLabel->setBounds(15, yOffset, labelWidth, 20);
    hostLabel->setFont(FontOptions("Inter", "Regular", 12));
    connectionGroup->addAndMakeVisible(hostLabel.get());
    
    hostEditor = std::make_unique<TextEditor>("Host Editor");
    hostEditor->setBounds(100, yOffset, editorWidth, 20);
    hostEditor->setTextToShowWhenEmpty("localhost", Colours::grey);
    hostEditor->addListener(this);
    connectionGroup->addAndMakeVisible(hostEditor.get());
    
    hostTooltip = std::make_unique<Label>("Host Tooltip", "Redis server address (e.g., localhost, 192.168.1.100)");
    hostTooltip->setBounds(230, yOffset, tooltipWidth, 20);
    hostTooltip->setFont(FontOptions("Inter", "Regular", 10));
    hostTooltip->setColour(Label::textColourId, Colours::grey);
    connectionGroup->addAndMakeVisible(hostTooltip.get());
    
    yOffset += rowHeight;
    
    // Port
    portLabel = std::make_unique<Label>("Port Label", "Port:");
    portLabel->setBounds(15, yOffset, labelWidth, 20);
    portLabel->setFont(FontOptions("Inter", "Regular", 12));
    connectionGroup->addAndMakeVisible(portLabel.get());
    
    portEditor = std::make_unique<TextEditor>("Port Editor");
    portEditor->setBounds(100, yOffset, 80, 20);
    portEditor->setTextToShowWhenEmpty("6379", Colours::grey);
    portEditor->setInputRestrictions(5, "0123456789");
    portEditor->addListener(this);
    connectionGroup->addAndMakeVisible(portEditor.get());
    
    portTooltip = std::make_unique<Label>("Port Tooltip", "Redis server port (usually 6379)");
    portTooltip->setBounds(190, yOffset, tooltipWidth, 20);
    portTooltip->setFont(FontOptions("Inter", "Regular", 10));
    portTooltip->setColour(Label::textColourId, Colours::grey);
    connectionGroup->addAndMakeVisible(portTooltip.get());
    
    yOffset += rowHeight;
    
    // Password
    passwordLabel = std::make_unique<Label>("Password Label", "Password:");
    passwordLabel->setBounds(15, yOffset, labelWidth, 20);
    passwordLabel->setFont(FontOptions("Inter", "Regular", 12));
    connectionGroup->addAndMakeVisible(passwordLabel.get());
    
    passwordEditor = std::make_unique<TextEditor>("Password Editor");
    passwordEditor->setBounds(100, yOffset, editorWidth, 20);
    passwordEditor->setPasswordCharacter('*');
    passwordEditor->setTextToShowWhenEmpty("(optional)", Colours::grey);
    passwordEditor->addListener(this);
    connectionGroup->addAndMakeVisible(passwordEditor.get());
    
    passwordTooltip = std::make_unique<Label>("Password Tooltip", "Redis authentication password (leave empty if none)");
    passwordTooltip->setBounds(230, yOffset, tooltipWidth, 20);
    passwordTooltip->setFont(FontOptions("Inter", "Regular", 10));
    passwordTooltip->setColour(Label::textColourId, Colours::grey);
    connectionGroup->addAndMakeVisible(passwordTooltip.get());
}

void RedisConfigurationPanel::createStreamGroup()
{
    streamGroup = std::make_unique<GroupComponent>("Stream", "Data Stream Settings");
    addAndMakeVisible(streamGroup.get());
    
    int yOffset = 25;
    int rowHeight = 25;
    int labelWidth = 80;
    int editorWidth = 120;
    int tooltipWidth = 150;
    
    // Channel/Stream Name
    channelLabel = std::make_unique<Label>("Channel Label", "Channel:");
    channelLabel->setBounds(15, yOffset, labelWidth, 20);
    channelLabel->setFont(FontOptions("Inter", "Regular", 12));
    streamGroup->addAndMakeVisible(channelLabel.get());
    
    channelEditor = std::make_unique<TextEditor>("Channel Editor");
    channelEditor->setBounds(100, yOffset, editorWidth, 20);
    channelEditor->setTextToShowWhenEmpty("neural_data", Colours::grey);
    channelEditor->addListener(this);
    streamGroup->addAndMakeVisible(channelEditor.get());
    
    channelTooltip = std::make_unique<Label>("Channel Tooltip", "Redis channel/stream name (e.g., neural_data, openephys_data)");
    channelTooltip->setBounds(230, yOffset, tooltipWidth, 20);
    channelTooltip->setFont(FontOptions("Inter", "Regular", 10));
    channelTooltip->setColour(Label::textColourId, Colours::grey);
    streamGroup->addAndMakeVisible(channelTooltip.get());
    
    yOffset += rowHeight;
    
    // Stream Mode
    streamModeLabel = std::make_unique<Label>("Stream Mode Label", "Stream Mode:");
    streamModeLabel->setBounds(15, yOffset, labelWidth, 20);
    streamModeLabel->setFont(FontOptions("Inter", "Regular", 12));
    streamGroup->addAndMakeVisible(streamModeLabel.get());
    
    streamModeButton = std::make_unique<ToggleButton>("Stream Mode");
    streamModeButton->setBounds(100, yOffset, 20, 20);
    streamModeButton->addListener(this);
    streamGroup->addAndMakeVisible(streamModeButton.get());
    
    streamModeTooltip = std::make_unique<Label>("Stream Mode Tooltip", "Enable Redis Stream mode (recommended for BRANDBCI)");
    streamModeTooltip->setBounds(130, yOffset, tooltipWidth + 50, 20);
    streamModeTooltip->setFont(FontOptions("Inter", "Regular", 10));
    streamModeTooltip->setColour(Label::textColourId, Colours::grey);
    streamGroup->addAndMakeVisible(streamModeTooltip.get());
}

void RedisConfigurationPanel::textEditorTextChanged(TextEditor& editor)
{
    // Real-time validation
    if (&editor == hostEditor.get())
        validateField(hostEditor.get(), "host");
    else if (&editor == portEditor.get())
        validateField(portEditor.get(), "port");
    else if (&editor == channelEditor.get())
        validateField(channelEditor.get(), "channel");
    else if (&editor == sampleRateEditor.get())
        validateField(sampleRateEditor.get(), "sampleRate");
    else if (&editor == numChannelsEditor.get())
        validateField(numChannelsEditor.get(), "numChannels");
    else if (&editor == bufferSizeEditor.get())
        validateField(bufferSizeEditor.get(), "bufferSize");
    
    updateValidationStatus();
}

void RedisConfigurationPanel::textEditorReturnKeyPressed(TextEditor& editor)
{
    // Apply settings when Enter is pressed
    applyToThread();
}

void RedisConfigurationPanel::textEditorFocusLost(TextEditor& editor)
{
    // Validate when focus is lost
    textEditorTextChanged(editor);
}

void RedisConfigurationPanel::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == dataFormatCombo.get())
    {
        applyToThread();
    }
    else if (comboBox == presetCombo.get())
    {
        String selectedPreset = presetCombo->getText();
        if (selectedPreset != "Select Preset...")
        {
            loadPreset(selectedPreset);
        }
    }
}

void RedisConfigurationPanel::buttonClicked(Button* button)
{
    if (button == streamModeButton.get())
    {
        applyToThread();
    }
    else if (button == testConnectionButton.get())
    {
        testConnection();
    }
    else if (button == resetButton.get())
    {
        // Reset to default values
        loadPreset("Default");
    }
    else if (button == savePresetButton.get())
    {
        // TODO: Implement save preset functionality
        AlertWindow::showMessageBox(AlertWindow::InfoIcon,
                                   "Save Preset",
                                   "Preset saving functionality will be implemented in a future version.");
    }
    else if (button == helpButton.get())
    {
        showHelpDialog();
    }
    else if (button == openEphysFormatButton.get() || button == dataValidationButton.get())
    {
        applyToThread();
    }
}

void RedisConfigurationPanel::timerCallback()
{
    // Update connection status periodically
    updateValidationStatus();
}

void RedisConfigurationPanel::createFormatGroup()
{
    formatGroup = std::make_unique<GroupComponent>("Format", "Data Format Settings");
    addAndMakeVisible(formatGroup.get());

    int yOffset = 25;
    int rowHeight = 25;
    int labelWidth = 80;
    int editorWidth = 120;
    int tooltipWidth = 150;

    // Sample Rate
    sampleRateLabel = std::make_unique<Label>("Sample Rate Label", "Sample Rate:");
    sampleRateLabel->setBounds(15, yOffset, labelWidth, 20);
    sampleRateLabel->setFont(FontOptions("Inter", "Regular", 12));
    formatGroup->addAndMakeVisible(sampleRateLabel.get());

    sampleRateEditor = std::make_unique<TextEditor>("Sample Rate Editor");
    sampleRateEditor->setBounds(100, yOffset, 80, 20);
    sampleRateEditor->setTextToShowWhenEmpty("30000", Colours::grey);
    sampleRateEditor->setInputRestrictions(0, "0123456789.");
    sampleRateEditor->addListener(this);
    formatGroup->addAndMakeVisible(sampleRateEditor.get());

    sampleRateTooltip = std::make_unique<Label>("Sample Rate Tooltip", "Sampling rate in Hz (e.g., 30000 for 30kHz)");
    sampleRateTooltip->setBounds(190, yOffset, tooltipWidth, 20);
    sampleRateTooltip->setFont(FontOptions("Inter", "Regular", 10));
    sampleRateTooltip->setColour(Label::textColourId, Colours::grey);
    formatGroup->addAndMakeVisible(sampleRateTooltip.get());

    yOffset += rowHeight;

    // Number of Channels
    numChannelsLabel = std::make_unique<Label>("Num Channels Label", "Channels:");
    numChannelsLabel->setBounds(15, yOffset, labelWidth, 20);
    numChannelsLabel->setFont(FontOptions("Inter", "Regular", 12));
    formatGroup->addAndMakeVisible(numChannelsLabel.get());

    numChannelsEditor = std::make_unique<TextEditor>("Num Channels Editor");
    numChannelsEditor->setBounds(100, yOffset, 80, 20);
    numChannelsEditor->setTextToShowWhenEmpty("32", Colours::grey);
    numChannelsEditor->setInputRestrictions(4, "0123456789");
    numChannelsEditor->addListener(this);
    formatGroup->addAndMakeVisible(numChannelsEditor.get());

    numChannelsTooltip = std::make_unique<Label>("Num Channels Tooltip", "Number of channels (1-1024, e.g., 32, 96)");
    numChannelsTooltip->setBounds(190, yOffset, tooltipWidth, 20);
    numChannelsTooltip->setFont(FontOptions("Inter", "Regular", 10));
    numChannelsTooltip->setColour(Label::textColourId, Colours::grey);
    formatGroup->addAndMakeVisible(numChannelsTooltip.get());

    yOffset += rowHeight;

    // Data Format
    dataFormatLabel = std::make_unique<Label>("Data Format Label", "Format:");
    dataFormatLabel->setBounds(15, yOffset, labelWidth, 20);
    dataFormatLabel->setFont(FontOptions("Inter", "Regular", 12));
    formatGroup->addAndMakeVisible(dataFormatLabel.get());

    dataFormatCombo = std::make_unique<ComboBox>("Data Format Combo");
    dataFormatCombo->setBounds(100, yOffset, 100, 20);
    dataFormatCombo->addItem("BRANDBCI (recommended)", 1);
    dataFormatCombo->addItem("JSON (general)", 2);
    dataFormatCombo->addItem("Binary (performance)", 3);
    dataFormatCombo->addListener(this);
    formatGroup->addAndMakeVisible(dataFormatCombo.get());

    dataFormatTooltip = std::make_unique<Label>("Data Format Tooltip", "Data format for Redis communication");
    dataFormatTooltip->setBounds(210, yOffset, tooltipWidth, 20);
    dataFormatTooltip->setFont(FontOptions("Inter", "Regular", 10));
    dataFormatTooltip->setColour(Label::textColourId, Colours::grey);
    formatGroup->addAndMakeVisible(dataFormatTooltip.get());
}

void RedisConfigurationPanel::createAdvancedGroup()
{
    advancedGroup = std::make_unique<GroupComponent>("Advanced", "Advanced Settings");
    addAndMakeVisible(advancedGroup.get());

    int yOffset = 25;
    int rowHeight = 25;
    int labelWidth = 100;
    int editorWidth = 80;

    // Buffer Size
    bufferSizeLabel = std::make_unique<Label>("Buffer Size Label", "Buffer Size:");
    bufferSizeLabel->setBounds(15, yOffset, labelWidth, 20);
    bufferSizeLabel->setFont(FontOptions("Inter", "Regular", 12));
    advancedGroup->addAndMakeVisible(bufferSizeLabel.get());

    bufferSizeEditor = std::make_unique<TextEditor>("Buffer Size Editor");
    bufferSizeEditor->setBounds(120, yOffset, editorWidth, 20);
    bufferSizeEditor->setTextToShowWhenEmpty("1000", Colours::grey);
    bufferSizeEditor->setInputRestrictions(6, "0123456789");
    bufferSizeEditor->addListener(this);
    advancedGroup->addAndMakeVisible(bufferSizeEditor.get());

    bufferSizeTooltip = std::make_unique<Label>("Buffer Size Tooltip", "Buffer size in samples (100-100000)");
    bufferSizeTooltip->setBounds(210, yOffset, 150, 20);
    bufferSizeTooltip->setFont(FontOptions("Inter", "Regular", 10));
    bufferSizeTooltip->setColour(Label::textColourId, Colours::grey);
    advancedGroup->addAndMakeVisible(bufferSizeTooltip.get());

    yOffset += rowHeight;

    // Open Ephys Format
    openEphysFormatLabel = std::make_unique<Label>("OpenEphys Format Label", "OpenEphys Format:");
    openEphysFormatLabel->setBounds(15, yOffset, labelWidth, 20);
    openEphysFormatLabel->setFont(FontOptions("Inter", "Regular", 12));
    advancedGroup->addAndMakeVisible(openEphysFormatLabel.get());

    openEphysFormatButton = std::make_unique<ToggleButton>("OpenEphys Format");
    openEphysFormatButton->setBounds(120, yOffset, 20, 20);
    openEphysFormatButton->addListener(this);
    advancedGroup->addAndMakeVisible(openEphysFormatButton.get());

    // Data Validation
    dataValidationLabel = std::make_unique<Label>("Data Validation Label", "Data Validation:");
    dataValidationLabel->setBounds(200, yOffset, labelWidth, 20);
    dataValidationLabel->setFont(FontOptions("Inter", "Regular", 12));
    advancedGroup->addAndMakeVisible(dataValidationLabel.get());

    dataValidationButton = std::make_unique<ToggleButton>("Data Validation");
    dataValidationButton->setBounds(305, yOffset, 20, 20);
    dataValidationButton->addListener(this);
    advancedGroup->addAndMakeVisible(dataValidationButton.get());
}

void RedisConfigurationPanel::createControlButtons()
{
    // Preset selector
    presetCombo = std::make_unique<ComboBox>("Preset Combo");
    presetCombo->addListener(this);
    addAndMakeVisible(presetCombo.get());

    // Test connection button
    testConnectionButton = std::make_unique<UtilityButton>("Test");
    testConnectionButton->addListener(this);
    addAndMakeVisible(testConnectionButton.get());

    // Reset button
    resetButton = std::make_unique<UtilityButton>("Reset");
    resetButton->addListener(this);
    addAndMakeVisible(resetButton.get());

    // Save preset button
    savePresetButton = std::make_unique<UtilityButton>("Save Preset");
    savePresetButton->addListener(this);
    addAndMakeVisible(savePresetButton.get());

    // Help button
    helpButton = std::make_unique<UtilityButton>("Help");
    helpButton->addListener(this);
    addAndMakeVisible(helpButton.get());

    // Status indicators
    validationStatus = std::make_unique<Label>("Validation Status", "All settings valid");
    validationStatus->setFont(FontOptions("Inter", "Regular", 11));
    validationStatus->setColour(Label::textColourId, Colours::green);
    addAndMakeVisible(validationStatus.get());

    connectionStatus = std::make_unique<Label>("Connection Status", "Not connected");
    connectionStatus->setFont(FontOptions("Inter", "Regular", 11));
    connectionStatus->setColour(Label::textColourId, Colours::orange);
    addAndMakeVisible(connectionStatus.get());

    performanceHints = std::make_unique<Label>("Performance Hints", "💡 Use BRANDBCI format for best performance");
    performanceHints->setFont(FontOptions("Inter", "Regular", 10));
    performanceHints->setColour(Label::textColourId, Colours::grey);
    addAndMakeVisible(performanceHints.get());
}

void RedisConfigurationPanel::setupTooltips()
{
    // Set detailed tooltips for all components
    hostEditor->setTooltip("Redis server hostname or IP address.\n"
                          "Examples:\n"
                          "• localhost - Local Redis server\n"
                          "• 192.168.1.100 - Remote server\n"
                          "• redis.example.com - Domain name");

    portEditor->setTooltip("Redis server port number (1-65535).\n"
                          "Default: 6379\n"
                          "Common alternatives: 6380, 16379");

    passwordEditor->setTooltip("Redis authentication password.\n"
                              "Leave empty if Redis AUTH is not configured.\n"
                              "Required for Redis Cloud or secured instances.");

    channelEditor->setTooltip("Redis channel or stream name for data.\n"
                             "Examples:\n"
                             "• neural_data - BRANDBCI standard\n"
                             "• openephys_data - Default format\n"
                             "• lfp_stream - LFP data stream");

    streamModeButton->setTooltip("Enable Redis Stream mode (XREAD commands).\n"
                                "Recommended for:\n"
                                "• BRANDBCI systems\n"
                                "• Real-time streaming\n"
                                "• Multiple consumers\n"
                                "\n"
                                "Disable for legacy BLPOP mode.");

    sampleRateEditor->setTooltip("Expected sampling rate in Hz.\n"
                                "Common values:\n"
                                "• 30000 - High-frequency neural data\n"
                                "• 1000 - LFP or slow signals\n"
                                "• 2000 - Behavioral data");

    numChannelsEditor->setTooltip("Number of data channels per sample.\n"
                                 "Range: 1-1024\n"
                                 "Common values:\n"
                                 "• 32 - Small electrode arrays\n"
                                 "• 96 - Utah arrays\n"
                                 "• 128 - High-density probes");

    dataFormatCombo->setTooltip("Data format for Redis communication:\n"
                               "• BRANDBCI - Native BRANDBCI format (recommended)\n"
                               "• JSON - Human-readable, flexible\n"
                               "• Binary - Highest performance, compact");

    bufferSizeEditor->setTooltip("Internal buffer size in samples.\n"
                                "Range: 100-100000\n"
                                "Guidelines:\n"
                                "• 500-2000: Real-time, low latency\n"
                                "• 3000-10000: Balanced performance\n"
                                "• 10000+: High throughput, higher latency");

    openEphysFormatButton->setTooltip("Enable Open Ephys native format support.\n"
                                     "Provides better integration with\n"
                                     "Open Ephys signal processing chain.");

    dataValidationButton->setTooltip("Enable real-time data validation.\n"
                                    "Checks for:\n"
                                    "• Correct channel count\n"
                                    "• Valid data ranges\n"
                                    "• Format consistency\n"
                                    "\n"
                                    "Disable for maximum performance.");
}

void RedisConfigurationPanel::setupPresets()
{
    presetCombo->addItem("Select Preset...", 1);
    presetCombo->addItem("Default", 2);
    presetCombo->addItem("BRANDBCI Standard", 3);
    presetCombo->addItem("Local Testing", 4);
    presetCombo->addItem("High Performance", 5);
    presetCombo->addItem("Utah Array 96ch", 6);
    presetCombo->addItem("Neuropixels", 7);
    presetCombo->addItem("LFP Recording", 8);
    presetCombo->addItem("Behavioral Data", 9);
    presetCombo->setSelectedItemIndex(0);
}

void RedisConfigurationPanel::updateFromThread()
{
    if (!dataThread) return;

    // Update connection settings
    hostEditor->setText(dataThread->getRedisHost(), false);
    portEditor->setText(String(dataThread->getRedisPort()), false);
    passwordEditor->setText(dataThread->getRedisPassword(), false);

    // Update stream settings
    channelEditor->setText(dataThread->getRedisChannelName(), false);
    streamModeButton->setToggleState(dataThread->getStreamMode(), dontSendNotification);

    // Update format settings
    sampleRateEditor->setText(String(dataThread->getSampleRate()), false);
    numChannelsEditor->setText(String(dataThread->getNumDataChannels()), false);

    String format = dataThread->getDataFormat();
    int formatIndex = format == "brandbci" ? 0 : format == "json" ? 1 : 2;
    dataFormatCombo->setSelectedItemIndex(formatIndex, dontSendNotification);

    // Update advanced settings
    bufferSizeEditor->setText(String(dataThread->getBufferSize()), false);
    openEphysFormatButton->setToggleState(dataThread->isOpenEphysFormatEnabled(), dontSendNotification);
    dataValidationButton->setToggleState(dataThread->isDataValidationEnabled(), dontSendNotification);

    // Validate all fields
    updateValidationStatus();
}

void RedisConfigurationPanel::applyToThread()
{
    if (!dataThread || !validateAllSettings()) return;

    // Apply connection settings
    dataThread->setRedisHost(hostEditor->getText());
    dataThread->setRedisPort(portEditor->getText().getIntValue());
    dataThread->setRedisPassword(passwordEditor->getText());

    // Apply stream settings
    dataThread->setRedisChannel(channelEditor->getText());
    dataThread->setStreamMode(streamModeButton->getToggleState());

    // Apply format settings
    dataThread->setSampleRate(sampleRateEditor->getText().getFloatValue());
    dataThread->setNumChannels(numChannelsEditor->getText().getIntValue());

    int formatIndex = dataFormatCombo->getSelectedItemIndex();
    String format = formatIndex == 0 ? "brandbci" : formatIndex == 1 ? "json" : "binary";
    dataThread->setDataFormat(format);

    // Apply advanced settings
    dataThread->setBufferSize(bufferSizeEditor->getText().getIntValue());
    dataThread->setOpenEphysFormatEnabled(openEphysFormatButton->getToggleState());
    dataThread->setDataValidationEnabled(dataValidationButton->getToggleState());
}

bool RedisConfigurationPanel::validateAllSettings()
{
    bool allValid = true;

    // Validate host
    if (hostEditor->getText().isEmpty())
    {
        showFieldError(hostEditor.get(), "Host cannot be empty");
        allValid = false;
    }
    else
    {
        clearFieldError(hostEditor.get());
    }

    // Validate port
    int port = portEditor->getText().getIntValue();
    if (port <= 0 || port > 65535)
    {
        showFieldError(portEditor.get(), "Port must be 1-65535");
        allValid = false;
    }
    else
    {
        clearFieldError(portEditor.get());
    }

    // Validate channel
    if (channelEditor->getText().isEmpty())
    {
        showFieldError(channelEditor.get(), "Channel cannot be empty");
        allValid = false;
    }
    else
    {
        clearFieldError(channelEditor.get());
    }

    // Validate sample rate
    float sampleRate = sampleRateEditor->getText().getFloatValue();
    if (sampleRate <= 0)
    {
        showFieldError(sampleRateEditor.get(), "Sample rate must be positive");
        allValid = false;
    }
    else
    {
        clearFieldError(sampleRateEditor.get());
    }

    // Validate number of channels
    int numChannels = numChannelsEditor->getText().getIntValue();
    if (numChannels <= 0 || numChannels > 1024)
    {
        showFieldError(numChannelsEditor.get(), "Channels must be 1-1024");
        allValid = false;
    }
    else
    {
        clearFieldError(numChannelsEditor.get());
    }

    // Validate buffer size
    int bufferSize = bufferSizeEditor->getText().getIntValue();
    if (bufferSize < 100 || bufferSize > 100000)
    {
        showFieldError(bufferSizeEditor.get(), "Buffer size must be 100-100000");
        allValid = false;
    }
    else
    {
        clearFieldError(bufferSizeEditor.get());
    }

    return allValid;
}

void RedisConfigurationPanel::validateField(TextEditor* editor, const String& fieldName)
{
    if (fieldName == "host")
    {
        if (editor->getText().isEmpty())
            showFieldError(editor, "Host cannot be empty");
        else
            clearFieldError(editor);
    }
    else if (fieldName == "port")
    {
        int port = editor->getText().getIntValue();
        if (port <= 0 || port > 65535)
            showFieldError(editor, "Port must be 1-65535");
        else
            clearFieldError(editor);
    }
    else if (fieldName == "channel")
    {
        if (editor->getText().isEmpty())
            showFieldError(editor, "Channel cannot be empty");
        else
            clearFieldError(editor);
    }
    else if (fieldName == "sampleRate")
    {
        float rate = editor->getText().getFloatValue();
        if (rate <= 0)
            showFieldError(editor, "Sample rate must be positive");
        else
            clearFieldError(editor);
    }
    else if (fieldName == "numChannels")
    {
        int channels = editor->getText().getIntValue();
        if (channels <= 0 || channels > 1024)
            showFieldError(editor, "Channels must be 1-1024");
        else
            clearFieldError(editor);
    }
    else if (fieldName == "bufferSize")
    {
        int size = editor->getText().getIntValue();
        if (size < 100 || size > 100000)
            showFieldError(editor, "Buffer size must be 100-100000");
        else
            clearFieldError(editor);
    }
}

void RedisConfigurationPanel::updateValidationStatus()
{
    bool allValid = validateAllSettings();

    if (allValid)
    {
        validationStatus->setText("✓ All settings valid", dontSendNotification);
        validationStatus->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        validationStatus->setText("⚠ Please check highlighted fields", dontSendNotification);
        validationStatus->setColour(Label::textColourId, Colours::orange);
    }

    // Update connection status
    if (dataThread && dataThread->isConnected())
    {
        connectionStatus->setText("🟢 Connected to " + dataThread->getRedisHost() + ":" + String(dataThread->getRedisPort()), dontSendNotification);
        connectionStatus->setColour(Label::textColourId, Colours::green);
    }
    else
    {
        connectionStatus->setText("🔴 Not connected", dontSendNotification);
        connectionStatus->setColour(Label::textColourId, Colours::red);
    }

    // Update performance hints based on current settings
    updatePerformanceHints();
}

void RedisConfigurationPanel::updatePerformanceHints()
{
    String hint = "💡 ";

    // Check data format
    int formatIndex = dataFormatCombo->getSelectedItemIndex();
    if (formatIndex == 2) // Binary
    {
        hint += "Excellent choice! Binary format provides best performance.";
    }
    else if (formatIndex == 0) // BRANDBCI
    {
        hint += "Good choice! BRANDBCI format is optimized for neural data.";
    }
    else // JSON
    {
        hint += "Consider Binary format for better performance.";
    }

    // Check stream mode
    if (streamModeButton->getToggleState())
    {
        hint += " Stream mode enabled for real-time processing.";
    }
    else
    {
        hint += " Enable Stream mode for better real-time performance.";
    }

    // Check buffer size
    int bufferSize = bufferSizeEditor->getText().getIntValue();
    if (bufferSize < 1000)
    {
        hint += " Small buffer = low latency.";
    }
    else if (bufferSize > 10000)
    {
        hint += " Large buffer = high throughput.";
    }

    performanceHints->setText(hint, dontSendNotification);
}

void RedisConfigurationPanel::showFieldError(Component* field, const String& message)
{
    field->setColour(TextEditor::outlineColourId, Colours::red);
    field->setColour(TextEditor::focusedOutlineColourId, Colours::red);
    field->setColour(TextEditor::backgroundColourId, Colours::red.withAlpha(0.1f));

    // Add tooltip with error message
    if (auto* editor = dynamic_cast<TextEditor*>(field))
    {
        String originalTooltip = editor->getTooltip();
        editor->setTooltip("❌ " + message + "\n\n" + originalTooltip);
    }
}

void RedisConfigurationPanel::clearFieldError(Component* field)
{
    field->setColour(TextEditor::outlineColourId, findColour(ThemeColours::defaultText).withAlpha(0.3f));
    field->setColour(TextEditor::focusedOutlineColourId, findColour(ThemeColours::defaultText));
    field->setColour(TextEditor::backgroundColourId, findColour(ThemeColours::widgetBackground));

    if (auto* editor = dynamic_cast<TextEditor*>(field))
    {
        // Restore original tooltip (remove error prefix)
        String tooltip = editor->getTooltip();
        if (tooltip.startsWith("❌"))
        {
            int newlinePos = tooltip.indexOf("\n\n");
            if (newlinePos > 0)
            {
                editor->setTooltip(tooltip.substring(newlinePos + 2));
            }
            else
            {
                editor->setTooltip("");
            }
        }
    }
}

void RedisConfigurationPanel::loadPreset(const String& presetName)
{
    if (presetName == "Default")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("openephys_data");
        streamModeButton->setToggleState(false, dontSendNotification);
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("32");
        dataFormatCombo->setSelectedItemIndex(1); // JSON
        bufferSizeEditor->setText("1000");
        openEphysFormatButton->setToggleState(false, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "BRANDBCI Standard")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("neural_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("96");
        dataFormatCombo->setSelectedItemIndex(0); // BRANDBCI
        bufferSizeEditor->setText("2000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "Local Testing")
    {
        hostEditor->setText("127.0.0.1");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("test_data");
        streamModeButton->setToggleState(false, dontSendNotification);
        sampleRateEditor->setText("1000");
        numChannelsEditor->setText("8");
        dataFormatCombo->setSelectedItemIndex(1); // JSON
        bufferSizeEditor->setText("500");
        openEphysFormatButton->setToggleState(false, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "High Performance")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("neural_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("128");
        dataFormatCombo->setSelectedItemIndex(2); // Binary
        bufferSizeEditor->setText("5000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(false, dontSendNotification);
    }
    else if (presetName == "Utah Array 96ch")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("utah_array_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("96");
        dataFormatCombo->setSelectedItemIndex(0); // BRANDBCI
        bufferSizeEditor->setText("3000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "Neuropixels")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("neuropixels_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("384");
        dataFormatCombo->setSelectedItemIndex(2); // Binary for high channel count
        bufferSizeEditor->setText("10000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(false, dontSendNotification); // Disable for performance
    }
    else if (presetName == "LFP Recording")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("lfp_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        sampleRateEditor->setText("1000");
        numChannelsEditor->setText("64");
        dataFormatCombo->setSelectedItemIndex(0); // BRANDBCI
        bufferSizeEditor->setText("2000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "Behavioral Data")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelEditor->setText("behavior_data");
        streamModeButton->setToggleState(false, dontSendNotification); // Use list mode for behavioral
        sampleRateEditor->setText("100");
        numChannelsEditor->setText("8");
        dataFormatCombo->setSelectedItemIndex(1); // JSON for flexibility
        bufferSizeEditor->setText("500");
        openEphysFormatButton->setToggleState(false, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }

    // Apply the preset
    applyToThread();
    updateValidationStatus();
}

void RedisConfigurationPanel::showHelpDialog()
{
    String helpText =
        "Redis Configuration Help\n\n"

        "CONNECTION SETTINGS:\n"
        "• Host: Redis server address (localhost for local server)\n"
        "• Port: Redis server port (default: 6379)\n"
        "• Password: Authentication password (optional)\n\n"

        "STREAM SETTINGS:\n"
        "• Channel: Redis channel/stream name for data\n"
        "• Stream Mode: Enable for Redis Streams (XREAD), disable for Lists (BLPOP)\n\n"

        "DATA FORMAT:\n"
        "• Sample Rate: Expected sampling frequency in Hz\n"
        "• Channels: Number of data channels per sample\n"
        "• Format: Data encoding format\n"
        "  - BRANDBCI: Native BRANDBCI format (recommended)\n"
        "  - JSON: Human-readable, flexible\n"
        "  - Binary: Highest performance\n\n"

        "ADVANCED SETTINGS:\n"
        "• Buffer Size: Internal buffer size (100-100000 samples)\n"
        "• OpenEphys Format: Enable native Open Ephys integration\n"
        "• Data Validation: Enable real-time data checking\n\n"

        "PRESETS:\n"
        "• Default: Basic configuration for testing\n"
        "• BRANDBCI Standard: Optimized for BRANDBCI systems\n"
        "• Local Testing: Low-resource configuration for development\n"
        "• High Performance: Maximum throughput configuration\n"
        "• Utah Array 96ch: Optimized for 96-channel Utah arrays\n"
        "• Neuropixels: High-density probe configuration (384 channels)\n"
        "• LFP Recording: Low-frequency local field potential recording\n"
        "• Behavioral Data: Low-rate behavioral/event data\n\n"

        "PERFORMANCE TIPS:\n"
        "• Use Binary format for highest performance\n"
        "• Enable Stream Mode for real-time applications\n"
        "• Adjust buffer size based on latency requirements\n"
        "• Disable Data Validation for maximum speed\n\n"

        "TROUBLESHOOTING:\n"
        "• Red fields indicate validation errors\n"
        "• Use Test button to verify connection\n"
        "• Check Redis server is running and accessible\n"
        "• Verify network connectivity and firewall settings";

    AlertWindow::showMessageBox(AlertWindow::InfoIcon,
                               "Redis Configuration Help",
                               helpText);
}

void RedisConfigurationPanel::testConnection()
{
    // Validate settings first
    if (!validateAllSettings())
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                   "Invalid Settings",
                                   "Please fix the highlighted configuration errors before testing connection.");
        return;
    }

    // Apply current settings to thread
    applyToThread();

    // Update UI to show testing state
    testConnectionButton->setEnabled(false);
    connectionStatus->setText("🟡 Testing connection...", dontSendNotification);
    connectionStatus->setColour(Label::textColourId, Colours::orange);

    // Test connection using the data thread
    bool connectionSuccess = false;
    String errorMessage = "";

    try
    {
        // Disconnect first if already connected
        if (dataThread->isConnected())
        {
            dataThread->disconnectFromRedis();
        }

        // Attempt to connect with current settings
        connectionSuccess = dataThread->connectToRedis(
            hostEditor->getText(),
            portEditor->getText().getIntValue(),
            passwordEditor->getText()
        );

        if (!connectionSuccess)
        {
            errorMessage = "Failed to connect to Redis server. Please check:\n"
                          "• Redis server is running\n"
                          "• Host and port are correct\n"
                          "• Network connectivity\n"
                          "• Firewall settings\n"
                          "• Authentication credentials";
        }
    }
    catch (const std::exception& e)
    {
        connectionSuccess = false;
        errorMessage = "Connection error: " + String(e.what());
    }

    // Update UI based on test result
    if (connectionSuccess)
    {
        connectionStatus->setText("🟢 Connection successful!", dontSendNotification);
        connectionStatus->setColour(Label::textColourId, Colours::green);

        AlertWindow::showMessageBox(AlertWindow::InfoIcon,
                                   "Connection Test",
                                   "✓ Successfully connected to Redis server!\n\n"
                                   "Host: " + hostEditor->getText() + "\n"
                                   "Port: " + portEditor->getText() + "\n"
                                   "Channel: " + channelEditor->getText());
    }
    else
    {
        connectionStatus->setText("🔴 Connection failed", dontSendNotification);
        connectionStatus->setColour(Label::textColourId, Colours::red);

        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                   "Connection Test Failed",
                                   errorMessage);
    }

    // Re-enable test button
    testConnectionButton->setEnabled(true);
}
