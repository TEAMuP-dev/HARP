//
//  PianoRollEditorComponent.hpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 05/02/2020.
//

#ifndef PianoRollEditorComponent_hpp
#define PianoRollEditorComponent_hpp

#include "PConstants.h"
#include "NoteGridControlPanel.hpp"
#include "TimelineComponent.hpp"
#include "KeyboardComponent.hpp"

/*
 This custom viewport is used so that when the main piano roll is moved the others can be moved also, see diagram below
 */
class CustomViewport : public Viewport
{
public:
    void visibleAreaChanged (const Rectangle<int>& newVisibleArea)
    {
        Viewport::visibleAreaChanged(newVisibleArea);
        if (positionMoved) {
            positionMoved(getViewPositionX(), getViewPositionY());
        }
    }
    
    std::function<void(int,int)> positionMoved;
};


/*
 
 |      |   timeline component
 |______|__________________________________________
 |      |
 |      |
 |keys  |   main viewport and note grid component
 |      |
 |      |
 |      |
 |      |
 |      |
 |      |
 |      |
 |      |
 |      |
 
 
 */

class PianoRollEditorComponent : public Component
{
public:
    
    struct ExternalModelEditor {
        std::vector<NoteModel *> models; //const event pointers but mutable elements
        std::function<void()> update; //once you have made the edits then call this
    };
    
    //==============================================================================
    PianoRollEditorComponent();
    ~PianoRollEditorComponent();
    //==============================================================================
    
    /*
     This needs to be called after the constructor and determines the size of the grid.
     
     Once setup the number of bars can be dynamically altered through @updateBars(..
     
     Todo: automatically resize the number of bars
     */
    void setup (const int bars, const int pixelsPerBar, const int noteHeight);
    void updateBars (const int newNumberOfBars);
    
    
    void paint (Graphics&) override;
    void paintOverChildren (Graphics&) override;

    void resized() override;
    
    void showControlPannel (bool state);
//    void setStyleSheet (NoteGridStyleSheet style);
    
    
    /*
     PRE Sequence is essentially the input and output to the grid, i.e. the data model abstracted away from the GUI
     The GUI creates a PRESequence
     The GUI understands how to render a PRESequence
     
     Although this approach is probably inefficent, its unlikely to cause realtime performance issues...
     */
    void loadSequence (PRESequence sequence);

    // void clearSequence();

    PRESequence getSequence ();
    
    
    void setScroll (double x, double y);
    void setPlaybackMarkerPosition (const st_int ticks, bool isVisable = true); 
    void setZoomFactor (float factor);
    
    /*
     If you just want to view the editor then disable the grid.
     */
    void disableEditing (bool value);
    NoteGridControlPanel & getControlPannel ();
    
    /*
     Returns any notes that are selected used in IGME
     */
    ExternalModelEditor getSelectedNoteModels ();
    
    /*
     This is called when the grid is edited.
     */
    std::function<void()> onEdit;
    /*
     You can use this to implement simple MIDI synthesis when notes are being edited,
     when notes are edited this function will be called
     */
    std::function<void(int note,int velocity)> sendChange;

private:
    //==============================================================================
    
    NoteGridStyleSheet      styleSheet;
    
    /*
     These 3 essential components make up the piano roll editor.
     Each is stored in a customViewport instance, that are coupled to move in unison
     */
    NoteGridComponent       noteGrid; //the actual piano roll
    TimelineComponent       timelineComp; // the timeline marker at the top
    KeyboardComponent       keyboardComp; // the keyboard visualiser to the left
    
    CustomViewport          viewportGrid, viewportPiano, viewportTimeline;
    
    /*
     Optional control pannel
     */
    NoteGridControlPanel    controlPannel;
    
    st_int  playbackTicks;
    bool    showPlaybackMarker;
    
    JUCE_DECLARE_NON_COPYABLE(PianoRollEditorComponent)
//    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    
    
};


#endif /* PianoRollEditorComponent_hpp */
