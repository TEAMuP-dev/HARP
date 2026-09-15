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
#include <chrono>
#include <memory>
#include <thread>

#include "../Interface.h"

namespace
{
/* How long the worker waits between checks for a paste request. It also bounds
   how long replacing the clipboard blocks the caller, since that joins. */
constexpr auto pollInterval = std::chrono::milliseconds(10);

/*
  An owner of the X11 CLIPBOARD selection, and the thread that services it.

  X11 keeps no copy of what was copied: the owner has to stay alive and answer a
  SelectionRequest every time something asks to paste. That is what the thread is
  for, and why the state outlives the call that created it.

  All of the Xlib setup happens on the calling thread before the worker starts,
  and the worker is joined before anything is torn down, so only one thread ever
  touches the connection and XInitThreads is not needed.
*/
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

    std::thread worker;

    ~Clipboard()
    {
        running = false;

        /* Joining rather than detaching is what makes the teardown below safe. The
           worker polls, so this waits up to one poll interval. */
        if (worker.joinable())
        {
            worker.join();
        }

        if (display != nullptr)
        {
            if (window != 0)
            {
                XDestroyWindow(display, window);
            }

            XCloseDisplay(display);
        }
    }
};

/* Ownership is released only when a later copy replaces it, or at shutdown. The
   selection is given up either way, since X ties it to the connection. */
std::unique_ptr<Clipboard> clipboardState;

void handleRequest(Clipboard& state, XEvent& e)
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

    if (req->target == state.targets)
    {
        Atom list[] = { state.targets, state.uriList, state.textPlain, state.utf8, state.gnome };

        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char*) list,
                        (int) (sizeof(list) / sizeof(list[0])));

        res.xselection.property = req->property;
    }
    else if (req->target == state.uriList)
    {
        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        state.uriList,
                        8,
                        PropModeReplace,
                        (unsigned char*) state.dataUri.c_str(),
                        (int) state.dataUri.size());

        res.xselection.property = req->property;
    }
    else if (req->target == state.textPlain || req->target == state.utf8)
    {
        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        req->target,
                        8,
                        PropModeReplace,
                        (unsigned char*) state.dataPlain.c_str(),
                        (int) state.dataPlain.size());

        res.xselection.property = req->property;
    }
    else if (req->target == state.gnome)
    {
        XChangeProperty(req->display,
                        req->requestor,
                        req->property,
                        state.gnome,
                        8,
                        PropModeReplace,
                        (unsigned char*) state.dataGnome.c_str(),
                        (int) state.dataGnome.size());

        res.xselection.property = req->property;
    }

    // A property of None refuses a target that is not on offer, as ICCCM requires
    XSendEvent(req->display, req->requestor, False, 0, &res);
    XFlush(req->display);
}

/* Takes the state it services as an argument rather than reading the global, so
   that replacing the global cannot pull the state out from under a running loop. */
void runLoop(Clipboard* state)
{
    while (state->running)
    {
        while (XPending(state->display))
        {
            XEvent e;
            XNextEvent(state->display, &e);

            if (e.type == SelectionRequest)
                handleRequest(*state, e);
        }

        std::this_thread::sleep_for(pollInterval);
    }
}
} // namespace

void copyFileToClipboard(const File& file)
{
    if (! file.existsAsFile())
        return;

    /* Give up the previous ownership first. The destructor joins its worker, so the
       state it is still reading cannot be freed underneath it. */
    clipboardState.reset();

    auto state = std::make_unique<Clipboard>();

    state->display = XOpenDisplay(nullptr);

    if (state->display == nullptr)
        return;

    state->window =
        XCreateSimpleWindow(state->display, DefaultRootWindow(state->display), 0, 0, 1, 1, 0, 0, 0);

    state->clipboard = XInternAtom(state->display, "CLIPBOARD", False);
    state->targets = XInternAtom(state->display, "TARGETS", False);
    state->uriList = XInternAtom(state->display, "text/uri-list", False);
    state->textPlain = XInternAtom(state->display, "text/plain", False);
    state->utf8 = XInternAtom(state->display, "UTF8_STRING", False);
    state->gnome = XInternAtom(state->display, "x-special/gnome-copied-files", False);

    /* Percent-encodes the path, which a URI built by concatenation does not. A name
       containing a space or a "#" would otherwise produce a URI the file manager
       either truncates or rejects. */
    std::string uri = URL(file).toString(false).toStdString();

    std::string path = file.getFullPathName().toStdString();

    // RFC 2483 terminates each entry of a uri-list, so the trailing break belongs here
    state->dataUri = uri + "\r\n";

    state->dataPlain = path;

    /* The GNOME format is an operation followed by one URI per line, where the line
       break separates entries rather than terminating them. A trailing break leaves
       an empty final entry, which the file manager takes for a second file, fails to
       find, and reports through a dialog offering to skip it. The real file is still
       pasted, which is why the warning looks spurious. */
    state->dataGnome = "copy\n" + uri;

    XSetSelectionOwner(state->display, state->clipboard, state->window, CurrentTime);

    /* Started last, so that every Xlib call above has already run on this thread and
       the worker is the only one using the connection from here on. */
    state->worker = std::thread(runLoop, state.get());

    clipboardState = std::move(state);
}
