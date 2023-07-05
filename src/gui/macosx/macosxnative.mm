/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2013-2017 Matt Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
 
 #include "macosxnative.h"

#include <AppKit/NSWindow.h>
#include <Availability.h>

namespace MacOSXNative
{
    #if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_14
        NSString* const NSAppearanceNameDarkAqua = @"NSAppearanceNameDarkAqua";
    #endif

    void removeUnwantedMenuItems()
    {
        // Remove "Show Tab Bar" option from the "View" menu if possible

        if ([NSWindow respondsToSelector:@selector(allowsAutomaticWindowTabbing)])
        {
            NSWindow.allowsAutomaticWindowTabbing = NO;
        }
    }

    bool isMouseCoalescingEnabled()
    {
        return NSEvent.isMouseCoalescingEnabled;
    }

    void setMouseCoalescingEnabled(bool enabled)
    {
        NSEvent.mouseCoalescingEnabled = enabled;
    }

    bool isDarkMode()
    {
        if (@available(macOS 10.14, *))
        {
            NSAppearanceName appearance =
                [[NSApp effectiveAppearance] bestMatchFromAppearancesWithNames:@[
                  NSAppearanceNameAqua, NSAppearanceNameDarkAqua
                ]];
            return [appearance isEqual:NSAppearanceNameDarkAqua];
        }
        return false;
    }
}
