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

#include "RedisSavePresetPopup.h"
#include "RedisConfigurationPanel.h"

RedisSavePresetPopup::RedisSavePresetPopup(RedisConfigurationPanel* parent)
    : configPanel(parent)
{
    setSize(320, 140);  // 更紧凑的尺寸：宽度减少80px，高度减少40px
    setupUI();
}

RedisSavePresetPopup::~RedisSavePresetPopup()
{
}

void RedisSavePresetPopup::setupUI()
{
    // Title label - 更紧凑的布局
    titleLabel = std::make_unique<Label>("Title Label", "Save Preset");
    titleLabel->setBounds(15, 10, getWidth() - 30, 22);
    titleLabel->setFont(FontOptions("Inter", "Bold", 15));
    titleLabel->setJustificationType(Justification::centred);
    addAndMakeVisible(titleLabel.get());

    // Instruction label - 减少垂直间距
    instructionLabel = std::make_unique<Label>("Instruction Label", "Enter a name for this preset:");
    instructionLabel->setBounds(15, 35, getWidth() - 30, 18);
    instructionLabel->setFont(FontOptions("Inter", "Regular", 11));
    instructionLabel->setJustificationType(Justification::left);
    addAndMakeVisible(instructionLabel.get());

    // Preset name text editor - 更紧凑的间距
    presetNameEditor = std::make_unique<TextEditor>("Preset Name Editor");
    presetNameEditor->setBounds(15, 56, getWidth() - 30, 24);
    presetNameEditor->setFont(FontOptions("Inter", "Regular", 13));
    presetNameEditor->setText("My Preset");
    presetNameEditor->setSelectAllWhenFocused(true);
    presetNameEditor->setEscapeAndReturnKeysConsumed(false);
    presetNameEditor->addListener(this);
    addAndMakeVisible(presetNameEditor.get());

    // 按钮布局 - 更紧凑的间距和尺寸
    int buttonWidth = 65;
    int buttonHeight = 22;
    int buttonSpacing = 8;
    int rightMargin = 15;

    // Cancel button (右侧)
    cancelButton = std::make_unique<TextButton>("Cancel");
    cancelButton->setBounds(getWidth() - rightMargin - buttonWidth,
                           getHeight() - 30,
                           buttonWidth, buttonHeight);
    cancelButton->addListener(this);
    addAndMakeVisible(cancelButton.get());

    // Save button (Cancel按钮左侧)
    saveButton = std::make_unique<TextButton>("Save");
    saveButton->setBounds(getWidth() - rightMargin - buttonWidth * 2 - buttonSpacing,
                         getHeight() - 30,
                         buttonWidth, buttonHeight);
    saveButton->addListener(this);
    addAndMakeVisible(saveButton.get());
}

void RedisSavePresetPopup::paint(Graphics& g)
{
    // Background
    g.fillAll(findColour(ThemeColours::componentBackground));
    
    // Border
    g.setColour(findColour(ThemeColours::defaultText).withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void RedisSavePresetPopup::resized()
{
    // 使用紧凑的布局参数
    int buttonWidth = 65;
    int buttonHeight = 22;
    int buttonSpacing = 8;
    int rightMargin = 15;

    if (titleLabel != nullptr)
        titleLabel->setBounds(15, 10, getWidth() - 30, 22);

    if (instructionLabel != nullptr)
        instructionLabel->setBounds(15, 35, getWidth() - 30, 18);

    if (presetNameEditor != nullptr)
        presetNameEditor->setBounds(15, 56, getWidth() - 30, 24);

    if (cancelButton != nullptr)
        cancelButton->setBounds(getWidth() - rightMargin - buttonWidth,
                               getHeight() - 30,
                               buttonWidth, buttonHeight);

    if (saveButton != nullptr)
        saveButton->setBounds(getWidth() - rightMargin - buttonWidth * 2 - buttonSpacing,
                             getHeight() - 30,
                             buttonWidth, buttonHeight);
}

void RedisSavePresetPopup::textEditorReturnKeyPressed(TextEditor& editor)
{
    if (&editor == presetNameEditor.get())
    {
        savePreset();
    }
}

void RedisSavePresetPopup::textEditorEscapeKeyPressed(TextEditor& editor)
{
    if (&editor == presetNameEditor.get())
    {
        cancelDialog();
    }
}

void RedisSavePresetPopup::buttonClicked(Button* button)
{
    if (button == saveButton.get())
    {
        savePreset();
    }
    else if (button == cancelButton.get())
    {
        cancelDialog();
    }
}

void RedisSavePresetPopup::visibilityChanged()
{
    if (isVisible() && presetNameEditor != nullptr)
    {
        // Give focus to the text editor when the popup becomes visible
        presetNameEditor->grabKeyboardFocus();
        presetNameEditor->selectAll();
    }
}

void RedisSavePresetPopup::savePreset()
{
    String presetName = presetNameEditor->getText().trim();
    
    if (!isValidPresetName(presetName))
    {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                   "Invalid Preset Name",
                                   "Please enter a valid preset name (non-empty, no special characters).");
        presetNameEditor->grabKeyboardFocus();
        return;
    }

    // Call the parent's savePreset method
    if (configPanel != nullptr)
    {
        configPanel->savePreset(presetName);
        
        // Show success message
        AlertWindow::showMessageBox(AlertWindow::InfoIcon,
                                   "Preset Saved",
                                   "Preset '" + presetName + "' has been saved successfully.");
    }

    // Close the popup by exiting modal state
    if (auto* parent = getParentComponent())
        parent->exitModalState(1);
}

void RedisSavePresetPopup::cancelDialog()
{
    // Close the popup by exiting modal state
    if (auto* parent = getParentComponent())
        parent->exitModalState(0);
}

bool RedisSavePresetPopup::isValidPresetName(const String& name)
{
    // First trim the name to check if it's empty after trimming
    String trimmed = name.trim();
    if (trimmed.isEmpty())
        return false;

    // Check for invalid characters that might cause file system issues
    String invalidChars = "<>:\"/\\|?*";
    for (int i = 0; i < invalidChars.length(); ++i)
    {
        if (trimmed.containsChar(invalidChars[i]))
            return false;
    }

    return true;
}
