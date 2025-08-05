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

#include "RedisDataThreadEditor.h"
#include "RedisDataThread.h"
#include "RedisDataDisplayPopup.h"
#include <CoreServicesHeader.h>

RedisDataThreadEditor::RedisDataThreadEditor(GenericProcessor* parentNode, RedisDataThread* thread)
    : GenericEditor(parentNode)
    , dataThread(thread)
{
    desiredWidth = 250;

    // Create compact interface with just essential controls
    createStatusControls();

    updateSettings();

    // Start timer for status updates
    startTimer(1000); // Update every second
}

RedisDataThreadEditor::~RedisDataThreadEditor()
{
    stopTimer();
}

void RedisDataThreadEditor::createConnectionControls()
{
    // Host
    hostLabel = std::make_unique<Label>("Host Label", "Host:");
    hostLabel->setBounds(10, 30, 50, 20);
    hostLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(hostLabel.get());

    hostEditor = std::make_unique<TextEditor>("Host Editor");
    hostEditor->setBounds(65, 30, 100, 20);
    hostEditor->setText(dataThread->getRedisHost());
    hostEditor->addListener(this);
    addAndMakeVisible(hostEditor.get());

    // Port
    portLabel = std::make_unique<Label>("Port Label", "Port:");
    portLabel->setBounds(175, 30, 35, 20);
    portLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(portLabel.get());

    portEditor = std::make_unique<TextEditor>("Port Editor");
    portEditor->setBounds(215, 30, 60, 20);
    portEditor->setText(String(dataThread->getRedisPort()));
    portEditor->addListener(this);
    addAndMakeVisible(portEditor.get());

    // Password
    passwordLabel = std::make_unique<Label>("Password Label", "Password:");
    passwordLabel->setBounds(10, 55, 70, 20);
    passwordLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(passwordLabel.get());

    passwordEditor = std::make_unique<TextEditor>("Password Editor");
    passwordEditor->setBounds(85, 55, 120, 20);
    passwordEditor->setPasswordCharacter('*');
    passwordEditor->addListener(this);
    addAndMakeVisible(passwordEditor.get());

    // Channel
    channelLabel = std::make_unique<Label>("Channel Label", "Channel:");
    channelLabel->setBounds(10, 80, 60, 20);
    channelLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(channelLabel.get());

    channelEditor = std::make_unique<TextEditor>("Channel Editor");
    channelEditor->setBounds(75, 80, 130, 20);
    channelEditor->setText(dataThread->getRedisChannel());
    channelEditor->addListener(this);
    addAndMakeVisible(channelEditor.get());
}

void RedisDataThreadEditor::createDataControls()
{
    // Sample Rate
    sampleRateLabel = std::make_unique<Label>("Sample Rate Label", "Sample Rate:");
    sampleRateLabel->setBounds(10, 110, 80, 20);
    sampleRateLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(sampleRateLabel.get());

    sampleRateEditor = std::make_unique<TextEditor>("Sample Rate Editor");
    sampleRateEditor->setBounds(95, 110, 70, 20);
    sampleRateEditor->setText(String(dataThread->getSampleRate()));
    sampleRateEditor->addListener(this);
    addAndMakeVisible(sampleRateEditor.get());

    // Number of Channels
    numChannelsLabel = std::make_unique<Label>("Num Channels Label", "Channels:");
    numChannelsLabel->setBounds(175, 110, 60, 20);
    numChannelsLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(numChannelsLabel.get());

    numChannelsEditor = std::make_unique<TextEditor>("Num Channels Editor");
    numChannelsEditor->setBounds(240, 110, 50, 20);
    numChannelsEditor->setText(String(dataThread->getNumChannels()));
    numChannelsEditor->addListener(this);
    addAndMakeVisible(numChannelsEditor.get());

    // Data Format
    dataFormatLabel = std::make_unique<Label>("Data Format Label", "Format:");
    dataFormatLabel->setBounds(10, 135, 50, 20);
    dataFormatLabel->setFont(Font("Small Text", 12, Font::plain));
    addAndMakeVisible(dataFormatLabel.get());

    dataFormatCombo = std::make_unique<ComboBox>("Data Format Combo");
    dataFormatCombo->setBounds(65, 135, 80, 20);
    dataFormatCombo->addItem("JSON", 1);
    dataFormatCombo->addItem("Binary", 2);
    dataFormatCombo->setSelectedItemIndex(dataThread->getDataFormat() == "json" ? 0 : 1);
    dataFormatCombo->addListener(this);
    addAndMakeVisible(dataFormatCombo.get());
}

void RedisDataThreadEditor::createStatusControls()
{
    // Use standard Open Ephys layout positioning (x=24, y starting at 29)
    int xPos = 24;
    int yPos = 29;
    int buttonHeight = 20;
    int spacing = 25;

    // Connection info display (similar to File Source's file path display)
    connectionInfoLabel = std::make_unique<Label>("Connection Info", "Redis: Not configured");
    connectionInfoLabel->setBounds(xPos, yPos, desiredWidth - 30, buttonHeight);
    connectionInfoLabel->setFont(FontOptions("Inter", "Regular", 12));
    connectionInfoLabel->setColour(Label::textColourId, findColour(ThemeColours::defaultText));
    connectionInfoLabel->setColour(Label::backgroundColourId, findColour(ThemeColours::widgetBackground));
    connectionInfoLabel->setBorderSize(BorderSize<int>(1));
    connectionInfoLabel->setJustificationType(Justification::centredLeft);
    addAndMakeVisible(connectionInfoLabel.get());

    yPos += spacing;

    // Configure button - opens popup with all settings
    configureButton = std::make_unique<UtilityButton>("Configure...");
    configureButton->setBounds(xPos, yPos, 80, buttonHeight);
    configureButton->addListener(this);
    addAndMakeVisible(configureButton.get());

    // Connect button
    connectButton = std::make_unique<UtilityButton>("Connect");
    connectButton->setBounds(xPos + 90, yPos, 70, buttonHeight);
    connectButton->setClickingTogglesState(true);
    connectButton->addListener(this);
    addAndMakeVisible(connectButton.get());

    // Test button
    testButton = std::make_unique<UtilityButton>("Test");
    testButton->setBounds(xPos + 170, yPos, 50, buttonHeight);
    testButton->addListener(this);
    addAndMakeVisible(testButton.get());

    yPos += spacing;

    // Status label and value
    statusLabel = std::make_unique<Label>("Status Label", "Status:");
    statusLabel->setBounds(xPos, yPos, 50, buttonHeight);
    statusLabel->setFont(FontOptions("Inter", "Regular", 12));
    statusLabel->setColour(Label::textColourId, findColour(ThemeColours::defaultText));
    addAndMakeVisible(statusLabel.get());

    statusValueLabel = std::make_unique<Label>("Status Value Label", "Disconnected");
    statusValueLabel->setBounds(xPos + 55, yPos, 165, buttonHeight);
    statusValueLabel->setFont(FontOptions("Inter", "Regular", 12));
    statusValueLabel->setColour(Label::textColourId, Colours::red);
    addAndMakeVisible(statusValueLabel.get());

    yPos += spacing;

    // Data button
    dataButton = std::make_unique<UtilityButton>("Data");
    dataButton->setBounds(xPos, yPos, 60, buttonHeight);
    dataButton->addListener(this);
    addAndMakeVisible(dataButton.get());
}

void RedisDataThreadEditor::paint(Graphics& g)
{
    // Use standard GenericEditor paint method for consistent styling
    GenericEditor::paint(g);
}

void RedisDataThreadEditor::resized()
{
    GenericEditor::resized();

    // Reposition components using standard layout patterns
    if (!getCollapsedState())
    {
        int xPos = 24;
        int yPos = 29;
        int buttonHeight = 20;
        int spacing = 25;
        int availableWidth = desiredWidth - 30;

        // Connection info display
        if (connectionInfoLabel)
            connectionInfoLabel->setBounds(xPos, yPos, availableWidth, buttonHeight);

        yPos += spacing;

        // Button row
        if (configureButton)
            configureButton->setBounds(xPos, yPos, 80, buttonHeight);
        if (connectButton)
            connectButton->setBounds(xPos + 90, yPos, 70, buttonHeight);
        if (testButton)
            testButton->setBounds(xPos + 170, yPos, 50, buttonHeight);

        yPos += spacing;

        // Status row
        if (statusLabel)
            statusLabel->setBounds(xPos, yPos, 50, buttonHeight);
        if (statusValueLabel)
            statusValueLabel->setBounds(xPos + 55, yPos, 165, buttonHeight);

        yPos += spacing;

        // Data button
        if (dataButton)
            dataButton->setBounds(xPos, yPos, 60, buttonHeight);
    }
}

void RedisDataThreadEditor::textEditorTextChanged(TextEditor& editor)
{
    // Real-time validation could be added here
}

void RedisDataThreadEditor::textEditorReturnKeyPressed(TextEditor& editor)
{
    applySettings();
}

void RedisDataThreadEditor::textEditorFocusLost(TextEditor& editor)
{
    applySettings();
}

void RedisDataThreadEditor::comboBoxChanged(ComboBox* comboBox)
{
    if (comboBox == dataFormatCombo.get())
    {
        applySettings();
    }
}

void RedisDataThreadEditor::buttonClicked(Button* button)
{
    if (button == configureButton.get())
    {
        showConfigurationDialog();
    }
    else if (button == connectButton.get())
    {
        if (connectButton->getToggleState())
        {
            // Apply settings and connect
            if (validateSettings())
            {
                applySettings();
                if (dataThread->foundInputSource())
                {
                    connectButton->setLabel("Disconnect");
                }
                else
                {
                    connectButton->setToggleState(false, dontSendNotification);
                    AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                               "Connection Failed",
                                               "Could not connect to Redis server. Please check your settings.");
                }
            }
            else
            {
                connectButton->setToggleState(false, dontSendNotification);
            }
        }
        else
        {
            // Disconnect
            dataThread->disconnectFromRedis();
            connectButton->setLabel("Connect");
        }
    }
    else if (button == testButton.get())
    {
        // Test connection without changing state
        if (validateSettings())
        {
            String host = dataThread->getRedisHost();
            int port = dataThread->getRedisPort();
            String password = dataThread->getRedisPassword();

            RedisDataThread tempThread(nullptr);
            if (tempThread.connectToRedis(host, port, password))
            {
                AlertWindow::showMessageBox(AlertWindow::InfoIcon,
                                           "Connection Test",
                                           "Successfully connected to Redis server!");
                tempThread.disconnectFromRedis();
            }
            else
            {
                AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                           "Connection Test",
                                           "Failed to connect to Redis server.");
            }
        }
    }
    else if (button == dataButton.get())
    {
        // Show latest data from Redis
        showLatestData();
    }
}

void RedisDataThreadEditor::timerCallback()
{
    updateConnectionStatus();
}

void RedisDataThreadEditor::updateConnectionStatus()
{
    String status = dataThread->getConnectionStatus();
    statusValueLabel->setText(status, dontSendNotification);

    if (dataThread->isConnected())
    {
        statusValueLabel->setColour(Label::textColourId, Colours::green);
        connectButton->setToggleState(true, dontSendNotification);
        connectButton->setLabel("Disconnect");
    }
    else
    {
        statusValueLabel->setColour(Label::textColourId, Colours::red);
        connectButton->setToggleState(false, dontSendNotification);
        connectButton->setLabel("Connect");
    }
}

void RedisDataThreadEditor::updateSettings()
{
    // Update connection info display
    String connectionInfo = "Redis: " + dataThread->getRedisHost() + ":" + String(dataThread->getRedisPort());
    if (!dataThread->getRedisChannel().isEmpty())
    {
        connectionInfo += " (" + dataThread->getRedisChannel() + ")";
    }
    connectionInfoLabel->setText(connectionInfo, dontSendNotification);

    // Update connection status
    updateConnectionStatus();
}

void RedisDataThreadEditor::applySettings()
{
    // In compact mode, settings are applied through the configuration dialog
    // This method is called when connecting, so we just validate current settings
    if (!validateSettings())
        return;

    // Settings are already stored in the dataThread object
    // No need to read from editors since they don't exist in compact mode
}

bool RedisDataThreadEditor::validateSettings()
{
    // Validate host
    if (dataThread->getRedisHost().isEmpty())
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings", "Host cannot be empty.");
        return false;
    }

    // Validate port
    int port = dataThread->getRedisPort();
    if (port <= 0 || port > 65535)
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings", "Port must be between 1 and 65535.");
        return false;
    }

    // Validate channel
    if (dataThread->getRedisChannel().isEmpty())
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings", "Channel cannot be empty.");
        return false;
    }

    // Validate sample rate
    float sampleRate = dataThread->getSampleRate();
    if (sampleRate <= 0)
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings", "Sample rate must be positive.");
        return false;
    }

    // Validate number of channels
    int numChannels = dataThread->getNumChannels();
    if (numChannels <= 0 || numChannels > 1024)
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings", "Number of channels must be between 1 and 1024.");
        return false;
    }

    return true;
}

void RedisDataThreadEditor::showConfigurationDialog()
{
    // Create a dialog window with all Redis configuration options
    AlertWindow configDialog("Redis Configuration",
                            "Configure Redis connection settings",
                            AlertWindow::NoIcon);

    configDialog.addTextEditor("host", dataThread->getRedisHost(), "Host:");
    configDialog.addTextEditor("port", String(dataThread->getRedisPort()), "Port:");
    configDialog.addTextEditor("password", dataThread->getRedisPassword(), "Password:");
    configDialog.addTextEditor("channel", dataThread->getRedisChannel(), "Channel:");
    configDialog.addTextEditor("sampleRate", String(dataThread->getSampleRate()), "Sample Rate:");
    configDialog.addTextEditor("numChannels", String(dataThread->getNumChannels()), "Channels:");

    configDialog.addComboBox("dataFormat", StringArray("JSON", "Binary"), "Format:");
    configDialog.getComboBoxComponent("dataFormat")->setSelectedItemIndex(
        dataThread->getDataFormat() == "json" ? 0 : 1);

    configDialog.addButton("OK", 1, KeyPress(KeyPress::returnKey));
    configDialog.addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

    if (configDialog.runModalLoop() == 1) // OK button pressed
    {
        // Apply the settings from the dialog
        dataThread->setRedisHost(configDialog.getTextEditorContents("host"));
        dataThread->setRedisPort(configDialog.getTextEditorContents("port").getIntValue());
        dataThread->setRedisPassword(configDialog.getTextEditorContents("password"));
        dataThread->setRedisChannel(configDialog.getTextEditorContents("channel"));
        dataThread->setSampleRate(configDialog.getTextEditorContents("sampleRate").getFloatValue());
        dataThread->setNumChannels(configDialog.getTextEditorContents("numChannels").getIntValue());

        String selectedFormat = configDialog.getComboBoxComponent("dataFormat")->getSelectedItemIndex() == 0 ? "json" : "binary";
        dataThread->setDataFormat(selectedFormat);

        updateSettings();
    }
}

void RedisDataThreadEditor::startAcquisition()
{
    // Disable editing during acquisition
    configureButton->setEnabled(false);
    connectButton->setEnabled(false);
    testButton->setEnabled(false);
    // Keep Data button enabled during acquisition
}

void RedisDataThreadEditor::stopAcquisition()
{
    // Re-enable editing after acquisition
    configureButton->setEnabled(true);
    connectButton->setEnabled(true);
    testButton->setEnabled(true);
    // Data button remains enabled
}

void RedisDataThreadEditor::showLatestData()
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
