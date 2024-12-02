/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QColorDialog;
QT_END_NAMESPACE
class TabletCanvas;
class QUndoView;

class DialsAndKnobs;
class TimeLine;
class LayerManager;
class Editor;
class ColorBox;
class FileManager;
class StyleManager;
class ProjectPropertiesDialog;
class PreferencesDialog;
class OnionSkinsDocker;
class GroupsWidget;
class QPieMenu;
class QPushButton;
class QActionGroup;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(TabletCanvas *canvas);
    ~MainWindow() Q_DECL_OVERRIDE;

protected:
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent* event) Q_DECL_OVERRIDE;
    void dragMoveEvent(QDragMoveEvent* event) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent* event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent* event) Q_DECL_OVERRIDE;

private slots:
    void openRecentFile();
    void updateZoomLabel();
    void setProjectProperties();
    void setPreferences();
    void setAlphaValuator(QAction *action);
    void setLineWidthValuator(QAction *action);
    void setSaturationValuator(QAction *action);
    void setToolValuator(QAction *action);
    void newProject();
    void save();
    void saveAs();
    void load();
    void about();
    void updateColorIcon(const QColor &c);
    void updateTitleSaveState(bool saved);
    void exportImageSequence();
    // void importImageSequence();

private:
    bool maybeSave();
    bool openProject(const QString &filename);
    bool saveProject(const QString &filename);
    void addToRecentFiles(const QString &filename);
    void createMenus();
    void createToolBar();
    void updateRecentFileActions();
    void makeTimeLineConnections();
    void makeGroupsWidgetConnections();
    void readSettings();

    TabletCanvas *m_canvas;
    ColorBox     *m_colorBox;
    TimeLine     *m_timeLine;
    LayerManager *m_layers;
    Editor       *m_editor;
    FileManager  *m_fileManager;
    ProjectPropertiesDialog* m_projectDialog;
    PreferencesDialog* m_preferenceDialog;

    OnionSkinsDocker* m_onionSkinsDock;
    QDockWidget* m_historyDock;
    QUndoView* m_undoView;
    QAction* m_colorAction, *m_saveAction;
    QMenu*   m_windowsMenu;
    GroupsWidget* m_groupsWidget;

    QPieMenu *m_pieMenu;
    QDockWidget *m_optionsDock;

    DialsAndKnobs *m_dials_and_knobs;
    QVector<QAction*> m_recent_workingset_action;

    QActionGroup *m_toolGroup;
};

#endif
