#include "CustomTooltipComponent.h"

CustomTooltipComponent::CustomTooltipComponent(const String& title, const String& description, 
                                             const StringArray& examples, const String& tip)
    : titleText(title), descriptionText(description), exampleTexts(examples), tipText(tip)
{
    setupLabels();
    setSize(320, calculateRequiredHeight());
}

CustomTooltipComponent::~CustomTooltipComponent()
{
}

void CustomTooltipComponent::setupLabels()
{
    // Title label
    titleLabel = std::make_unique<Label>("title", titleText);
    titleLabel->setFont(Font(FontOptions("Inter", "Bold", 14.0f)));
    titleLabel->setColour(Label::textColourId, Colours::white);
    titleLabel->setJustificationType(Justification::topLeft);
    addAndMakeVisible(titleLabel.get());

    // Description label
    descriptionLabel = std::make_unique<Label>("description", descriptionText);
    descriptionLabel->setFont(Font(FontOptions("Inter", "Regular", 12.0f)));
    descriptionLabel->setColour(Label::textColourId, Colours::lightgrey);
    descriptionLabel->setJustificationType(Justification::topLeft);
    addAndMakeVisible(descriptionLabel.get());

    // Examples label
    if (exampleTexts.size() > 0)
    {
        String exampleText = "Examples:\n";
        for (const auto& example : exampleTexts)
        {
            exampleText += "• " + example + "\n";
        }
        exampleText = exampleText.trimEnd();

        examplesLabel = std::make_unique<Label>("examples", exampleText);
        examplesLabel->setFont(Font(FontOptions("Inter", "Regular", 11.0f)));
        examplesLabel->setColour(Label::textColourId, Colour(0xff9bb5ff));
        examplesLabel->setJustificationType(Justification::topLeft);
        addAndMakeVisible(examplesLabel.get());
    }

    // Tip label
    if (tipText.isNotEmpty())
    {
        tipLabel = std::make_unique<Label>("tip", "💡 " + tipText);
        tipLabel->setFont(Font(FontOptions("Inter", "Italic", 11.0f)));
        tipLabel->setColour(Label::textColourId, Colour(0xffffcc99));
        tipLabel->setJustificationType(Justification::topLeft);
        addAndMakeVisible(tipLabel.get());
    }
}

int CustomTooltipComponent::calculateRequiredHeight()
{
    int height = 10; // Top padding
    
    // Title height
    height += 20;
    
    // Description height (estimate based on text length)
    int descLines = (descriptionText.length() / 50) + 1;
    height += descLines * 15 + 5;
    
    // Examples height
    if (exampleTexts.size() > 0)
    {
        height += 15 + (exampleTexts.size() * 13) + 5;
    }
    
    // Tip height
    if (tipText.isNotEmpty())
    {
        int tipLines = (tipText.length() / 50) + 1;
        height += tipLines * 13 + 5;
    }
    
    height += 10; // Bottom padding
    
    return jmax(80, height);
}

void CustomTooltipComponent::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xff2a2a2a));
    
    // Border
    g.setColour(Colour(0xff555555));
    g.drawRect(getLocalBounds(), 1);
}

void CustomTooltipComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    
    if (titleLabel)
    {
        titleLabel->setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5);
    }
    
    if (descriptionLabel)
    {
        int descLines = (descriptionText.length() / 50) + 1;
        descriptionLabel->setBounds(bounds.removeFromTop(descLines * 15));
        bounds.removeFromTop(5);
    }
    
    if (examplesLabel)
    {
        int exampleHeight = 15 + (exampleTexts.size() * 13);
        examplesLabel->setBounds(bounds.removeFromTop(exampleHeight));
        bounds.removeFromTop(5);
    }
    
    if (tipLabel)
    {
        int tipLines = (tipText.length() / 50) + 1;
        tipLabel->setBounds(bounds.removeFromTop(tipLines * 13));
    }
}

void CustomTooltipComponent::showTooltip(Component* anchor, const String& title, 
                                        const String& description, const StringArray& examples, 
                                        const String& tip)
{
    auto tooltipComponent = std::make_unique<CustomTooltipComponent>(title, description, examples, tip);
    
    auto& callout = CallOutBox::launchAsynchronously(std::move(tooltipComponent), 
                                                    anchor->getScreenBounds(), 
                                                    nullptr);
    callout.setDismissalMouseClicksAreAlwaysConsumed(true);
}

// TooltipHelper implementation
TooltipHelper::TooltipData TooltipHelper::getHostTooltipData()
{
    TooltipData data;
    data.title = "Redis Server Address";
    data.description = "Hostname or IP address of the Redis server.";
    data.examples = {"localhost - Local Redis server", 
                    "192.168.1.100 - Remote server", 
                    "redis.example.com - Domain name"};
    data.tip = "Use 'localhost' for local development";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getPortTooltipData()
{
    TooltipData data;
    data.title = "Redis Server Port";
    data.description = "Port number for Redis server connection (1-65535).";
    data.examples = {"6379 - Default Redis port", 
                    "6380 - Alternative port", 
                    "16379 - Redis Cluster port"};
    data.tip = "Check your Redis configuration file";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getPasswordTooltipData()
{
    TooltipData data;
    data.title = "Redis Authentication";
    data.description = "Password for Redis AUTH command.";
    data.examples = {"Redis Cloud instances", 
                    "Production servers with security", 
                    "Custom Redis configurations"};
    data.tip = "Leave empty for local development";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getChannelTooltipData()
{
    TooltipData data;
    data.title = "Redis Channel/Stream Name";
    data.description = "Name of the Redis channel or stream for data.";
    data.examples = {"neural_data - BRANDBCI neural data", 
                    "openephys_data - General purpose", 
                    "lfp_stream - Local field potentials", 
                    "spike_data - Spike events"};
    data.tip = "Use descriptive names for clarity";
    return data;
}

void TooltipHelper::showRedisHostTooltip(Component* anchor)
{
    auto data = getHostTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showRedisPortTooltip(Component* anchor)
{
    auto data = getPortTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showRedisPasswordTooltip(Component* anchor)
{
    auto data = getPasswordTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showRedisChannelTooltip(Component* anchor)
{
    auto data = getChannelTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

TooltipHelper::TooltipData TooltipHelper::getStreamModeTooltipData()
{
    TooltipData data;
    data.title = "Redis Stream Mode";
    data.description = "Enable Redis Streams (XREAD) vs Lists (BLPOP).";
    data.examples = {"Better for real-time data",
                    "Multiple consumer support",
                    "Built-in message persistence",
                    "BRANDBCI compatibility"};
    data.tip = "Enable for modern applications";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getAlwaysLatestTooltipData()
{
    TooltipData data;
    data.title = "Always Read Latest Data";
    data.description = "Control stream reading behavior.";
    data.examples = {"Enabled: reads newest data (uses '$')",
                    "Enabled: may skip data during gaps",
                    "Disabled: sequential reading (no data loss)",
                    "Disabled: processes all data in order"};
    data.tip = "Enable for live monitoring, disable for data recording";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getSampleRateTooltipData()
{
    TooltipData data;
    data.title = "Sampling Rate (Hz)";
    data.description = "Expected data sampling frequency.";
    data.examples = {"30000 Hz - High-freq neural (spikes)",
                    "1000 Hz - LFP/slow signals",
                    "2000 Hz - Behavioral data",
                    "100 Hz - Event markers"};
    data.tip = "Match your data source rate";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getNumChannelsTooltipData()
{
    TooltipData data;
    data.title = "Number of Channels";
    data.description = "Data channels per sample (1-1024).";
    data.examples = {"32 - Small arrays/tetrodes",
                    "96 - Utah microelectrode arrays",
                    "128 - High-density silicon probes",
                    "384 - Neuropixels probes"};
    data.tip = "Match your electrode count";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getDataFormatTooltipData()
{
    TooltipData data;
    data.title = "Data Encoding Format";
    data.description = "Choose the data format for Redis communication.";
    data.examples = {"BRANDBCI - Native format, best integration",
                    "JSON - Human-readable, flexible parsing",
                    "Binary - Highest performance, compact size"};
    data.tip = "Use BRANDBCI for neural data systems";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getBufferSizeTooltipData()
{
    TooltipData data;
    data.title = "Buffer Size (Samples)";
    data.description = "Internal buffer size for data processing.";
    data.examples = {"500-2000 - Real-time, low latency",
                    "3000-10000 - Balanced performance",
                    "10000+ - High throughput mode"};
    data.tip = "Smaller = lower latency, Larger = more stable";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getOpenEphysFormatTooltipData()
{
    TooltipData data;
    data.title = "Open Ephys Format";
    data.description = "Enable native Open Ephys format support.";
    data.examples = {"Better signal chain integration",
                    "Optimized data processing",
                    "Native metadata support"};
    data.tip = "Enable for best Open Ephys compatibility";
    return data;
}

TooltipHelper::TooltipData TooltipHelper::getDataValidationTooltipData()
{
    TooltipData data;
    data.title = "Data Validation";
    data.description = "Enable real-time data validation checks.";
    data.examples = {"Correct channel count",
                    "Valid data ranges",
                    "Format consistency",
                    "Error detection"};
    data.tip = "Disable for maximum performance";
    return data;
}

void TooltipHelper::showStreamModeTooltip(Component* anchor)
{
    auto data = getStreamModeTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showAlwaysLatestTooltip(Component* anchor)
{
    auto data = getAlwaysLatestTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showSampleRateTooltip(Component* anchor)
{
    auto data = getSampleRateTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showNumChannelsTooltip(Component* anchor)
{
    auto data = getNumChannelsTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showDataFormatTooltip(Component* anchor)
{
    auto data = getDataFormatTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showBufferSizeTooltip(Component* anchor)
{
    auto data = getBufferSizeTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showOpenEphysFormatTooltip(Component* anchor)
{
    auto data = getOpenEphysFormatTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}

void TooltipHelper::showDataValidationTooltip(Component* anchor)
{
    auto data = getDataValidationTooltipData();
    CustomTooltipComponent::showTooltip(anchor, data.title, data.description, data.examples, data.tip);
}
