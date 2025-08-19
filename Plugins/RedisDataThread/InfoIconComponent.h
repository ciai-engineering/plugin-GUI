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

#ifndef __INFOICONCOMPONENT_H_INCLUDED__
#define __INFOICONCOMPONENT_H_INCLUDED__

#include <EditorHeaders.h>

/**
 * Small info icon that shows detailed tooltip on hover
 * 
 * Provides a clean way to show detailed parameter information
 * without cluttering the interface with long text labels.
 */
class InfoIconComponent : public Component
{
public:
    /** Constructor */
    InfoIconComponent(const String& tooltipText);
    
    /** Destructor */
    ~InfoIconComponent();
    
    /** Component interface */
    void paint(Graphics& g) override;
    void mouseEnter(const MouseEvent& event) override;
    void mouseExit(const MouseEvent& event) override;
    void mouseDown(const MouseEvent& event) override;
    
    /** Update tooltip text */
    void setTooltipText(const String& text);
    
    /** Set icon style */
    void setIconStyle(bool isWarning = false);

private:
    String tooltipText;
    bool isHovered;
    bool isWarning;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InfoIconComponent);
};

/**
 * Enhanced tooltip component with rich formatting
 * 
 * Shows detailed parameter information in a nicely formatted popup.
 */
class EnhancedTooltipWindow : public Component
{
public:
    /** Constructor */
    EnhancedTooltipWindow();
    
    /** Destructor */
    ~EnhancedTooltipWindow();
    
    /** Show tooltip at specific position */
    void showTooltip(const String& title, const String& description, 
                     const StringArray& examples, const Point<int>& position);
    
    /** Hide tooltip */
    void hideTooltip();
    
    /** Component interface */
    void paint(Graphics& g) override;
    
private:
    String title;
    String description;
    StringArray examples;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnhancedTooltipWindow);
};

#endif // __INFOICONCOMPONENT_H_INCLUDED__
