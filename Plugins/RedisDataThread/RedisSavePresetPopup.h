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

#ifndef REDISSAVEPRESETPOPUP_H_INCLUDED
#define REDISSAVEPRESETPOPUP_H_INCLUDED

#include <ProcessorHeaders.h>

// Forward declaration
class RedisConfigurationPanel;

/**
 * Popup component for saving Redis configuration presets
 * 
 * Provides a text input field for entering preset names with proper
 * focus handling and keyboard input support.
 */
class RedisSavePresetPopup : public Component, public TextEditor::Listener, public Button::Listener
{
public:
    /** Constructor */
    RedisSavePresetPopup(RedisConfigurationPanel* parent);
    
    /** Destructor */
    ~RedisSavePresetPopup();

    /** Component interface */
    void paint(Graphics& g) override;
    void resized() override;

    /** TextEditor::Listener interface */
    void textEditorReturnKeyPressed(TextEditor& editor) override;
    void textEditorEscapeKeyPressed(TextEditor& editor) override;

    /** Button::Listener interface */
    void buttonClicked(Button* button) override;

    /** Called when component becomes visible to set focus */
    void visibilityChanged() override;

private:
    // UI components
    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> instructionLabel;
    std::unique_ptr<TextEditor> presetNameEditor;
    std::unique_ptr<TextButton> saveButton;
    std::unique_ptr<TextButton> cancelButton;

    // Parent reference
    RedisConfigurationPanel* configPanel;

    // Helper methods
    void setupUI();
    void savePreset();
    void cancelDialog();
    bool isValidPresetName(const String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisSavePresetPopup)
};

#endif // REDISSAVEPRESETPOPUP_H_INCLUDED
