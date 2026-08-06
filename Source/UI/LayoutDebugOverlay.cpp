#include "LayoutDebugOverlay.h"
#include <NdlrDefaultLayoutData.h>

namespace ndlr::ui
{
juce::ValueTree LayoutDebugOverlay::getCompiledDefaultLayout()
{
    static const auto layout = []
    {
        if (const auto xml = juce::AudioProcessor::getXmlFromBinary (
                ndlr_resources::ndlrstate, ndlr_resources::ndlrstateSize))
            return juce::ValueTree::fromXml (*xml).getChildWithName ("UILayout");

        return juce::ValueTree {};
    }();

    return layout;
}
}
