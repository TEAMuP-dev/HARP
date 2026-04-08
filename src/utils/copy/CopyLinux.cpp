/**
 * @file CopyLinux.cpp
 * @brief Copy file path to clipboard on Linux.
 * @author JEYuhas, cwitkowitz
 */

#include "../Interface.h"

void copyFileToClipboard(const File& file)
{
    if (! file.existsAsFile())
        return;

    String cmd;

    cmd << "printf \"file://" << file.getFullPathName()
        << "\\n\" | xclip -selection clipboard -t text/uri-list";

    std::system(cmd.toRawUTF8());
}
