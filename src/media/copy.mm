#import <Cocoa/Cocoa.h>

#include "copy.h"

void copyFileToClipboard (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    NSPasteboard* pb = [NSPasteboard generalPasteboard];

    [pb declareTypes: [NSArray arrayWithObject: NSPasteboardTypeFileURL]
               owner: nil];

    NSString* path = [NSString stringWithUTF8String: file.getFullPathName().toUTF8()];
    NSURL* fileURL = [NSURL fileURLWithPath: path];

    [pb setString: [fileURL absoluteString]
          forType: NSPasteboardTypeFileURL];
}