// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause
//
// Thin Objective-C++ wrapper around NSWorkspace for opening URLs.
//
// system("open") is silenced by hosts (e.g. DaVinci Resolve) that sandbox plugin
// subprocesses.  LSOpenURLsWithApplication was the C alternative but was removed
// entirely from the CoreServices framework in macOS 26.  NSWorkspace is the current
// supported path and works correctly regardless of process sandboxing.

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

extern "C" {

/// Open a URL in the default browser.
/// Returns 0 immediately; the actual open is dispatched asynchronously by the OS.
/// Returns -1 if url_cstr is NULL or not a valid URL.
int ofx_open_url(const char* url_cstr)
{
    if (!url_cstr) return -1;

    NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:url_cstr]];
    if (!url) return -1;

    [[NSWorkspace sharedWorkspace] openURL:url
                              configuration:[NSWorkspaceOpenConfiguration new]
                             completionHandler:^(NSRunningApplication* app, NSError* error) {
        (void)app;
        (void)error;
    }];

    return 0;
}

}  // extern "C"
