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

#include "InfoIconComponent.h"

InfoIconComponent::InfoIconComponent(const String& tooltipText)
    : tooltipText(tooltipText), isHovered(false), isWarning(false)
{
    setSize(16, 16);
    setTooltip(tooltipText);
}

InfoIconComponent::~InfoIconComponent()
{
}

void InfoIconComponent::paint(Graphics& g)
{
    // Draw circular background
    Colour bgColor = isWarning ? Colours::orange : Colours::lightblue;
    if (isHovered)
        bgColor = bgColor.brighter(0.2f);
    
    g.setColour(bgColor.withAlpha(0.8f));
    g.fillEllipse(1, 1, getWidth() - 2, getHeight() - 2);
    
    // Draw border
    g.setColour(bgColor.darker(0.3f));
    g.drawEllipse(1, 1, getWidth() - 2, getHeight() - 2, 1.0f);
    
    // Draw "i" or "!" symbol
    g.setColour(Colours::white);
    g.setFont(FontOptions("Arial", "Bold", 10));
    
    String symbol = isWarning ? "!" : "i";
    g.drawText(symbol, getLocalBounds(), Justification::centred);
}

void InfoIconComponent::mouseEnter(const MouseEvent& event)
{
    isHovered = true;
    repaint();
}

void InfoIconComponent::mouseExit(const MouseEvent& event)
{
    isHovered = false;
    repaint();
}

void InfoIconComponent::mouseDown(const MouseEvent& event)
{
    // Show detailed tooltip on click
    AlertWindow::showMessageBox(AlertWindow::InfoIcon, 
                               "Parameter Information", 
                               tooltipText);
}

void InfoIconComponent::setTooltipText(const String& text)
{
    tooltipText = text;
    setTooltip(text);
}

void InfoIconComponent::setIconStyle(bool warning)
{
    isWarning = warning;
    repaint();
}

// Enhanced Tooltip Window Implementation

EnhancedTooltipWindow::EnhancedTooltipWindow()
{
    setAlwaysOnTop(true);
    setVisible(false);
}

EnhancedTooltipWindow::~EnhancedTooltipWindow()
{
}

void EnhancedTooltipWindow::showTooltip(const String& titleText, const String& descriptionText, 
                                       const StringArray& exampleList, const Point<int>& position)
{
    title = titleText;
    description = descriptionText;
    examples = exampleList;
    
    // Calculate size based on content
    int width = 300;
    int height = 80 + (examples.size() * 20);
    
    setBounds(position.x, position.y, width, height);
    setVisible(true);
    toFront(true);
}

void EnhancedTooltipWindow::hideTooltip()
{
    setVisible(false);
}

void EnhancedTooltipWindow::paint(Graphics& g)
{
    // Background with shadow
    g.setColour(Colours::black.withAlpha(0.1f));
    g.fillRoundedRectangle(2, 2, getWidth() - 2, getHeight() - 2, 5);
    
    g.setColour(Colours::white);
    g.fillRoundedRectangle(0, 0, getWidth() - 4, getHeight() - 4, 5);
    
    g.setColour(Colours::grey);
    g.drawRoundedRectangle(0, 0, getWidth() - 4, getHeight() - 4, 5, 1);
    
    // Content
    int yPos = 10;
    
    // Title
    g.setColour(Colours::black);
    g.setFont(FontOptions("Inter", "Bold", 14));
    g.drawText(title, 10, yPos, getWidth() - 20, 20, Justification::left);
    yPos += 25;
    
    // Description
    g.setFont(FontOptions("Inter", "Regular", 12));
    g.drawText(description, 10, yPos, getWidth() - 20, 30, Justification::left);
    yPos += 35;
    
    // Examples
    if (examples.size() > 0)
    {
        g.setFont(FontOptions("Inter", "Bold", 11));
        g.drawText("Examples:", 10, yPos, getWidth() - 20, 15, Justification::left);
        yPos += 18;
        
        g.setFont(FontOptions("Inter", "Regular", 10));
        g.setColour(Colours::darkgrey);
        
        for (const String& example : examples)
        {
            g.drawText("• " + example, 20, yPos, getWidth() - 30, 15, Justification::left);
            yPos += 18;
        }
    }
}
