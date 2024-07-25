//
//  NoteComponent.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 16/08/2019.
//

#include "PNoteComponent.hpp"

PNoteComponent::PNoteComponent ()
{
    mouseOver = useCustomColour = false;
    setMouseCursor(normal);
    startWidth = startX = startY = -1;
    coordiantesDiffer = false;
    isMultiDrag = false;
    state = eNone;

    setCustomColour(Colours::green);
}

PNoteComponent::~PNoteComponent () {}

void PNoteComponent::paint (Graphics & g)
{
    Colour orange(253,188,64);
    orange = orange.brighter();
    Colour red(252,97,92);

    g.fillAll(Colours::darkgrey); //border...
    Colour cToUse;
    if (useCustomColour && model.flags.isGenerative) {
        cToUse = customColour;
    } else {
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
    if (getWidth() > 10) {
        g.setColour(cToUse.brighter());
        const int lineMax = getWidth() - 5;

        g.drawLine(5, getHeight() * 0.5 - 2, lineMax * (model.getVelocity()/127.0), getHeight() * 0.5 - 2, 4);
    }

    String toDraw;
    //toDraw += String(PRE::pitches_names[model.getNote() % 12]) + String(model.getNote() / 12) + String(" ");
    //toDraw += String(model.getNote());

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
