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
#include "RedisConfigurationPanel.h"
#include "RedisConfigurationPopup.h"
#include <CoreServicesHeader.h>

RedisDataThreadEditor::RedisDataThreadEditor(GenericProcessor* parentNode, RedisDataThread* thread)
    : GenericEditor(parentNode)
    , dataThread(thread)
    , currentConfigPopup(nullptr)
{
    desiredWidth = 250;

    // Create compact UI similar to File Reader (no parameter editors in main view)
    createCompactInterface();

    updateSettings();

    // Start timer for status updates
    startTimer(1000); // Update every second
}

RedisDataThreadEditor::~RedisDataThreadEditor()
{
    stopTimer();
}



void RedisDataThreadEditor::createCompactInterface()
{
    // Create a compact interface similar to File Reader
    // Use standard Open Ephys layout: x=24, y starting at 29, spacing=25

    int xPos = 24;
    int yPos = 29;
    int buttonHeight = 20;
    int spacing = 25;

    // Connection info display (similar to File Reader's file path display)
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
}



void RedisDataThreadEditor::paint(Graphics& g)
{
    // Use standard GenericEditor paint method for consistent styling
    GenericEditor::paint(g);
}

void RedisDataThreadEditor::resized()
{
    GenericEditor::resized();

    // Reposition compact interface components
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

        yPos += spacing;

        // Status row
        if (statusLabel)
            statusLabel->setBounds(xPos, yPos, 50, buttonHeight);
        if (statusValueLabel)
            statusValueLabel->setBounds(xPos + 55, yPos, 165, buttonHeight);
    }
}

void RedisDataThreadEditor::textEditorTextChanged(TextEditor& editor)
{
    // No longer needed - using parameter system
}

void RedisDataThreadEditor::textEditorReturnKeyPressed(TextEditor& editor)
{
    // No longer needed - using parameter system
}

void RedisDataThreadEditor::textEditorFocusLost(TextEditor& editor)
{
    // No longer needed - using parameter system
}

void RedisDataThreadEditor::comboBoxChanged(ComboBox* comboBox)
{
    // No longer needed - using parameter system
}

void RedisDataThreadEditor::buttonClicked(Button* button)
{
    if (button == configureButton.get())
    {
        currentConfigPopup = new RedisConfigurationPopup(this, dataThread);
        CoreServices::getPopupManager()->showPopup(std::unique_ptr<PopupComponent>(currentConfigPopup), button);
        currentConfigPopup->addComponentListener(this);
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
    // Update connection info display using dataThread values (more reliable)
    String connectionInfo = "Redis: " + dataThread->getRedisHost() + ":" + String(dataThread->getRedisPort());
    if (!dataThread->getRedisChannelName().isEmpty())
    {
        connectionInfo += " (" + dataThread->getRedisChannelName() + ")";
    }
    connectionInfoLabel->setText(connectionInfo, dontSendNotification);

    // Update connection status
    updateConnectionStatus();
}

void RedisDataThreadEditor::applySettings()
{
    // Settings are now managed through the parameter system
    // Parameter changes automatically trigger parameterValueChanged in the DataThread
    if (!validateSettings())
        return;
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
    if (dataThread->getRedisChannelName().isEmpty())
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
    int numChannels = dataThread->getNumDataChannels();
    if (numChannels <= 0 || numChannels > 1024)
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings", "Number of channels must be between 1 and 1024.");
        return false;
    }

    // Validate buffer size
    int bufferSize = dataThread->getBufferSize();
    if (bufferSize < 100 || bufferSize > 100000)
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon, "Invalid Settings",
                                   "Buffer size must be between 100 and 100000 samples.\n"
                                   "Recommended values:\n"
                                   "• Real-time applications: 500-2000\n"
                                   "• Balanced performance: 3000-10000\n"
                                   "• High throughput: 10000-50000");
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
    configDialog.addTextEditor("channel", dataThread->getRedisChannelName(), "Channel:");
    configDialog.addTextEditor("sampleRate", String(dataThread->getSampleRate()), "Sample Rate:");
    configDialog.addTextEditor("numChannels", String(dataThread->getNumDataChannels()), "Channels:");
    configDialog.addTextEditor("bufferSize", String(dataThread->getBufferSize()), "Buffer Size:");

    configDialog.addComboBox("dataFormat", StringArray("JSON", "Binary", "BRANDBCI"), "Format:");
    int formatIndex = dataThread->getDataFormat() == "json" ? 0 :
                     dataThread->getDataFormat() == "binary" ? 1 : 2;
    configDialog.getComboBoxComponent("dataFormat")->setSelectedItemIndex(formatIndex);



    // Add stream mode toggle (using a combo box since AlertWindow doesn't support toggle buttons)
    configDialog.addComboBox("streamMode", StringArray("List Mode", "Stream Mode"), "Mode:");
    configDialog.getComboBoxComponent("streamMode")->setSelectedItemIndex(dataThread->getStreamMode() ? 1 : 0);

    configDialog.addComboBox("dataValidation", StringArray("Disabled", "Enabled"), "Data Validation:");
    configDialog.getComboBoxComponent("dataValidation")->setSelectedItemIndex(dataThread->isDataValidationEnabled() ? 1 : 0);

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
        dataThread->setBufferSize(configDialog.getTextEditorContents("bufferSize").getIntValue());

        int selectedFormatIndex = configDialog.getComboBoxComponent("dataFormat")->getSelectedItemIndex();
        String selectedFormat = selectedFormatIndex == 0 ? "json" :
                               selectedFormatIndex == 1 ? "binary" : "brandbci";
        dataThread->setDataFormat(selectedFormat);


        bool streamMode = configDialog.getComboBoxComponent("streamMode")->getSelectedItemIndex() == 1;
        dataThread->setStreamMode(streamMode);

        bool dataValidation = configDialog.getComboBoxComponent("dataValidation")->getSelectedItemIndex() == 1;
        dataThread->setDataValidationEnabled(dataValidation);

        LOGD("Configuration updated:");
        LOGD("  - Data Validation: ", dataValidation ? "Enabled" : "Disabled");

        // Trigger processor update to recreate channels and buffers
        CoreServices::updateSignalChain(getProcessor());

        updateSettings();
    }
}

void RedisDataThreadEditor::startAcquisition()
{
    // Disable editing during acquisition
    configureButton->setEnabled(false);
    connectButton->setEnabled(false);
}

void RedisDataThreadEditor::stopAcquisition()
{
    // Re-enable editing after acquisition
    configureButton->setEnabled(true);
    connectButton->setEnabled(true);
}





void RedisDataThreadEditor::componentBeingDeleted(Component& component)
{
    if (&component == currentConfigPopup)
    {
        currentConfigPopup = nullptr;
    }
}


