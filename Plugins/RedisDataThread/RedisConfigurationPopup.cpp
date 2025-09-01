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

#include "RedisConfigurationPopup.h"
#include "RedisConfigurationPanel.h"
#include "RedisDataThread.h"

RedisConfigurationPopup::RedisConfigurationPopup(Component* parent, RedisDataThread* thread)
    : PopupComponent(parent), dataThread(thread)
{
    // Create the configuration panel
    configPanel = std::make_unique<RedisConfigurationPanel>(dataThread);
    addAndMakeVisible(configPanel.get());

    // Set the size to match the configuration panel's calculated size
    int panelWidth = 420;
    int panelHeight = 810; // Reduced by 60px due to streamGroup height reduction (870-60=810)
    setSize(panelWidth, panelHeight);
    configPanel->setBounds(0, 0, panelWidth, panelHeight);
}

RedisConfigurationPopup::~RedisConfigurationPopup()
{
}

void RedisConfigurationPopup::updatePopup()
{
    // Update the configuration panel if needed
    if (configPanel != nullptr)
    {
        configPanel->updateFromThread();
    }
}

void RedisConfigurationPopup::resized()
{
    // Make the configuration panel fill the entire popup
    if (configPanel != nullptr)
    {
        configPanel->setBounds(getLocalBounds());
    }
}

void RedisConfigurationPopup::paint(Graphics& g)
{
    // Use the standard popup background
    g.fillAll(findColour(ThemeColours::widgetBackground));
}
