/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2013-2017 Matt Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QString>
#include <QStringList>
#include <QDir>
#include <QProcess>
#include <QProgressDialog>
#include <QSysInfo>
#include <QSettings>
#include <QDebug>

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  #include <QOperatingSystemVersion>
#endif

#ifdef __APPLE__
#include "macosxnative.h"
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace PlatformHandler
{

    void configurePlatformSpecificSettings()
    {
#ifdef __APPLE__
        MacOSXNative::removeUnwantedMenuItems();
#endif
    }

    bool isDarkMode()
    {
#ifdef __APPLE__
        return MacOSXNative::isDarkMode();
#else
        return false;
#endif
    }

}

#ifdef __APPLE__

extern "C" {
// this is not declared in Carbon.h anymore, but it's in the framework
OSStatus
SetMouseCoalescingEnabled(
 Boolean    inNewState,
 Boolean *  outOldState);
}

extern "C" {

bool gIsMouseCoalecing = false;

void detectWhichOSX()
{
 #if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    QOperatingSystemVersion current = QOperatingSystemVersion::current();
    gIsMouseCoalecing = ( current >= QOperatingSystemVersion::OSXElCapitan );
#else
    gIsMouseCoalecing = false;
#endif
}

void disableCoalescing()
{
    SetMouseCoalescingEnabled(gIsMouseCoalecing, NULL);
}

void enableCoalescing()
{
    SetMouseCoalescingEnabled(true, NULL);
}

} // extern "C"
#endif
