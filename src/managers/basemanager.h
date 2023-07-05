/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BASEMANAGER_H
#define BASEMANAGER_H

#include "editor.h"

class BaseManager : public QObject
{
    Q_OBJECT
public:
    explicit BaseManager(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~BaseManager() { m_editor = nullptr; }

    void setEditor(Editor* editor)
    {
        Q_ASSERT_X( editor != nullptr, "BaseManager", "Editor is null." );
        m_editor = editor;
    }

    Editor* editor() { return m_editor; }

protected:
    Editor* m_editor = nullptr;
};

#endif // BASEMANAGER_H
