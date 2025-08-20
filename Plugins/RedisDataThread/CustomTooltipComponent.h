#ifndef __CUSTOMTOOLTIPCOMPONENT_H_INCLUDED__
#define __CUSTOMTOOLTIPCOMPONENT_H_INCLUDED__

#include "../../JuceLibraryCode/JuceHeader.h"

/**
 * A custom tooltip component that displays formatted help text in a CallOutBox
 * with proper left alignment and rich formatting
 */
class CustomTooltipComponent : public Component
{
public:
    CustomTooltipComponent(const String& title, const String& description, 
                          const StringArray& examples, const String& tip);
    ~CustomTooltipComponent();

    void paint(Graphics& g) override;
    void resized() override;

    // Static method to show tooltip
    static void showTooltip(Component* anchor, const String& title, 
                           const String& description, const StringArray& examples, 
                           const String& tip);

private:
    String titleText;
    String descriptionText;
    StringArray exampleTexts;
    String tipText;

    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> descriptionLabel;
    std::unique_ptr<Label> examplesLabel;
    std::unique_ptr<Label> tipLabel;

    void setupLabels();
    int calculateRequiredHeight();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomTooltipComponent);
};

/**
 * Helper class to manage tooltip display with proper formatting
 */
class TooltipHelper
{
public:
    // Predefined tooltip data structures
    struct TooltipData
    {
        String title;
        String description;
        StringArray examples;
        String tip;
    };

    // Show tooltip for Redis configuration fields
    static void showRedisHostTooltip(Component* anchor);
    static void showRedisPortTooltip(Component* anchor);
    static void showRedisPasswordTooltip(Component* anchor);
    static void showRedisChannelTooltip(Component* anchor);
    static void showStreamModeTooltip(Component* anchor);
    static void showAlwaysLatestTooltip(Component* anchor);
    static void showSampleRateTooltip(Component* anchor);
    static void showNumChannelsTooltip(Component* anchor);
    static void showDataFormatTooltip(Component* anchor);
    static void showBufferSizeTooltip(Component* anchor);
    static void showOpenEphysFormatTooltip(Component* anchor);
    static void showDataValidationTooltip(Component* anchor);

private:
    static TooltipData getHostTooltipData();
    static TooltipData getPortTooltipData();
    static TooltipData getPasswordTooltipData();
    static TooltipData getChannelTooltipData();
    static TooltipData getStreamModeTooltipData();
    static TooltipData getAlwaysLatestTooltipData();
    static TooltipData getSampleRateTooltipData();
    static TooltipData getNumChannelsTooltipData();
    static TooltipData getDataFormatTooltipData();
    static TooltipData getBufferSizeTooltipData();
    static TooltipData getOpenEphysFormatTooltipData();
    static TooltipData getDataValidationTooltipData();
};

#endif // __CUSTOMTOOLTIPCOMPONENT_H_INCLUDED__
