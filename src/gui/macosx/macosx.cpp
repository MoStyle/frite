/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2013-2017 Matt Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

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
