/**
 * @file CopyLinux.cpp
 * @brief Copy file path to clipboard on Linux.
 * @author JEYuhas, cwitkowitz
 */

extern "C"
{
#include <X11/Xatom.h>
#include <X11/Xlib.h>
}

#include <atomic>
#include <thread>

#include "../Interface.h"

namespace
{
struct Clipboard
{
    Display* display = nullptr;
    Window window = 0;

    Atom clipboard;
    Atom targets;
    Atom uriList;
    Atom textPlain;
    Atom utf8;
    Atom gnome;

    std::string dataUri;
    std::string dataPlain;
    std::string dataGnome;

    std::atomic<bool> running { true };
};

Clipboard* clipboardState = nullptr;

void handleRequest(XEvent& e)
{
    auto* req = &e.xselectionrequest;

    XEvent res {};
    res.xselection.type = SelectionNotify;
    res.xselection.display = req->display;
    res.xselection.requestor = req->requestor;
    res.xselection.selection = req->selection;
    res.xselection.target = req->target;
    res.xselection.time = req->time;
    res.xselection.property = None;

    if (req->target == clipboardState->targets)
    {
        Atom list[] = { clipboardState->targets,
                        clipboardState->uriList,
                        clipboardState->textPlain,
                        clipboardState->utf8,
                        clipboardState->gnome };

        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char*) list,
                        5);

        res.xselection.property = req->property;
    }
    else if (req->target == clipboardState->uriList)
    {
        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        clipboardState->uriList,
                        8,
                        PropModeReplace,
                        (unsigned char*) clipboardState->dataUri.c_str(),
                        (int) clipboardState->dataUri.size());

        res.xselection.property = req->property;
    }
    else if (req->target == clipboardState->textPlain || req->target == clipboardState->utf8)
    {
        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        req->target,
                        8,
                        PropModeReplace,
                        (unsigned char*) clipboardState->dataPlain.c_str(),
                        (int) clipboardState->dataPlain.size());

        res.xselection.property = req->property;
    }
    else if (req->target == clipboardState->gnome)
    {
        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        clipboardState->gnome,
                        8,
                        PropModeReplace,
                        (unsigned char*) clipboardState->dataGnome.c_str(),
                        (int) clipboardState->dataGnome.size());

        res.xselection.property = req->property;
    }

    XSendEvent(req->display, req->requestor, False, 0, &res);
    XFlush(req->display);
}

void runLoop()
{
    while (clipboardState->running)
    {
        while (XPending(clipboardState->display))
        {
            XEvent e;
            XNextEvent(clipboardState->display, &e);

            if (e.type == SelectionRequest)
                handleRequest(e);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
} // namespace

void copyFileToClipboard(const File& file)
{
    if (! file.existsAsFile())
        return;

    if (clipboardState)
    {
        clipboardState->running = false;
        delete clipboardState;
        clipboardState = nullptr;
    }

    clipboardState = new Clipboard();

    clipboardState->display = XOpenDisplay(nullptr);
    if (! clipboardState->display)
        return;

    clipboardState->window = XCreateSimpleWindow(
        clipboardState->display, DefaultRootWindow(clipboardState->display), 0, 0, 1, 1, 0, 0, 0);

    clipboardState->clipboard = XInternAtom(clipboardState->display, "CLIPBOARD", False);
    clipboardState->targets = XInternAtom(clipboardState->display, "TARGETS", False);
    clipboardState->uriList = XInternAtom(clipboardState->display, "text/uri-list", False);
    clipboardState->textPlain = XInternAtom(clipboardState->display, "text/plain", False);
    clipboardState->utf8 = XInternAtom(clipboardState->display, "UTF8_STRING", False);
    clipboardState->gnome =
        XInternAtom(clipboardState->display, "x-special/gnome-copied-files", False);

    std::string path = file.getFullPathName().toStdString();
    std::string uri = "file://" + path;

    clipboardState->dataUri = uri + "\r\n";
    clipboardState->dataPlain = path;
    clipboardState->dataGnome = "copy\n" + uri + "\n";

    XSetSelectionOwner(
        clipboardState->display, clipboardState->clipboard, clipboardState->window, CurrentTime);

    std::thread(runLoop).detach();
}
