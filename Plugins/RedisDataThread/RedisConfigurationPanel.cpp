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
#include "RedisDataDisplayPopup.h"
#include "RedisConnectionTestPopup.h"
#include "RedisChannelScanPopup.h"
#include "RedisHelpPopup.h"
#include "RedisSavePresetPopup.h"
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

    // Set initial size - optimized height for better spacing
    setSize(420, 660);
}

RedisConfigurationPanel::~RedisConfigurationPanel()
{
    stopTimer();
}

void RedisConfigurationPanel::paint(Graphics& g)
{
    // Background
    g.fillAll(findColour(ThemeColours::widgetBackground));

    // Title - using consistent font size with other components
    g.setColour(findColour(ThemeColours::defaultText));
    g.setFont(FontOptions("Inter", "Bold", 18));
    g.drawText("Redis Configuration", 10, 5, getWidth() - 20, 25, Justification::centred);
}

void RedisConfigurationPanel::resized()
{
    int yPos = 35;
    int connectionGroupHeight = 150; // Increased to accommodate Test button
    int streamGroupHeight = 155;     // Increased height to accommodate View Data button
    int formatGroupHeight = 120;     // Keep original height
    int advancedGroupHeight = 120;   // Keep original height
    int margin = 10;
    int groupSpacing = 15;

    // Connection group
    if (connectionGroup)
    {
        connectionGroup->setBounds(margin, yPos, getWidth() - 2 * margin, connectionGroupHeight);
        yPos += connectionGroupHeight + groupSpacing;
    }

    // Stream group
    if (streamGroup)
    {
        streamGroup->setBounds(margin, yPos, getWidth() - 2 * margin, streamGroupHeight);
        yPos += streamGroupHeight + groupSpacing;
    }
    
    // Format group
    if (formatGroup)
    {
        formatGroup->setBounds(margin, yPos, getWidth() - 2 * margin, formatGroupHeight);
        yPos += formatGroupHeight + groupSpacing;
    }

    // Advanced group
    if (advancedGroup)
    {
        advancedGroup->setBounds(margin, yPos, getWidth() - 2 * margin, advancedGroupHeight);
        yPos += advancedGroupHeight + groupSpacing;
    }
    
    // Control buttons - redesigned with proper visual hierarchy and grouping
    // Help button (tertiary) - positioned on the left as utility
    if (helpButton)
        helpButton->setBounds(margin, yPos, 50, 26); // Slightly taller for better touch target

    // Configuration group - preset selection and reset (with visual separation)
    if (presetCombo)
        presetCombo->setBounds(margin + 70, yPos, 125, 26); // Grouped with reset, more space from help
    if (resetButton)
        resetButton->setBounds(margin + 205, yPos, 70, 26); // Danger action, needs more space

    // Primary action - save preset (most important, positioned on the right with separation)
    if (savePresetButton)
        savePresetButton->setBounds(margin + 295, yPos, 100, 26); // Primary button, larger, more separation

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

    int yOffset = 28; // Slightly more space from group title
    int rowHeight = 26; // Slightly more breathing room
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
    
    hostTooltip = std::make_unique<Label>("Host Tooltip", "(IP or hostname)");
    hostTooltip->setBounds(230, yOffset, 100, 20);
    hostTooltip->setFont(FontOptions("Inter", "Regular", 10));
    hostTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f)); // Use theme color with transparency
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
    
    portTooltip = std::make_unique<Label>("Port Tooltip", "(1-65535)");
    portTooltip->setBounds(190, yOffset, 80, 20);
    portTooltip->setFont(FontOptions("Inter", "Regular", 10));
    portTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
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
    
    passwordTooltip = std::make_unique<Label>("Password Tooltip", "(leave empty if none)");
    passwordTooltip->setBounds(230, yOffset, 120, 20);
    passwordTooltip->setFont(FontOptions("Inter", "Regular", 10));
    passwordTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    connectionGroup->addAndMakeVisible(passwordTooltip.get());

    yOffset += rowHeight + 10; // Optimized spacing for visual consistency

    // Test connection button - increased width to show full text
    testConnectionButton = std::make_unique<UtilityButton>("Test Connection");
    testConnectionButton->setBounds(15, yOffset, 140, 25);
    testConnectionButton->addListener(this);
    connectionGroup->addAndMakeVisible(testConnectionButton.get());
}

void RedisConfigurationPanel::createStreamGroup()
{
    streamGroup = std::make_unique<GroupComponent>("Stream", "Data Stream Settings");
    addAndMakeVisible(streamGroup.get());

    int yOffset = 28; // Consistent with other groups
    int rowHeight = 26; // Consistent spacing
    int labelWidth = 80;
    int editorWidth = 120;
    int tooltipWidth = 150;
    
    // Channel/Stream Name
    channelLabel = std::make_unique<Label>("Channel Label", "Channel:");
    channelLabel->setBounds(15, yOffset, labelWidth, 20);
    channelLabel->setFont(FontOptions("Inter", "Regular", 12));
    streamGroup->addAndMakeVisible(channelLabel.get());

    channelComboBox = std::make_unique<ComboBox>("Channel ComboBox");
    channelComboBox->setBounds(100, yOffset, 160, 20); // Optimized width: 160px (standard 120 + 40 for long names)
    channelComboBox->setEditableText(true); // Allow manual input
    channelComboBox->setTextWhenNothingSelected("neural_data");
    channelComboBox->addListener(this);
    streamGroup->addAndMakeVisible(channelComboBox.get());

    refreshChannelsButton = std::make_unique<UtilityButton>("R");
    refreshChannelsButton->setBounds(265, yOffset, 25, 20); // Positioned right after ComboBox with 5px gap
    refreshChannelsButton->setTooltip("Refresh available channels from Redis");
    refreshChannelsButton->addListener(this);
    streamGroup->addAndMakeVisible(refreshChannelsButton.get());

    channelTooltip = std::make_unique<Label>("Channel Tooltip", "(stream identifier)");
    channelTooltip->setBounds(295, yOffset, 120, 20); // Positioned after refresh button with 5px gap
    channelTooltip->setFont(FontOptions("Inter", "Regular", 10));
    channelTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
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
    
    streamModeTooltip = std::make_unique<Label>("Stream Mode Tooltip", "(real-time streaming)");
    streamModeTooltip->setBounds(130, yOffset, 160, 20);
    streamModeTooltip->setFont(FontOptions("Inter", "Regular", 10));
    streamModeTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    streamGroup->addAndMakeVisible(streamModeTooltip.get());

    yOffset += rowHeight;

    // Always Read Latest Mode
    alwaysLatestLabel = std::make_unique<Label>("Always Latest Label", "Always Latest:");
    alwaysLatestLabel->setBounds(15, yOffset, labelWidth, 20);
    alwaysLatestLabel->setFont(FontOptions("Inter", "Regular", 12));
    streamGroup->addAndMakeVisible(alwaysLatestLabel.get());

    alwaysLatestButton = std::make_unique<ToggleButton>("Always Latest");
    alwaysLatestButton->setBounds(100, yOffset, 20, 20);
    alwaysLatestButton->addListener(this);
    streamGroup->addAndMakeVisible(alwaysLatestButton.get());

    alwaysLatestTooltip = std::make_unique<Label>("Always Latest Tooltip", "(skip to newest)");
    alwaysLatestTooltip->setBounds(130, yOffset, 160, 20);
    alwaysLatestTooltip->setFont(FontOptions("Inter", "Regular", 10));
    alwaysLatestTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    streamGroup->addAndMakeVisible(alwaysLatestTooltip.get());

    yOffset += rowHeight + 15; // Adjusted spacing to align with other groups' bottom margins

    // Data button - significantly increased width to show full text
    dataButton = std::make_unique<UtilityButton>("View Data");
    dataButton->setBounds(15, yOffset, 160, 25); // Increased to 160px to ensure full text visibility
    dataButton->addListener(this);
    streamGroup->addAndMakeVisible(dataButton.get());
}

void RedisConfigurationPanel::textEditorTextChanged(TextEditor& editor)
{
    // Real-time validation
    if (&editor == hostEditor.get())
        validateField(hostEditor.get(), "host");
    else if (&editor == portEditor.get())
        validateField(portEditor.get(), "port");
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
    else if (comboBox == channelComboBox.get())
    {
        // Extract channel name from the selected item (remove type info)
        String selectedText = channelComboBox->getText();
        String channelName = selectedText.upToFirstOccurrenceOf(" (", false, false);

        // Update the combo box text to show just the channel name
        if (channelName.isNotEmpty() && channelName != selectedText)
        {
            channelComboBox->setText(channelName, dontSendNotification);
        }

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
    else if (button == alwaysLatestButton.get())
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
        loadPreset("Default (32ch, 30kHz)");
    }
    else if (button == savePresetButton.get())
    {
        // Create and show save preset popup
        auto popup = std::make_unique<RedisSavePresetPopup>(this);

        // Show popup using the PopupManager
        CoreServices::getPopupManager()->showPopup(std::move(popup), savePresetButton.get());
    }
    else if (button == helpButton.get())
    {
        showHelpDialog();
    }
    else if (button == dataButton.get())
    {
        showLatestData();
    }
    else if (button == refreshChannelsButton.get())
    {
        refreshAvailableChannels();
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

    int yOffset = 28; // Consistent with other groups
    int rowHeight = 26; // Consistent spacing
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

    sampleRateTooltip = std::make_unique<Label>("Sample Rate Tooltip", "(Hz)");
    sampleRateTooltip->setBounds(190, yOffset, 100, 20);
    sampleRateTooltip->setFont(FontOptions("Inter", "Regular", 10));
    sampleRateTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
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

    numChannelsTooltip = std::make_unique<Label>("Num Channels Tooltip", "(1-1024)");
    numChannelsTooltip->setBounds(190, yOffset, 120, 20);
    numChannelsTooltip->setFont(FontOptions("Inter", "Regular", 10));
    numChannelsTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    formatGroup->addAndMakeVisible(numChannelsTooltip.get());

    yOffset += rowHeight;

    // Data Format
    dataFormatLabel = std::make_unique<Label>("Data Format Label", "Format:");
    dataFormatLabel->setBounds(15, yOffset, labelWidth, 20);
    dataFormatLabel->setFont(FontOptions("Inter", "Regular", 12));
    formatGroup->addAndMakeVisible(dataFormatLabel.get());

    dataFormatCombo = std::make_unique<ComboBox>("Data Format Combo");
    dataFormatCombo->setBounds(100, yOffset, 160, 20); // Further increased to show full text
    dataFormatCombo->addItem("BRANDBCI (recommended)", 1);
    dataFormatCombo->addItem("JSON (general)", 2);
    dataFormatCombo->addItem("Binary (performance)", 3);
    dataFormatCombo->addListener(this);
    formatGroup->addAndMakeVisible(dataFormatCombo.get());

    dataFormatTooltip = std::make_unique<Label>("Data Format Tooltip", "(encoding type)");
    dataFormatTooltip->setBounds(270, yOffset, 120, 20); // Adjusted position
    dataFormatTooltip->setFont(FontOptions("Inter", "Regular", 10));
    dataFormatTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    formatGroup->addAndMakeVisible(dataFormatTooltip.get());
}

void RedisConfigurationPanel::createAdvancedGroup()
{
    advancedGroup = std::make_unique<GroupComponent>("Advanced", "Advanced Settings");
    addAndMakeVisible(advancedGroup.get());

    int yOffset = 28; // Consistent with other groups
    int rowHeight = 26; // Consistent spacing
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

    bufferSizeTooltip = std::make_unique<Label>("Buffer Size Tooltip", "(samples)");
    bufferSizeTooltip->setBounds(210, yOffset, 120, 20);
    bufferSizeTooltip->setFont(FontOptions("Inter", "Regular", 10));
    bufferSizeTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    advancedGroup->addAndMakeVisible(bufferSizeTooltip.get());

    yOffset += rowHeight + 8; // Add more space to create visual separation

    // Format Options section (grouped together for better logic)
    // Open Ephys Format
    openEphysFormatLabel = std::make_unique<Label>("OpenEphys Format Label", "OpenEphys Format:");
    openEphysFormatLabel->setBounds(15, yOffset, labelWidth, 20);
    openEphysFormatLabel->setFont(FontOptions("Inter", "Regular", 12));
    advancedGroup->addAndMakeVisible(openEphysFormatLabel.get());

    openEphysFormatButton = std::make_unique<ToggleButton>("OpenEphys Format");
    openEphysFormatButton->setBounds(120, yOffset, 20, 20);
    openEphysFormatButton->addListener(this);
    advancedGroup->addAndMakeVisible(openEphysFormatButton.get());

    openEphysFormatTooltip = std::make_unique<Label>("OpenEphys Format Tooltip", "(native format)");
    openEphysFormatTooltip->setBounds(150, yOffset, 100, 20);
    openEphysFormatTooltip->setFont(FontOptions("Inter", "Regular", 10));
    openEphysFormatTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    advancedGroup->addAndMakeVisible(openEphysFormatTooltip.get());

    yOffset += rowHeight; // Normal spacing for related items

    // Data Validation - logically grouped with format options
    dataValidationLabel = std::make_unique<Label>("Data Validation Label", "Data Validation:");
    dataValidationLabel->setBounds(15, yOffset, labelWidth, 20);
    dataValidationLabel->setFont(FontOptions("Inter", "Regular", 12));
    advancedGroup->addAndMakeVisible(dataValidationLabel.get());

    dataValidationButton = std::make_unique<ToggleButton>("Data Validation");
    dataValidationButton->setBounds(120, yOffset, 20, 20);
    dataValidationButton->addListener(this);
    advancedGroup->addAndMakeVisible(dataValidationButton.get());

    dataValidationTooltip = std::make_unique<Label>("Data Validation Tooltip", "(error checking)");
    dataValidationTooltip->setBounds(150, yOffset, 100, 20);
    dataValidationTooltip->setFont(FontOptions("Inter", "Regular", 10));
    dataValidationTooltip->setColour(Label::textColourId, findColour(ThemeColours::defaultText).withAlpha(0.6f));
    advancedGroup->addAndMakeVisible(dataValidationTooltip.get());
}

void RedisConfigurationPanel::createControlButtons()
{
    // Preset selector (using theme colors)
    presetCombo = std::make_unique<ComboBox>("Preset Combo");
    presetCombo->addListener(this);
    // Use theme colors for consistent appearance
    presetCombo->setColour(ComboBox::backgroundColourId, findColour(ThemeColours::widgetBackground));
    presetCombo->setColour(ComboBox::textColourId, findColour(ThemeColours::defaultText));
    presetCombo->setColour(ComboBox::outlineColourId, findColour(ThemeColours::outline));
    addAndMakeVisible(presetCombo.get());

    // Reset button (using standard UtilityButton with theme colors)
    resetButton = std::make_unique<UtilityButton>("Reset");
    resetButton->addListener(this);
    addAndMakeVisible(resetButton.get());

    // Save preset button (using standard UtilityButton with theme colors)
    savePresetButton = std::make_unique<UtilityButton>("Save Preset");
    savePresetButton->addListener(this);
    addAndMakeVisible(savePresetButton.get());

    // Help button (using standard UtilityButton with theme colors)
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
    // Create custom tooltip window for better formatting
    setupCustomTooltips();
}

void RedisConfigurationPanel::setupCustomTooltips()
{
    // Set simple, clear tooltips without confusing instructions
    hostEditor->setTooltip("Redis Server Address");
    portEditor->setTooltip("Redis Server Port");
    passwordEditor->setTooltip("Redis Authentication Password");
    channelComboBox->setTooltip("Redis Channel/Stream Name");
    streamModeButton->setTooltip("Enable Redis Stream Mode");
    alwaysLatestButton->setTooltip("Always Read Latest Data");
    sampleRateEditor->setTooltip("Sampling Rate (Hz)");
    numChannelsEditor->setTooltip("Number of Channels");
    dataFormatCombo->setTooltip("Data Encoding Format");
    bufferSizeEditor->setTooltip("Buffer Size (Samples)");
    openEphysFormatButton->setTooltip("Enable Open Ephys Format");
    dataValidationButton->setTooltip("Enable Data Validation");
}



void RedisConfigurationPanel::setupPresets()
{
    presetCombo->addItem("Select Preset...", 1);
    presetCombo->addItem("Default (32ch, 30kHz)", 2);
    presetCombo->addItem("High Density (96ch, 30kHz)", 3);
    presetCombo->addItem("Low Frequency (32ch, 1kHz)", 4);
    presetCombo->addItem("Testing (8ch, 1kHz)", 5);

    // Load custom presets from files
    File appDataDir = File::getSpecialLocation(File::userApplicationDataDirectory);
    File presetsDir = appDataDir.getChildFile("Open Ephys").getChildFile("RedisDataThread").getChildFile("Presets");

    if (presetsDir.exists())
    {
        Array<File> presetFiles;
        presetsDir.findChildFiles(presetFiles, File::findFiles, false, "*.xml");

        for (const File& file : presetFiles)
        {
            String presetName = file.getFileNameWithoutExtension();
            presetCombo->addItem(presetName, presetCombo->getNumItems() + 1);
        }
    }

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
    channelComboBox->setText(dataThread->getRedisChannelName(), dontSendNotification);
    streamModeButton->setToggleState(dataThread->getStreamMode(), dontSendNotification);
    alwaysLatestButton->setToggleState(dataThread->getAlwaysReadLatest(), dontSendNotification);

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
    dataThread->setRedisChannel(channelComboBox->getText());
    dataThread->setStreamMode(streamModeButton->getToggleState());
    dataThread->setAlwaysReadLatest(alwaysLatestButton->getToggleState());

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
    if (channelComboBox->getText().isEmpty())
    {
        showFieldError(channelComboBox.get(), "Channel cannot be empty");
        allValid = false;
    }
    else
    {
        clearFieldError(channelComboBox.get());
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
    if (presetName == "Default (32ch, 30kHz)")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelComboBox->setText("neural_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        alwaysLatestButton->setToggleState(false, dontSendNotification); // Disable for complete data recording
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("32");
        dataFormatCombo->setSelectedItemIndex(0); // BRANDBCI
        bufferSizeEditor->setText("5000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "High Density (96ch, 30kHz)")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelComboBox->setText("neural_data_hd");
        streamModeButton->setToggleState(true, dontSendNotification);
        alwaysLatestButton->setToggleState(false, dontSendNotification); // Disable for complete data recording
        sampleRateEditor->setText("30000");
        numChannelsEditor->setText("96");
        dataFormatCombo->setSelectedItemIndex(0); // BRANDBCI
        bufferSizeEditor->setText("3000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "Low Frequency (32ch, 1kHz)")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelComboBox->setText("lfp_data");
        streamModeButton->setToggleState(true, dontSendNotification);
        alwaysLatestButton->setToggleState(false, dontSendNotification); // Disable for LFP recording
        sampleRateEditor->setText("1000");
        numChannelsEditor->setText("32");
        dataFormatCombo->setSelectedItemIndex(0); // BRANDBCI
        bufferSizeEditor->setText("2000");
        openEphysFormatButton->setToggleState(true, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }
    else if (presetName == "Testing (8ch, 1kHz)")
    {
        hostEditor->setText("localhost");
        portEditor->setText("6379");
        passwordEditor->setText("");
        channelComboBox->setText("test_data");
        streamModeButton->setToggleState(false, dontSendNotification);
        alwaysLatestButton->setToggleState(true, dontSendNotification); // Enable for testing
        sampleRateEditor->setText("1000");
        numChannelsEditor->setText("8");
        dataFormatCombo->setSelectedItemIndex(1); // JSON
        bufferSizeEditor->setText("1000");
        openEphysFormatButton->setToggleState(false, dontSendNotification);
        dataValidationButton->setToggleState(true, dontSendNotification);
    }

    else
    {
        // Try to load custom preset from file
        File appDataDir = File::getSpecialLocation(File::userApplicationDataDirectory);
        File presetsDir = appDataDir.getChildFile("Open Ephys").getChildFile("RedisDataThread").getChildFile("Presets");
        File presetFile = presetsDir.getChildFile(presetName + ".xml");

        if (presetFile.exists())
        {
            XmlDocument doc(presetFile);
            std::unique_ptr<XmlElement> xml = doc.getDocumentElement();

            if (xml != nullptr && xml->hasTagName("REDIS_PRESET"))
            {
                // Load custom preset
                hostEditor->setText(xml->getStringAttribute("redisHost", "localhost"));
                portEditor->setText(xml->getStringAttribute("redisPort", "6379"));
                passwordEditor->setText(xml->getStringAttribute("redisPassword", ""));
                channelComboBox->setText(xml->getStringAttribute("redisChannel", "neural_data"));
                streamModeButton->setToggleState(xml->getBoolAttribute("streamMode", true), dontSendNotification);
                alwaysLatestButton->setToggleState(xml->getBoolAttribute("alwaysReadLatest", true), dontSendNotification);
                sampleRateEditor->setText(xml->getStringAttribute("sampleRate", "30000"));
                numChannelsEditor->setText(xml->getStringAttribute("numChannels", "32"));
                dataFormatCombo->setSelectedItemIndex(xml->getIntAttribute("dataFormat", 0));
                bufferSizeEditor->setText(xml->getStringAttribute("bufferSize", "5000"));
                openEphysFormatButton->setToggleState(xml->getBoolAttribute("openEphysFormat", true), dontSendNotification);
                dataValidationButton->setToggleState(xml->getBoolAttribute("dataValidation", true), dontSendNotification);

                LOGD("Loaded custom preset: ", presetName);
            }
            else
            {
                AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                           "Load Error",
                                           "Invalid preset file format: " + presetName);
                return;
            }
        }
        else
        {
            AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                       "Preset Not Found",
                                       "Preset '" + presetName + "' not found.");
            return;
        }
    }

    // Apply the preset
    applyToThread();
    updateValidationStatus();
}

void RedisConfigurationPanel::showHelpDialog()
{
    // Create and show help popup
    auto popup = std::make_unique<RedisHelpPopup>();

    // Show popup using the PopupManager
    CoreServices::getPopupManager()->showPopup(std::move(popup), helpButton.get());
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
    }
    else
    {
        connectionStatus->setText("🔴 Connection failed", dontSendNotification);
        connectionStatus->setColour(Label::textColourId, Colours::red);
    }

    // Create and show popup with the test result
    auto popup = std::make_unique<RedisConnectionTestPopup>(
        connectionSuccess,
        hostEditor->getText(),
        portEditor->getText(),
        channelComboBox->getText(),
        errorMessage
    );

    // Show popup using the PopupManager
    CoreServices::getPopupManager()->showPopup(std::move(popup), testConnectionButton.get());

    // Re-enable test button
    testConnectionButton->setEnabled(true);
}

void RedisConfigurationPanel::showLatestData()
{
    if (!dataThread->isConnected())
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                   "Not Connected",
                                   "Please connect to Redis server first before retrieving data.");
        return;
    }

    // Retrieve latest records from Redis
    Array<String> records = dataThread->getLatestRecords(10);

    // Create and show popup with the data
    auto popup = std::make_unique<RedisDataDisplayPopup>(records, dataThread->getDataFormat());

    // Show popup using the PopupManager
    CoreServices::getPopupManager()->showPopup(std::move(popup), dataButton.get());
}

void RedisConfigurationPanel::refreshAvailableChannels()
{
    if (!dataThread->isConnected())
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                   "Not Connected",
                                   "Please connect to Redis server first to scan for available channels.");
        return;
    }

    // Disable refresh button temporarily
    refreshChannelsButton->setEnabled(false);
    refreshChannelsButton->setButtonText("...");

    // Get available channels from Redis
    Array<String> channels = dataThread->getAvailableChannels();

    // Clear existing items
    channelComboBox->clear();

    if (channels.isEmpty())
    {
        channelComboBox->addItem("No channels found", 1);
        channelComboBox->setSelectedItemIndex(0);

        // Will show popup after ComboBox update
    }
    else
    {
        // Add found channels to combo box with optimized display
        for (int i = 0; i < channels.size(); i++)
        {
            String channelInfo = channels[i];
            String channelName = channelInfo.upToFirstOccurrenceOf(" (", false, false);

            // For very long channel names, create a shortened display version for the dropdown
            String displayText = channelInfo;
            if (channelInfo.length() > 45) // If text is too long for comfortable reading
            {
                String shortName = channelName.length() > 25 ?
                    channelName.substring(0, 22) + "..." : channelName;
                String typeInfo = channelInfo.fromFirstOccurrenceOf(" (", true, false);
                displayText = shortName + typeInfo;
            }

            channelComboBox->addItem(displayText, i + 1);
        }

        // Try to select current channel if it exists
        String currentChannel = channelComboBox->getText();
        bool found = false;
        for (int i = 0; i < channels.size(); i++)
        {
            if (channels[i].startsWith(currentChannel))
            {
                channelComboBox->setSelectedItemIndex(i);
                found = true;
                break;
            }
        }

        if (!found && channels.size() > 0)
        {
            channelComboBox->setSelectedItemIndex(0);
        }

    }

    // Create and show popup with scan results (consistent with Test Connection)
    auto popup = std::make_unique<RedisChannelScanPopup>(
        !channels.isEmpty(),  // success = true if channels found
        channels,             // channel list
        channels.isEmpty() ? "No channels found in Redis database" : ""
    );

    // Show popup using the PopupManager (same as Test Connection)
    CoreServices::getPopupManager()->showPopup(std::move(popup), refreshChannelsButton.get());

    // Re-enable refresh button
    refreshChannelsButton->setEnabled(true);
    refreshChannelsButton->setButtonText("R");
}

void RedisConfigurationPanel::savePreset(const String& presetName)
{
    // Get the application data directory
    File appDataDir = File::getSpecialLocation(File::userApplicationDataDirectory);
    File openEphysDir = appDataDir.getChildFile("Open Ephys");
    File presetsDir = openEphysDir.getChildFile("RedisDataThread").getChildFile("Presets");

    // Create directories if they don't exist
    if (!presetsDir.exists())
    {
        presetsDir.createDirectory();
    }

    // Create preset file
    File presetFile = presetsDir.getChildFile(presetName + ".xml");

    // Create XML document
    XmlElement xml("REDIS_PRESET");
    xml.setAttribute("name", presetName);
    xml.setAttribute("version", "1.0");

    // Save current configuration
    xml.setAttribute("redisHost", hostEditor->getText());
    xml.setAttribute("redisPort", portEditor->getText());
    xml.setAttribute("redisPassword", passwordEditor->getText());
    xml.setAttribute("redisChannel", channelComboBox->getText());
    xml.setAttribute("streamMode", streamModeButton->getToggleState());
    xml.setAttribute("alwaysReadLatest", alwaysLatestButton->getToggleState());
    xml.setAttribute("sampleRate", sampleRateEditor->getText());
    xml.setAttribute("numChannels", numChannelsEditor->getText());
    xml.setAttribute("dataFormat", dataFormatCombo->getSelectedItemIndex());
    xml.setAttribute("bufferSize", bufferSizeEditor->getText());
    xml.setAttribute("openEphysFormat", openEphysFormatButton->getToggleState());
    xml.setAttribute("dataValidation", dataValidationButton->getToggleState());

    // Write to file
    if (!xml.writeTo(presetFile))
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                   "Save Error",
                                   "Failed to save preset to: " + presetFile.getFullPathName());
    }
    else
    {
        LOGD("Preset saved: ", presetFile.getFullPathName());

        // Add to combo box if not already present
        bool found = false;
        for (int i = 0; i < presetCombo->getNumItems(); i++)
        {
            if (presetCombo->getItemText(i) == presetName)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            presetCombo->addItem(presetName, presetCombo->getNumItems() + 1);
        }
    }
}
