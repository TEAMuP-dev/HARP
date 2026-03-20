#include <windows.h>
#include "copy.h"

void copyFileToClipboard (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    if (!OpenClipboard(nullptr))
        return;

    EmptyClipboard();

    std::wstring path = file.getFullPathName().toWideCharPointer();

    size_t size = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);

    HGLOBAL hMem = GlobalAlloc(GHND, size);
    DROPFILES* df = (DROPFILES*)GlobalLock(hMem);

    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;

    wchar_t* data = (wchar_t*)((BYTE*)df + sizeof(DROPFILES));
    wcscpy(data, path.c_str());

    GlobalUnlock(hMem);

    SetClipboardData(CF_HDROP, hMem);
    CloseClipboard();
}