//
//  NoteGridComponent.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 16/08/2019.
//

#include "NoteGridComponent.hpp"
#include <array>

#ifndef LIB_VERSION
#include "DataLoggerRoot.h"
#endif


#define RETURN_IF_EDITING_DISABLED if(styleSheet.disableEditing) { return; }

NoteGridComponent::NoteGridComponent (NoteGridStyleSheet & ss) : styleSheet(ss)
{
    blackPitches.add(1);
    blackPitches.add(3);
    blackPitches.add(6);
    blackPitches.add(8);
    blackPitches.add(10);
    addChildComponent(&selectorBox);
    addKeyListener(this);
    setWantsKeyboardFocus(true);
    currentQValue = PRE::quantisedDivisionValues[PRE::eQuantisationValue1_32];
    lastNoteLength = PRE::quantisedDivisionValues[PRE::eQuantisationValue1_4];
    firstDrag = false;
    firstCall = false;
    lastTrigger = -1;
    ticksPerTimeSignature = PRE::defaultResolution * 4; //4/4 assume

}
NoteGridComponent::~NoteGridComponent ()
{
    for (int i = 0; i < noteComps.size(); i++) {
        removeChildComponent(noteComps[i]);
        delete noteComps[i];
    }
}

void NoteGridComponent::paint (Graphics & g)
{
    g.fillAll(Colours::darkgrey);
    
    const int totalBars = (getWidth() / pixelsPerBar) + 1;
    
    
    //draw piano roll background first.
    {
        float line = 0;//noteCompHeight;
        
        for (int i = 127; i >= 0; i--) {
            const int pitch = i % 12;
            g.setColour(blackPitches.contains(pitch) ?
                        Colours::darkgrey.withAlpha(0.5f) :
                        Colours::lightgrey.darker().withAlpha(0.5f));
         
            g.fillRect(0, (int)line, getWidth(), (int)noteCompHeight);
//            g.setColour(Colours::white);
//            g.drawText(String(i), 5, line, 40, noteCompHeight, Justification::left);
            
            line += noteCompHeight;
            g.setColour(Colours::black);
            g.drawLine(0, line, getWidth(), line);
        }
    }
    
    //again this is assuming 4/4
    const float increment = pixelsPerBar / 16;
    float line = 0;
    g.setColour(Colours::lightgrey);
    for (int i = 0; line < getWidth() ; i++) {
        float lineThickness = 1.0;
        if (i % 16 == 0) { //bar marker
            lineThickness = 3.0;
        }
        else if (i % 4 == 0) { //1/4 div
            lineThickness = 2.0;
        }
        g.drawLine(line, 0, line, getHeight(), lineThickness);
        
        line += increment;
    }
    
}
void NoteGridComponent::resized ()
{
    
    for (auto component : noteComps) {
        if (component->coordiantesDiffer) {
            noteCompPositionMoved(component, false);
        }
        // convert from model representation into component representation (translation and scale)
        
        const float xPos = (component->getModel().getStartTime() / ((float) ticksPerTimeSignature)) * pixelsPerBar;
        const float yPos = (getHeight() - (component->getModel().getNote() * noteCompHeight)) - noteCompHeight;
        
        float len = (component->getModel().getNoteLegnth() / ((float) ticksPerTimeSignature)) * pixelsPerBar;
        
        component->setBounds(xPos, yPos, len, noteCompHeight);
    }

}




void NoteGridComponent::setupGrid (float px, float compHeight, const int bars)
{
    pixelsPerBar = px;
    noteCompHeight = compHeight;
    setSize(pixelsPerBar * bars, compHeight * 128); //we have 128 slots for notes
}

void NoteGridComponent::setQuantisation (const int val)
{
    if (val >= 0 && val <= PRE::eQuantisationValueTotal) {
        currentQValue = PRE::quantisedDivisionValues[val];
    }
    else {
        jassertfalse;
    }
}
//callback from PNoteComponent
void NoteGridComponent::noteCompSelected (PNoteComponent * nc, const MouseEvent& e)
{
    RETURN_IF_EDITING_DISABLED
    
    int dragMove = 0;
    for (auto component : noteComps) {
        if (component->isMultiDrag) {
            dragMove++;
        }
    }
//    std::cout << "Drag: " << dragMove << "\n";
    
    for (auto component : noteComps) {
        if (component == nc) {
            component->setState(PNoteComponent::eSelected);
            component->toFront(true);
        }
        /*
         This complicated if statement exists because if the user is dragging multiple notes around we don't want to clear the selection.
         We only want so switch the selected note when the user selects another note
         */
        else if (component->getState() == PNoteComponent::eSelected && !e.mods.isShiftDown() && !dragMove) {
            component->setState(PNoteComponent::eNone);
        }
    }
    // need to reset the multidrag
    for (auto component : noteComps) {
        if (component->isMultiDrag) {
            component->isMultiDrag = false;
        }
    }
    sendEdit();
}
void NoteGridComponent::noteCompPositionMoved (PNoteComponent * comp, bool callResize)
{
    RETURN_IF_EDITING_DISABLED
    
    if (!firstDrag) {

        firstDrag = true;
        // we want to move all the components...
        for (auto n : noteComps) {
            if (n != comp && n->getState() == PNoteComponent::eSelected) {
                noteCompPositionMoved(n, false);
            }
        }
        firstDrag = false;
        
    }
    
//could do with refactoring this code here..
    int xPos = (comp->getX() / ((float)pixelsPerBar)) * ticksPerTimeSignature;
    int note = 127 - (comp->getY() / noteCompHeight);
    if (note > 127) {
        note = 127;
    }
    else if (note < 0) {
        note = 0;
    }
    
    if (xPos <= 0) {
        xPos = 0;
    }
    
    const int len = (comp->getWidth() / ((float)pixelsPerBar)) * ticksPerTimeSignature;
    NoteModel nm = comp->getModel();
    nm.setNote(note);
    nm.setStartTime(xPos);
    nm.setNoteLegnth(len);
    nm.quantiseModel(currentQValue, true, true);
    nm.sendChange = sendChange;
    
    //todo: could make this toggleable behaviour
    lastNoteLength = nm.getNoteLegnth();
    
    comp->startY = -1;
    comp->startX = -1;
    comp->setValues(nm);
    if (callResize) {
        resized();
    }
    sendEdit();
}

void NoteGridComponent::noteCompLengthChanged (PNoteComponent * original, int diff)
{
    RETURN_IF_EDITING_DISABLED
    
    for (auto n : noteComps) {
        if (n->getState() == PNoteComponent::eSelected || n == original) {
            if (n->startWidth == -1) {
                n->startWidth = n->getWidth();
                n->coordiantesDiffer = true;
            }
            
            const int newWidth = n->startWidth - diff;
            // todo: this seems arbitary..
            if (newWidth > 20) {
                n->setSize(newWidth, n->getHeight());
            }
                
            
        }
    }
    sendEdit();
}

void NoteGridComponent::noteCompDragging (PNoteComponent* original, const MouseEvent& event)
{
    RETURN_IF_EDITING_DISABLED
    
    for (auto n : noteComps) {
        if (n->getState() == PNoteComponent::eSelected && n != original) {
            
            const int movedX = event.getDistanceFromDragStartX();// (event.getx - original->startX);
            const int movedY = event.getDistanceFromDragStartY(); //(original->getY() - original->startY);
            
            if (n->startY == -1) {
                n->startX = n->getX();
                n->startY = n->getY();
            }
            
            /*
            std::cout << "Started at: " << n->startX << " - " << n->startY << "\n";
            std::cout << n->getBounds().toString() << "\n";
             */
            
            const int newX = n->startX + movedX;
            const int newY = n->startY + movedY;
            const int xDif = abs(newX - n->startX);
            const int yDif = abs(newY - n->startY);
            if (xDif > 2 || yDif > 2) { //ingnore a small amount of jitter.
                n->setTopLeftPosition(newX, newY);
                n->isMultiDrag = true;
            }
            
            /*
            std::cout << "Moved: " << movedX << " : " << movedY << " -- " << n->getX() << " : " << n->getY() <<  "\n" ;
            std::cout << n->getBounds().toString() << "\n \n" ;
             */

        }

    }
    

    /*
     This enables the notes to be triggered while dragging.
     */
    int note = 127 - (original->getY() / noteCompHeight);
    if (note > 127) { note = 127; }
    else if (note < 0) { note = 0; }
    if (note != lastTrigger) {
        original->getModel().trigger(note, 100);
        lastTrigger = note;
    }
    
}
void NoteGridComponent::setPositions ()
{
    //unused..
}

void NoteGridComponent::mouseDown (const MouseEvent&)
{
    RETURN_IF_EDITING_DISABLED
    
    for (PNoteComponent * component : noteComps) {
        component->setState(PNoteComponent::eNone);
    }
    sendEdit();
}
void NoteGridComponent::mouseDrag (const MouseEvent& e)
{
    RETURN_IF_EDITING_DISABLED
    
    if (!selectorBox.isVisible()) {
        selectorBox.setVisible(true);
        selectorBox.toFront(false);
        
        selectorBox.setTopLeftPosition(e.getPosition());
        selectorBox.startX = e.getPosition().x;
        selectorBox.startY = e.getPosition().y;
        
    }
    else {
        int xDir = e.getPosition().x - selectorBox.startX;
        int yDir = e.getPosition().y - selectorBox.startY;
        
        //work out which way to draw the selection box
        if (xDir < 0 && yDir < 0) { //top left
            selectorBox.setTopLeftPosition(e.getPosition().x, e.getPosition().y);
            selectorBox.setSize(selectorBox.startX - e.getPosition().getX(), selectorBox.startY - e.getPosition().getY());
        }
        else if (xDir > 0 && yDir < 0) { //top right
            selectorBox.setTopLeftPosition(selectorBox.startX, e.getPosition().y);
            selectorBox.setSize(e.getPosition().getX() - selectorBox.startX, selectorBox.startY - e.getPosition().getY());
        }
        else if (xDir < 0 && yDir > 0) { //bottom left
            selectorBox.setTopLeftPosition(e.getPosition().x, selectorBox.startY);
            selectorBox.setSize(selectorBox.startX - e.getPosition().getX(), e.getPosition().getY() -  selectorBox.startY);
        }
        else { //bottom right
            selectorBox.setSize(e.getPosition().getX() - selectorBox.getX(), e.getPosition().getY() - selectorBox.getY());
        }
    }
}
void NoteGridComponent::mouseUp (const MouseEvent&)
{
    RETURN_IF_EDITING_DISABLED
    
    if (selectorBox.isVisible()) {
        
        
        for (PNoteComponent * component : noteComps) {
            if (component->getBounds().intersects(selectorBox.getBounds())) {
                component->setState(PNoteComponent::eState::eSelected);
            }
            else {
                component->setState(PNoteComponent::eState::eNone);
            }
        }
        selectorBox.setVisible(false);
        selectorBox.toFront(false);
        selectorBox.setSize(1,1);
    }
    
    sendEdit();
    
}

void NoteGridComponent::mouseDoubleClick (const MouseEvent& e)
{
    RETURN_IF_EDITING_DISABLED
    
    const int xPos = (e.getMouseDownX() / ((float)pixelsPerBar)) * ticksPerTimeSignature;
    const int yIn = ((float)e.getMouseDownY() / noteCompHeight);
    const int note = 127 - yIn;
    jassert(note >= 0 && note <= 127);
    
    /*set up lambdas..
    
     Essentialy each note component (child) sends messages back to parent (this) through a series of lambda callbacks
     */
    
    PNoteComponent * nn = new PNoteComponent(styleSheet);
    nn->onNoteSelect = [this](PNoteComponent * n, const MouseEvent& e) {
        this->noteCompSelected(n, e);
    };
    nn->onPositionMoved = [this](PNoteComponent * n) {
        this->noteCompPositionMoved(n);
    };
    nn->onLegnthChange = [this](PNoteComponent * n, int diff) {
        this->noteCompLengthChanged(n, diff);
    };
    nn->onDragging = [this](PNoteComponent * n, const MouseEvent & e) {
        this->noteCompDragging(n, e);
    };
    addAndMakeVisible(nn);
    
    const int defaultVelocity = 100;
    
    NoteModel nModel((u8)note, defaultVelocity, (st_int)xPos, lastNoteLength, {});
    nModel.quantiseModel(currentQValue, true, true);
    nModel.sendChange = sendChange;
    nModel.trigger();
    nn->setValues(nModel);
    

    noteComps.push_back(nn);
    
    resized();
    repaint();
    sendEdit();
}



bool NoteGridComponent::keyPressed (const KeyPress& key, Component* originatingComponent)
{
    
#ifndef LIB_VERSION
    LOG_KEY_PRESS(key.getKeyCode(), 1, key.getModifiers().getRawFlags());
#endif
    
    if (styleSheet.disableEditing) {
        return true;
    }
    if (key == KeyPress::backspaceKey) {
        //
        deleteAllSelected();
        sendEdit();
        return true;
    }
    else if (key == KeyPress::upKey || key == KeyPress::downKey) {
        bool didMove = false;
        for (auto nComp : noteComps) {
            if (nComp->getState() == PNoteComponent::eSelected) {
                NoteModel nModel =  nComp->getModel();
                
                (key == KeyPress::upKey) ?
                nModel.setNote(nModel.getNote() + 1) :
                nModel.setNote(nModel.getNote() - 1);
                
                nModel.sendChange = sendChange;
                nComp->setValues(nModel);
                didMove = true;
            }
        }
        if (didMove) {
            sendEdit();
            resized();
            return true;
            
        }
        
    }
    else if (key == KeyPress::leftKey || key == KeyPress::rightKey) {
        bool didMove = false;
        const int nudgeAmount = currentQValue;
        for (auto nComp : noteComps) {
            if (nComp->getState() == PNoteComponent::eSelected) {
                NoteModel nModel =  nComp->getModel();
                
                (key == KeyPress::rightKey) ?
                nModel.setStartTime(nModel.getStartTime() + nudgeAmount) :
                nModel.setStartTime(nModel.getStartTime() - nudgeAmount) ;
                
                nModel.sendChange = sendChange;
                nComp->setValues(nModel);
                didMove = true;
            }
        }
        if (didMove) {
            sendEdit();
            resized();
            return true;
        }
        
    }
    return false;
}

void NoteGridComponent::deleteAllSelected ()
{
    std::vector<PNoteComponent *> itemsToKeep;
    for (int i = 0; i < noteComps.size(); i++) {
        if (noteComps[i]->getState() == PNoteComponent::eSelected) {
            removeChildComponent(noteComps[i]);
            delete noteComps[i];
        }
        else {
            itemsToKeep.push_back(noteComps[i]);
        }
    }
    noteComps = itemsToKeep;
}

PRESequence NoteGridComponent::getSequence ()
{
    int leftToSort = (int) noteComps.size();
    
    std::vector<PNoteComponent *> componentsCopy = noteComps;
    /*
     inline lambda function to find the lowest startTime
     */
    auto findLowest = [&]() -> int {
        int lowestIndex = 0;
        for (int i = 0; i < componentsCopy.size(); i++) {
            if (componentsCopy[i]->getModel().getStartTime() < componentsCopy[lowestIndex]->getModel().getStartTime()) {
                lowestIndex = i;
            }
        }
        return lowestIndex;
    };
    
    
    PRESequence seq;
    while (leftToSort) {
        const int index = findLowest();
        auto m = componentsCopy[index]->getModel();
        m.flags.state = componentsCopy[index]->getState();
        seq.events.push_back(m);
//        seq.events[seq.events.size()-1]->flags =1  //we also want the selected flags..
        
        componentsCopy[index] = nullptr;
        componentsCopy.erase(componentsCopy.begin() + index);
        leftToSort--;
    }
    seq.print();
    return seq;
}

void NoteGridComponent::loadSequence (PRESequence sq)
{
    for (int i = 0; i < noteComps.size(); i++) {
        removeChildComponent(noteComps[i]);
        delete noteComps[i];
        
    }
    noteComps.clear();
    
    noteComps.reserve(sq.events.size());
    
    for (auto event : sq.events) {
        PNoteComponent * nn = new PNoteComponent(styleSheet);
        nn->onNoteSelect = [this](PNoteComponent * n, const MouseEvent& e) {
            this->noteCompSelected(n, e);
        };
        nn->onPositionMoved = [this](PNoteComponent * n) {
            this->noteCompPositionMoved(n);
        };
        nn->onLegnthChange = [this](PNoteComponent * n, int diff) {
            this->noteCompLengthChanged(n, diff);
        };
        nn->onDragging = [this](PNoteComponent * n, const MouseEvent & e) {
            this->noteCompDragging(n, e);
        };
        addAndMakeVisible(nn);
        NoteModel nModel(event);
        nModel.sendChange = sendChange;
//        nModel.quantiseModel(PRE::defaultResolution / 8, true, true);
        nn->setValues(nModel);
        
        noteComps.push_back(nn);

    }
    resized();
    repaint();
}

float NoteGridComponent::getNoteCompHeight ()
{
    return noteCompHeight;
}
float NoteGridComponent::getPixelsPerBar ()
{
    return pixelsPerBar;
}
std::vector<NoteModel *> NoteGridComponent::getSelectedModels ()
{
    std::vector<NoteModel *> noteModels;
    for (auto comp : noteComps) {
        if (comp->getState()) {
           noteModels.push_back(comp->getModelPtr());
        }
    }
    return noteModels;
}

void NoteGridComponent::sendEdit ()
{
    if (this->onEdit != nullptr) {
        this->onEdit();
    }
}
