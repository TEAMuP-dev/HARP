#include <gtk/gtk.h>
#include "copy.h"

void copyFileToClipboard (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    const juce::String uri = "file://" + file.getFullPathName();

    GtkClipboard* clipboard =
        gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

    gtk_clipboard_set_text(clipboard,
                           uri.toRawUTF8(),
                           -1);
}