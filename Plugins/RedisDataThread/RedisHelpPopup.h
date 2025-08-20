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

#ifndef REDISHELPPOPUP_H_INCLUDED
#define REDISHELPPOPUP_H_INCLUDED

#include <ProcessorHeaders.h>

/**
 * Popup component for displaying Redis configuration help
 */
class RedisHelpPopup : public Component
{
public:
    /** Constructor */
    RedisHelpPopup();
    
    /** Destructor */
    ~RedisHelpPopup();

    /** Component paint method */
    void paint(Graphics& g) override;

    /** Component resized method */
    void resized() override;

private:
    /** Setup the UI components */
    void setupUI();

    /** Get the help text content */
    String getHelpText();

    // UI Components
    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> iconLabel;
    std::unique_ptr<TextEditor> helpTextEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisHelpPopup)
};

#endif // REDISHELPPOPUP_H_INCLUDED
