#pragma once

#include "../DeepLearning/WebModel.h"
#include <ARA_Library/PlugIn/ARAPlug.h>

using std::unique_ptr;


/**
 * @class PlaybackRegion
 * @brief This class provides methods for manipulating playback regions.
 *
 * We override the ARAPlaybackRegion class to add flags for when a playback
 * region is selected for processing
 * Each playback region has its own audio modification,
 */

class PlaybackRegion : public juce::ARAPlaybackRegion {
public:
    // ARAAudioModification* modification,
    // ARA::ARAPlaybackRegionHostRef hostRef
    PlaybackRegion(juce::ARAAudioModification *modification,
                    ARA::ARAPlaybackRegionHostRef hostRef);

    bool isSelected() const;
    void setSelected(bool selectedFlag);

private:
    bool selected;
};
