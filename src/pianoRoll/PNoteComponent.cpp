//
//  NoteComponent.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 16/08/2019.
//

#include "PNoteComponent.hpp"

#define CHECK_EDIT if(styleSheet.disableEditing) { return; }

PNoteComponent::PNoteComponent (NoteGridStyleSheet & ss) : styleSheet(ss), edgeResizer(this, nullptr, ResizableEdgeComponent::Edge::rightEdge)
{
    mouseOver = useCustomColour = false;
    addAndMakeVisible(edgeResizer);
    setMouseCursor(normal);
    startWidth = startX = startY = -1;
    coordiantesDiffer = false;
    isMultiDrag = false;
    state = eNone;
    
    setCustomColour(Colours::green);
    
    
}
PNoteComponent::~PNoteComponent ()
{
    
}

void PNoteComponent::paint (Graphics & g)
{
    Colour orange(253,188,64);
    orange = orange.brighter();
    Colour red(252,97,92);
    
    g.fillAll(Colours::darkgrey); //border...
    Colour cToUse;
    if (useCustomColour && model.flags.isGenerative) {
        cToUse = customColour;
    }
    else {
//        cToUse = Colour::fromHSV(model.velocity/127.0, 1.0, 0.5, 0xFF);
        //cToUse = orange.interpolatedWith(red, model.velocity / 127.0);
        cToUse = red;
    }
    
    
    
    if (state || mouseOver) {
        cToUse = cToUse.brighter().brighter();
    }
    g.setColour(cToUse);
    
    //draw middle box.
    g.fillRect(1, 1, getWidth() - 2, getHeight() - 2);

    
    //draw velocity
    if (getWidth() > 10 && styleSheet.getDrawVelocity()) {
        g.setColour(cToUse.brighter());
        const int lineMax = getWidth() - 5;
        
        g.drawLine(5, getHeight() * 0.5 - 2, lineMax * (model.getVelocity()/127.0), getHeight() * 0.5 - 2, 4);
    }
    String toDraw;
    if (styleSheet.getDrawMIDINoteStr()) {
        toDraw += String(PRE::pitches_names[model.getNote() % 12]) + String(model.getNote() / 12) + String(" ");
    }
    if (styleSheet.getDrawMIDINum()) {
        toDraw += String(model.getNote());
    }
    
    g.setColour(Colours::white);
    g.drawText(String(toDraw), 3, 3, getWidth() - 6, getHeight() - 6, Justification::centred);
    
}
void PNoteComponent::resized ()
{
//    edgeResizer.setBounds(getWidth() - 10, getHeight(), 10, getHeight());
}
void PNoteComponent::setCustomColour (Colour c)
{
    customColour = c;
    useCustomColour = true;
}

void PNoteComponent::setValues (NoteModel m)
{
    
    if (m.getNote() == 255) { m.setNote(0); } //unsigned overflow
    if (m.getNote() > 127) { m.setNote(127); }
    
    //cast to int as noteLen is unsigned. allows us to check for 0
    if (((int)m.getStartTime()) < 0) { m.setStartTime(0); }
    
    model = m;
    repaint();

}
NoteModel PNoteComponent::getModel ()
{
    return model;
}
NoteModel * PNoteComponent::getModelPtr ()
{
    return &model;
}

void PNoteComponent::setState (eState s)
{
    state = s;
    repaint();
}
PNoteComponent::eState PNoteComponent::getState ()
{
    return state;
}

void PNoteComponent::mouseEnter (const MouseEvent&)
{
    CHECK_EDIT
    mouseOver = true;
    repaint();
}
void PNoteComponent::mouseExit  (const MouseEvent&)
{
    CHECK_EDIT
    mouseOver = false;
    setMouseCursor(MouseCursor::NormalCursor);
    repaint();
    
}
void PNoteComponent::mouseDown (const MouseEvent& e)
{
    CHECK_EDIT

    if (e.mods.isShiftDown()) {
        velocityEnabled = true;
        startVelocity = model.getVelocity();
    }
    else if (getWidth() - e.getMouseDownX() < 10) {
        resizeEnabled = true;
        startWidth = getWidth();
    }
    else {
        startDraggingComponent(this, e);
        
    }
    if (!resizeEnabled) {
        
    }
}
void PNoteComponent::mouseUp (const MouseEvent& e)
{
    CHECK_EDIT
    if (onPositionMoved != nullptr) {
        onPositionMoved(this);
    }
    if (onNoteSelect != nullptr) {
        onNoteSelect(this, e);
    }
    startWidth = startX = startY -1;
    mouseOver = false;
    resizeEnabled = false;
    velocityEnabled = false;
    repaint();
    isMultiDrag = false;
    
}
void PNoteComponent::mouseDrag  (const MouseEvent& e)
{
    CHECK_EDIT
    //velocityEnabled
    if (resizeEnabled) {
        if (onLegnthChange != nullptr) {
            onLegnthChange(this, startWidth-e.getPosition().getX());
        }

    }
    else if (velocityEnabled) {
        int velocityDiff = e.getDistanceFromDragStartY() * -0.5;
        int newVelocity = startVelocity + velocityDiff;
        if (newVelocity < 1) {
            newVelocity = 1;
        }
        else if (newVelocity > 127) {
            newVelocity = 127;
        }
        model.setVelocity(newVelocity);
        repaint();
//        std::cout << velocityDiff << "\n";
        
    }
    else {
        setMouseCursor(MouseCursor::DraggingHandCursor);
        dragComponent(this, e, nullptr);
        
        if (onDragging != nullptr ) { //&& isMultiDrag
            onDragging(this, e);
        }
    }
    
}
void PNoteComponent::mouseMove  (const MouseEvent& e)
{
    CHECK_EDIT
    if (getWidth() - e.getMouseDownX() < 10) {
        setMouseCursor(MouseCursor::RightEdgeResizeCursor);
    }
    else {
        setMouseCursor(MouseCursor::NormalCursor);
    }
}

