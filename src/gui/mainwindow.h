/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2008-2009 Mj Mendoza IV
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

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
