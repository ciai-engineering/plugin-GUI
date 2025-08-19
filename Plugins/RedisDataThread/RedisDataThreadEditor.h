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

#ifndef __REDISDATATHREADEDITOR_H_INCLUDED__
#define __REDISDATATHREADEDITOR_H_INCLUDED__

#include <EditorHeaders.h>

class RedisDataThread;
class RedisConfigurationPanel;
class RedisConfigurationPopup;

/**
 * Custom editor for the RedisDataThread
 * 
 * Provides UI controls for configuring Redis connection parameters,
 * data format settings, and monitoring connection status.
 */
class TESTABLE RedisDataThreadEditor : public GenericEditor,
                                       public TextEditor::Listener,
                                       public ComboBox::Listener,
                                       public Button::Listener,
                                       public Timer,
                                       public ComponentListener
{
public:
    /** Constructor */
    RedisDataThreadEditor(GenericProcessor* parentNode, RedisDataThread* thread);

    /** Destructor */
    ~RedisDataThreadEditor();

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

    /** ComponentListener interface */
    void componentBeingDeleted(Component& component) override;

    /** Updates the UI with current settings */
    void updateSettings();

    /** Called when acquisition starts/stops */
    void startAcquisition() override;
    void stopAcquisition() override;

private:
    RedisDataThread* dataThread;

    // Connection settings
    std::unique_ptr<Label> hostLabel;
    std::unique_ptr<TextEditor> hostEditor;
    std::unique_ptr<Label> portLabel;
    std::unique_ptr<TextEditor> portEditor;
    std::unique_ptr<Label> passwordLabel;
    std::unique_ptr<TextEditor> passwordEditor;
    std::unique_ptr<Label> channelLabel;
    std::unique_ptr<TextEditor> channelEditor;

    // Data settings
    std::unique_ptr<Label> sampleRateLabel;
    std::unique_ptr<TextEditor> sampleRateEditor;
    std::unique_ptr<Label> numChannelsLabel;
    std::unique_ptr<TextEditor> numChannelsEditor;
    std::unique_ptr<Label> dataFormatLabel;
    std::unique_ptr<ComboBox> dataFormatCombo;

    // Stream settings
    std::unique_ptr<Label> streamModeLabel;
    std::unique_ptr<ToggleButton> streamModeButton;

    // Control buttons
    std::unique_ptr<UtilityButton> connectButton;
    std::unique_ptr<UtilityButton> testButton;
    std::unique_ptr<UtilityButton> configureButton;
    std::unique_ptr<UtilityButton> dataButton;

    // Status display
    std::unique_ptr<Label> connectionInfoLabel;
    std::unique_ptr<Label> statusLabel;
    std::unique_ptr<Label> statusValueLabel;

    // Configuration popup
    RedisConfigurationPopup* currentConfigPopup;

    // Helper methods
    void createConnectionControls();
    void createDataControls();
    void createStatusControls();
    void updateConnectionStatus();
    void applySettings();
    bool validateSettings();
    void showConfigurationDialog();
    void showLatestData();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisDataThreadEditor);
};

#endif // __REDISDATATHREADEDITOR_H_INCLUDED__
