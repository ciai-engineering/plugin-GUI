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

#ifndef __REDISCONFIGURATIONPOPUP_H__
#define __REDISCONFIGURATIONPOPUP_H__

#include <EditorHeaders.h>

class RedisDataThread;
class RedisConfigurationPanel;

/**
 * Popup window for Redis configuration that follows the standard Open Ephys popup pattern.
 * This class wraps the existing RedisConfigurationPanel in a PopupComponent.
 */
class RedisConfigurationPopup : public PopupComponent
{
public:
    /** Constructor */
    RedisConfigurationPopup(Component* parent, RedisDataThread* thread);

    /** Destructor */
    ~RedisConfigurationPopup();

    /** Called when the popup needs to be updated */
    void updatePopup() override;

    /** Handle resizing */
    void resized() override;

    /** Handle painting */
    void paint(Graphics& g) override;

private:
    /** Pointer to the Redis data thread */
    RedisDataThread* dataThread;

    /** The actual configuration panel */
    std::unique_ptr<RedisConfigurationPanel> configPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RedisConfigurationPopup)
};

#endif // __REDISCONFIGURATIONPOPUP_H__
