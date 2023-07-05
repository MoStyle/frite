/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2008-2009 Mj Mendoza IV
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TABLETAPPLICATION_H
#define TABLETAPPLICATION_H

#include <QApplication>

#include "tabletcanvas.h"

class TabletApplication : public QApplication
{
    Q_OBJECT

public:
    TabletApplication(int &argv, char **args)
        : QApplication(argv, args) {}

    bool event(QEvent *event) Q_DECL_OVERRIDE;

    void setCanvas(TabletCanvas *canvas) { m_canvas = canvas; }

private:
    TabletCanvas *m_canvas;
};

#endif
