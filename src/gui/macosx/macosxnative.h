/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2013-2017 Matt Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MACOSXNATIVE_H
#define MACOSXNATIVE_H

namespace MacOSXNative
{
    void removeUnwantedMenuItems();

    bool isMouseCoalescingEnabled();
    void setMouseCoalescingEnabled(bool enabled);
    bool isDarkMode();
}

#endif // MACOSXNATIVE_H
