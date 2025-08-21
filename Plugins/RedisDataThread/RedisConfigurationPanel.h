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

#ifndef __REDISCONFIGURATIONPANEL_H_INCLUDED__
#define __REDISCONFIGURATIONPANEL_H_INCLUDED__

#include <EditorHeaders.h>
#include "CustomTooltipComponent.h"

class RedisDataThread;
class RedisSavePresetPopup;
class RedisChannelScanPopup;



/**
 * Configuration panel with grouped settings for Redis connection
 * 
 * Provides a well-organized interface with:
 * - Connection settings group
 * - Data stream settings group  
 * - Data format settings group
 * - Real-time validation and tooltips
 * - Preset configuration templates
 */
class RedisConfigurationPanel : public Component,
                               public TextEditor::Listener,
                               public ComboBox::Listener,
                               public Button::Listener,
                               public Timer
{
public:
    /** Constructor */
    RedisConfigurationPanel(RedisDataThread* thread);
    
    /** Destructor */
    ~RedisConfigurationPanel();
    
    /** Component interface */
    void paint(Graphics& g) override;
    void resized() override;
    
    /** TextEditor::Listener interface */
    void textEditorTextChanged(TextEditor& editor) override;
    void textEditorReturnKeyPressed(TextEditor& editor) override;
    void textEditorFocusLost(TextEditor& editor) override;
    
    /** ComboBox::Listener interface */
    void comboBoxChanged(ComboBox* comboBox) override;
    
    /** Button::Listener interface */
    void buttonClicked(Button* button) override;

    /** Timer interface for status updates */
    void timerCallback() override;

    /** Update UI with current settings */
    void updateFromThread();
    
    /** Apply current UI settings to thread */
    void applyToThread();
    
    /** Validate all current settings */
    bool validateAllSettings();
    
    /** Load preset configuration */
    void loadPreset(const String& presetName);

    /** Save current configuration as preset */
    void savePreset(const String& presetName);

    /** Show help dialog */
    void showHelpDialog();

    /** Test connection with current settings */
    void testConnection();

    /** Show latest data from Redis */
    void showLatestData();

    /** Refresh available channels from Redis */
    void refreshAvailableChannels();

private:
    RedisDataThread* dataThread;
    
    // Group components
    std::unique_ptr<GroupComponent> connectionGroup;
    std::unique_ptr<GroupComponent> streamGroup;
    std::unique_ptr<GroupComponent> formatGroup;
    std::unique_ptr<GroupComponent> advancedGroup;
    
    // Connection settings
    std::unique_ptr<Label> hostLabel;
    std::unique_ptr<TextEditor> hostEditor;
    std::unique_ptr<Label> hostTooltip;
    std::unique_ptr<UtilityButton> hostInfoButton;
    
    std::unique_ptr<Label> portLabel;
    std::unique_ptr<TextEditor> portEditor;
    std::unique_ptr<Label> portTooltip;
    
    std::unique_ptr<Label> passwordLabel;
    std::unique_ptr<TextEditor> passwordEditor;
    std::unique_ptr<Label> passwordTooltip;
    
    // Stream settings
    std::unique_ptr<Label> channelLabel;
    std::unique_ptr<ComboBox> channelComboBox;
    std::unique_ptr<UtilityButton> refreshChannelsButton;
    std::unique_ptr<Label> channelTooltip;
    
    std::unique_ptr<Label> streamModeLabel;
    std::unique_ptr<ToggleButton> streamModeButton;
    std::unique_ptr<Label> streamModeTooltip;

    std::unique_ptr<Label> alwaysLatestLabel;
    std::unique_ptr<ToggleButton> alwaysLatestButton;
    std::unique_ptr<Label> alwaysLatestTooltip;
    
    // Format settings
    std::unique_ptr<Label> sampleRateLabel;
    std::unique_ptr<TextEditor> sampleRateEditor;
    std::unique_ptr<Label> sampleRateTooltip;
    
    std::unique_ptr<Label> numChannelsLabel;
    std::unique_ptr<TextEditor> numChannelsEditor;
    std::unique_ptr<Label> numChannelsTooltip;
    
    std::unique_ptr<Label> dataFormatLabel;
    std::unique_ptr<ComboBox> dataFormatCombo;
    std::unique_ptr<Label> dataFormatTooltip;
    
    // Advanced settings (collapsible)
    std::unique_ptr<Label> bufferSizeLabel;
    std::unique_ptr<TextEditor> bufferSizeEditor;
    std::unique_ptr<Label> bufferSizeTooltip;
    
    std::unique_ptr<Label> openEphysFormatLabel;
    std::unique_ptr<ToggleButton> openEphysFormatButton;
    std::unique_ptr<Label> openEphysFormatTooltip;

    std::unique_ptr<Label> dataValidationLabel;
    std::unique_ptr<ToggleButton> dataValidationButton;
    std::unique_ptr<Label> dataValidationTooltip;
    
    // Preset and control buttons
    std::unique_ptr<ComboBox> presetCombo;
    std::unique_ptr<UtilityButton> testConnectionButton;
    std::unique_ptr<UtilityButton> resetButton;
    std::unique_ptr<UtilityButton> savePresetButton;
    std::unique_ptr<UtilityButton> helpButton;
    std::unique_ptr<UtilityButton> dataButton;

    // Status indicators
    std::unique_ptr<Label> validationStatus;
    std::unique_ptr<Label> connectionStatus;
    std::unique_ptr<Label> performanceHints;
    
    // Helper methods
    void createConnectionGroup();
    void createStreamGroup();
    void createFormatGroup();
    void createAdvancedGroup();
    void createControlButtons();
    void setupTooltips();
    void setupCustomTooltips();
    void validateField(TextEditor* editor, const String& fieldName);
    void updateValidationStatus();
    void updatePerformanceHints();
    void showFieldError(Component* field, const String& message);
    void clearFieldError(Component* field);
    String getFieldTooltip(const String& fieldName);
    void setupPresets();


    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisConfigurationPanel);
};

#endif // __REDISCONFIGURATIONPANEL_H_INCLUDED__
