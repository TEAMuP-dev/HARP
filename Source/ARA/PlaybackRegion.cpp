#include "PlaybackRegion.h"

PlaybackRegion::PlaybackRegion(juce::ARAAudioModification *modification,
                                ARA::ARAPlaybackRegionHostRef hostRef)
    : juce::ARAPlaybackRegion(modification, hostRef) {
    selected = true;
}

bool PlaybackRegion::isSelected() const { return selected; }
void PlaybackRegion::setSelected(bool selectedFlag) {
    this->selected = selectedFlag;
    DBG("PlaybackRegion::setSelect");
}
