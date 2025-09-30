/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __GROUPLISTWIDGET_H__
#define __GROUPLISTWIDGET_H__

#include <QWidget>
#include <QHash>
#include <QVBoxLayout>
#include <QLabel>

class Editor;
class GroupList;

class GroupListWidget : public QWidget {
    Q_OBJECT

public:
    GroupListWidget(Editor *editor, QWidget *parent = nullptr);

    void updateAll(const GroupList& groupList);

    void clearAll();

    QWidget *groupInfoWidget(int id) const;

    unsigned int nbGroups() const { return m_groupWidgets.size(); }

private:
    Editor *m_editor;
    QVBoxLayout *m_layout;
    QLabel *m_headerLabel;

    QHash<int, QWidget *> m_groupWidgets;
};

#endif // __GROUPLISTWIDGET_H__