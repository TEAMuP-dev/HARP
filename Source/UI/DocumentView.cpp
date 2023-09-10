/**
 * @file
 * @brief This file is part of the JUCE examples.
 *
 * Copyright (c) 2022 - Raw Material Software Limited
 * The code included in this file is provided under the terms of the ISC license
 * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
 * To use, copy, modify, and/or distribute this software for any purpose with or
 * without fee is hereby granted provided that the above copyright notice and
 * this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
 * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
 * PURPOSE, ARE DISCLAIMED.
 *
 * @brief This class handles the UI updates for the plugins document display.
 * The display mirrors the DAW document, and will show region sequences with the
 * ARA plugin is placed in the first position of the effects chain.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#include "DocumentView.h"

// DocumentView::DocumentView(ARAEditorView &editorView,
//                            PlayHeadState &playHeadState)
DocumentView::DocumentView(EditorView &editorView,
                           PlayHeadState &playHeadState)
    : araEditorView(editorView),
      araDocument(
          *editorView.getDocumentController()->getDocument<ARADocument>()),
      rulersView(playHeadState, timeToViewScaling, araDocument),
      overlay(playHeadState, timeToViewScaling),
      playheadPositionLabel(playHeadState) {
  if (araDocument.getMusicalContexts().size() > 0)
    selectMusicalContext(araDocument.getMusicalContexts().front());

  addAndMakeVisible(rulersHeader);

  viewport.content.addAndMakeVisible(rulersView);

  viewport.onVisibleAreaChanged = [this](const auto &r) {
    viewportHeightOffset = r.getY();
    overlay.setHorizontalOffset(r.getX());
    resized();
  };

  addAndMakeVisible(viewport);
  addAndMakeVisible(overlay);
  addAndMakeVisible(playheadPositionLabel);

  zoomControls.setZoomInCallback([this] { zoom(2.0); });
  zoomControls.setZoomOutCallback([this] { zoom(0.5); });
  addAndMakeVisible(zoomControls);

  invalidateRegionSequenceViews();

  araDocument.addListener(this);
  araEditorView.addListener(this);
}

DocumentView::~DocumentView() {
  araEditorView.removeListener(this);
  araDocument.removeListener(this);
  selectMusicalContext(nullptr);
}

void DocumentView::didAddMusicalContextToDocument(
    ARADocument *, ARAMusicalContext *musicalContext) {
  if (selectedMusicalContext == nullptr)
    selectMusicalContext(musicalContext);
}

void DocumentView::willDestroyMusicalContext(
    ARAMusicalContext *musicalContext) {
  if (selectedMusicalContext == musicalContext)
    selectMusicalContext(nullptr);
}

void DocumentView::didReorderRegionSequencesInDocument(ARADocument *) {
  invalidateRegionSequenceViews();
}

void DocumentView::didAddRegionSequenceToDocument(ARADocument *,
                                                  ARARegionSequence *) {
  invalidateRegionSequenceViews();
}

void DocumentView::willRemoveRegionSequenceFromDocument(
    ARADocument *, ARARegionSequence *regionSequence) {
  removeRegionSequenceView(regionSequence);
}

void DocumentView::didEndEditing(ARADocument *) {
  rebuildRegionSequenceViews();
  update();
}

void DocumentView::changeListenerCallback(ChangeBroadcaster *) { update(); }

void DocumentView::onNewSelection(const ARAViewSelection &viewSelection) {
  auto getNewSelectedMusicalContext =
      [&viewSelection]() -> ARAMusicalContext * {
    if (!viewSelection.getRegionSequences().empty())
      return viewSelection.getRegionSequences<ARARegionSequence>()
          .front()
          ->getMusicalContext();
    else if (!viewSelection.getPlaybackRegions().empty())
      return viewSelection.getPlaybackRegions<ARAPlaybackRegion>()
          .front()
          ->getRegionSequence()
          ->getMusicalContext();

    return nullptr;
  };

  if (auto *newSelectedMusicalContext = getNewSelectedMusicalContext())
    if (newSelectedMusicalContext != selectedMusicalContext)
      selectMusicalContext(newSelectedMusicalContext);

  if (const auto timeRange = viewSelection.getTimeRange())
    overlay.setSelectedTimeRange(*timeRange);
  else
    overlay.setSelectedTimeRange(std::nullopt);
}

void DocumentView::onHideRegionSequences(
    const std::vector<ARARegionSequence *> &regionSequences) {
  hiddenRegionSequences = regionSequences;
  invalidateRegionSequenceViews();
}

void DocumentView::paint(Graphics &g) {
  g.fillAll(getLookAndFeel()
                .findColour(ResizableWindow::backgroundColourId)
                .darker());
}

void DocumentView::resized() {
  auto bounds = getLocalBounds();

  FlexBox fb;
  fb.justifyContent = FlexBox::JustifyContent::spaceBetween;
  fb.items.add(
      FlexItem(playheadPositionLabel).withWidth(450.0f).withMinWidth(250.0f));
  fb.items.add(FlexItem(zoomControls).withMinWidth(80.0f));
  fb.performLayout(bounds.removeFromBottom(40));

  auto headerBounds = bounds.removeFromLeft(headerWidth);
  rulersHeader.setBounds(headerBounds.removeFromTop(trackHeight));
  layOutVertically(headerBounds, trackHeaders, viewportHeightOffset);

  viewport.setBounds(bounds);
  overlay.setBounds(bounds.reduced(1));

  const auto width =
      jmax(timeToViewScaling.getXForTime(timelineLength), viewport.getWidth());
  const auto height = (int)(regionSequenceViews.size() + 1) * trackHeight;
  viewport.content.setSize(width, height);
  viewport.content.resized();
}

void DocumentView::selectMusicalContext(
    ARAMusicalContext *newSelectedMusicalContext) {
  if (auto oldContext =
          std::exchange(selectedMusicalContext, newSelectedMusicalContext);
      oldContext != selectedMusicalContext) {
    if (oldContext != nullptr)
      oldContext->removeListener(this);

    if (selectedMusicalContext != nullptr)
      selectedMusicalContext->addListener(this);

    rulersView.selectMusicalContext(selectedMusicalContext);
    playheadPositionLabel.selectMusicalContext(selectedMusicalContext);
  }
}

void DocumentView::zoom(double factor) {
  timeToViewScaling.zoom(factor);
  update();
}

template <typename T>
void DocumentView::layOutVertically(Rectangle<int> bounds, T &components,
                                    int verticalOffset) {
  bounds = bounds.withY(bounds.getY() - verticalOffset)
               .withHeight(bounds.getHeight() + verticalOffset);

  for (auto &component : components) {
    component.second->setBounds(bounds.removeFromTop(trackHeight));
    component.second->resized();
  }
}

void DocumentView::update() {
  timelineLength = 0.0;

  for (const auto &view : regionSequenceViews)
    timelineLength =
        std::max(timelineLength, view.second->getPlaybackDuration());

  resized();
}

void DocumentView::addTrackViews(ARARegionSequence *regionSequence) {
  const auto insertIntoMap = [](auto &map, auto key, auto value) -> auto & {
    auto it = map.insert({std::move(key), std::move(value)});
    return *(it.first->second);
  };

  auto &regionSequenceView = insertIntoMap(
      regionSequenceViews, RegionSequenceViewKey{regionSequence},
      std::make_unique<RegionSequenceView>(araEditorView, timeToViewScaling,
                                           *regionSequence, waveformCache));

  regionSequenceView.addChangeListener(this);
  viewport.content.addAndMakeVisible(regionSequenceView);

  auto &trackHeader = insertIntoMap(
      trackHeaders, RegionSequenceViewKey{regionSequence},
      std::make_unique<TrackHeader>(araEditorView, *regionSequence));

  addAndMakeVisible(trackHeader);
}

void DocumentView::removeRegionSequenceView(ARARegionSequence *regionSequence) {
  const auto &view =
      regionSequenceViews.find(RegionSequenceViewKey{regionSequence});

  if (view != regionSequenceViews.cend()) {
    removeChildComponent(view->second.get());
    regionSequenceViews.erase(view);
  }

  invalidateRegionSequenceViews();
}

void DocumentView::invalidateRegionSequenceViews() {
  regionSequenceViewsAreValid = false;
  rebuildRegionSequenceViews();
}

void DocumentView::rebuildRegionSequenceViews() {
  if (!regionSequenceViewsAreValid &&
      !araDocument.getDocumentController()->isHostEditingDocument()) {
    for (auto &view : regionSequenceViews)
      removeChildComponent(view.second.get());

    regionSequenceViews.clear();

    for (auto &view : trackHeaders)
      removeChildComponent(view.second.get());

    trackHeaders.clear();

    for (auto *regionSequence : araDocument.getRegionSequences())
      if (std::find(hiddenRegionSequences.begin(), hiddenRegionSequences.end(),
                    regionSequence) == hiddenRegionSequences.end())
        addTrackViews(regionSequence);

    update();

    regionSequenceViewsAreValid = true;
  }
}
