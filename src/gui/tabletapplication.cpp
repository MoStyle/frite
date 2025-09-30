/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2008-2009 Mj Mendoza IV
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtWidgets>

#include "tabletapplication.h"

bool TabletApplication::event(QEvent *event)
{
    if (event->type() == QEvent::TabletEnterProximity ||
        event->type() == QEvent::TabletLeaveProximity) {
        m_canvas->setTabletDevice(static_cast<QTabletEvent *>(event));
        return true;
    }
    return QApplication::event(event);
}
