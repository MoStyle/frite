/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2008-2009 Mj Mendoza IV
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <QSettings>
#include <QStatusBar>
#include <QPushButton>
#include <QLabel>

#include "mainwindow.h"
#include "tabletcanvas.h"
#include "dialsandknobs.h"
#include "timeline.h"
#include "timecontrols.h"
#include "layermanager.h"
#include "playbackmanager.h"
#include "colormanager.h"
#include "viewmanager.h"
#include "editor.h"
#include "projectpropertiesdialog.h"
#include "preferencesdialog.h"
#include "filemanager.h"
#include "stylemanager.h"
#include "colorbox.h"
#include "onion_skins_docker.h"
#include "groupswidget.h"
#include "toolsmanager.h"
#include "tools/picktool.h"
#include "keyframedparams.h"

const int MAX_RECENT_WORKINGSET = 9;

MainWindow::MainWindow(TabletCanvas* canvas) : m_canvas(canvas), m_projectDialog(nullptr), m_preferenceDialog(nullptr) {
    setCentralWidget(m_canvas);
    setUnifiedTitleAndToolBarOnMac(true);
    setTabShape(QTabWidget::Rounded);
    setStatusBar(new QStatusBar());
    setWindowState(Qt::WindowMaximized);

    m_editor = new Editor(this);
    m_editor->init(canvas);

    statusBar()->addPermanentWidget(new QLabel("Zoom: 100%"));
    connect(m_editor, &Editor::updateStatusBar, statusBar(), &QStatusBar::showMessage);

    m_canvas->setEditor(m_editor);
    m_canvas->setFocusPolicy(Qt::ClickFocus);
    m_canvas->updateCursor();

    m_timeLine = new TimeLine(m_editor, this);
    m_timeLine->setFocusPolicy(Qt::NoFocus);
    setDockOptions(QMainWindow::AllowTabbedDocks);
    addDockWidget(Qt::BottomDockWidgetArea, m_timeLine);

    m_historyDock = new QDockWidget(this);
    m_undoView = new QUndoView(m_editor->undoStack());
    m_undoView->setCleanIcon(m_editor->style()->getIcon("save"));
    m_undoView->setEmptyLabel("New project");
    m_historyDock->setWidget(m_undoView);
    m_historyDock->setObjectName("History");
    m_historyDock->setWindowTitle("Undo History");
    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);

    m_colorBox = new ColorBox(this);
    m_colorBox->setToolTip(tr("color palette:<br>use <b>(C)</b><br>toggle at cursor"));
    m_colorBox->setObjectName("ColorWheel");
    addDockWidget(Qt::RightDockWidgetArea, m_colorBox);
    connect(m_colorBox, &ColorBox::colorChanged, m_editor->color(), &ColorManager::setColor);
    connect(m_editor->color(), &ColorManager::colorChanged, m_colorBox, &ColorBox::setColor);
    connect(m_colorBox, &ColorBox::colorChanged, this, &MainWindow::updateColorIcon);

    makeTimeLineConnections();

    m_editor->view()->resetView();

    m_onionSkinsDock = new OnionSkinsDocker(this, m_editor);
    addDockWidget(Qt::RightDockWidgetArea, m_onionSkinsDock);

    m_groupsWidget = new GroupsWidget(m_editor, this);
    makeGroupsWidgetConnections();
    m_groupsWidget->setVisible(false);

    createMenus();
    createToolBar();
    readSettings();

    m_fileManager = new FileManager(this);
    m_fileManager->createWorkingDir();
    setWindowTitle("[*]" + m_fileManager->fileName() + " - Frite");
    updateTitleSaveState(false);
    setAcceptDrops(true);

    m_editor->layers()->newLayer();
    m_editor->scrubTo(0);

    m_editor->tools()->setTool(Tool::Select);
}

MainWindow::~MainWindow() { 
    m_fileManager->deleteWorkingDir();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) { event->acceptProposedAction(); }

void MainWindow::dragMoveEvent(QDragMoveEvent* event) { event->acceptProposedAction(); }

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event) { event->accept(); }

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        QString path = urlList.first().toLocalFile();

        if (openProject(path)) event->acceptProposedAction();
    }
}

void MainWindow::setPreferences() {
    if (!m_preferenceDialog) {
        m_preferenceDialog = new PreferencesDialog(this);
        connect(m_preferenceDialog, SIGNAL(frameSizeChanged(int)), m_timeLine, SIGNAL(frameSizeChange(int)));
        connect(m_preferenceDialog, SIGNAL(fontSizeChanged(int)), m_timeLine, SIGNAL(fontSizeChange(int)));
    }
    m_preferenceDialog->exec();
}

void MainWindow::setProjectProperties() {
    if (!m_projectDialog)
        m_projectDialog = new ProjectPropertiesDialog(this, m_canvas->canvasRect().width(), m_canvas->canvasRect().height());
    if (m_projectDialog->exec() == QDialog::Accepted) m_canvas->setCanvasRect(m_projectDialog->getWidth(), m_projectDialog->getHeight());
}

void MainWindow::setToolValuator(QAction* action) { m_editor->tools()->setTool(action->data().value<Tool::ToolType>()); }

void MainWindow::setAlphaValuator(QAction* action) { m_canvas->setAlphaChannelValuator(action->data().value<TabletCanvas::Valuator>()); }

void MainWindow::setLineWidthValuator(QAction* action) { m_canvas->setLineWidthType(action->data().value<TabletCanvas::Valuator>()); }

void MainWindow::setSaturationValuator(QAction* action) {
    m_canvas->setColorSaturationValuator(action->data().value<TabletCanvas::Valuator>());
}

void MainWindow::updateTitleSaveState(bool saved) {
    setWindowModified(!saved);
    m_saveAction->setEnabled(!saved);
}

bool MainWindow::maybeSave() {
    if (isWindowModified()) {
        int ret = QMessageBox::warning(this, tr("Warning"), tr("This project has been modified.\n Do you want to save your changes?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (ret == QMessageBox::Yes) {
            saveAs();
            return true;
        } else if (ret == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

// TODO: reset filename
void MainWindow::newProject() {
    if (maybeSave()) {
        setProjectProperties();
        m_editor->layers()->clear();
        m_editor->layers()->newLayer();
        m_editor->scrubTo(0);
        m_undoView->setEmptyLabel("New project");
        m_editor->undoStack()->clear();
        m_fileManager->createWorkingDir();
        m_fileManager->resetFileName();
        setWindowTitle("[*]" + m_fileManager->fileName() + " - Frite");
        updateTitleSaveState(false);
    }
}

void MainWindow::save() {
    if (!m_fileManager->filePath().isEmpty()) {
        saveProject(m_fileManager->filePath());
    } else {
        saveAs();
    }
}

void MainWindow::saveAs() {
    QSettings settings("manao", "Frite");
    QString lastOpenPath = settings.value("LastFilePath", QDir::currentPath()).toString();
    QString path = lastOpenPath + "/" + m_fileManager->fileName() + ".xml";
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Project"), path);
    if (filename.isEmpty()) return;

    if (saveProject(filename)) {
        settings.setValue("LastFilePath", QVariant(QFileInfo(filename).absolutePath()));
        addToRecentFiles(filename);
    }
}

void MainWindow::load() {
    QSettings settings("manao", "Frite");
    QString lastOpenPath = settings.value("LastFilePath", QDir::currentPath()).toString();
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Project"), lastOpenPath, "*.fries *.xml");
    if (openProject(filename)) {
        settings.setValue("LastFilePath", QVariant(QFileInfo(filename).absolutePath()));
        addToRecentFiles(filename);
    }
}

bool MainWindow::openProject(const QString& filename) {
    if (m_fileManager->load(filename, m_editor, m_dials_and_knobs)) {
        statusBar()->showMessage("Project loaded", 3000);
        setWindowTitle("[*]" + m_fileManager->fileName() + " - Frite");
        updateTitleSaveState(true);
        m_undoView->setEmptyLabel("Open project");
        m_editor->undoStack()->clear();
        m_editor->scrubTo(0);
        return true;
    }
    return false;
}

bool MainWindow::saveProject(const QString& filename) {
    if (m_fileManager->save(filename, m_editor, m_dials_and_knobs)) {
        statusBar()->showMessage("Project saved", 3000);
        setWindowTitle("[*]" + m_fileManager->fileName() + " - Frite");
        updateTitleSaveState(true);
        m_editor->undoStack()->beginMacro("Save project");
        m_editor->undoStack()->endMacro();
        m_editor->undoStack()->setClean();
        return true;
    }
    return false;
}

void MainWindow::addToRecentFiles(const QString& filename) {
    QSettings settings("manao", "Frite");
    QStringList files = settings.value("recentFileList").toStringList();
    files.removeAll(filename);
    files.prepend(filename);
    while (files.size() > MAX_RECENT_WORKINGSET) files.removeLast();
    settings.setValue("recentFileList", files);
    updateRecentFileActions();
}

void MainWindow::openRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) openProject(action->data().toString());
}

void MainWindow::exportImageSequence() {
    // Path
    QSettings settings("manao", "Frite");
    QString strInitPath = settings.value("lastExportPath", QDir::currentPath() + "/untitled").toString();

    QFileInfo info(strInitPath);
    QString strFilePath = QFileDialog::getSaveFileName(this, tr("Export Sequence"), strInitPath,
                                                       "Bitmap (*.png *.jpg *.jpeg *.tif *.tiff *.bmp);;SVG (*.svg)");
    if (strFilePath.isEmpty()) return;
    settings.setValue("lastExportPath", strFilePath);

    // Export
    QSize exportSize = QSize(m_canvas->canvasRect().width(), m_canvas->canvasRect().height());
    // if (exportSize.width() < 3840 || exportSize.height() < 2160) exportSize.scale(1920, 1080, Qt::KeepAspectRatioByExpanding);
    if (exportSize.width() < 3840 || exportSize.height() < 2160) exportSize.scale(3840, 2160, Qt::KeepAspectRatio);
    m_editor->exportFrames(strFilePath, exportSize);
    statusBar()->showMessage("Sequence exported", 3000);
}

void MainWindow::about() { QMessageBox::about(this, tr("About Frite"), tr("2D animation software")); }

void MainWindow::createMenus() {
#ifdef Q_OS_MAC
    QApplication::instance()->setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif

    StyleManager* styleManager = m_editor->style();
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* newAction = fileMenu->addAction(styleManager->getIcon("new"), tr("&New..."), this, &MainWindow::newProject, QKeySequence::New);
    QAction* openAction = fileMenu->addAction(styleManager->getIcon("open"), tr("&Open..."), this, &MainWindow::load, QKeySequence::Open);

    m_recent_workingset_action.resize(MAX_RECENT_WORKINGSET);
    for (int i = 0; i < MAX_RECENT_WORKINGSET; ++i) {
        m_recent_workingset_action[i] = new QAction(this);
        m_recent_workingset_action[i]->setVisible(false);
        connect(m_recent_workingset_action[i], SIGNAL(triggered()), this, SLOT(openRecentFile()));
    }
    QMenu* recentMenu = new QMenu(tr("Recent..."), this);
    recentMenu->setIcon(styleManager->getIcon("recent"));
    fileMenu->addMenu(recentMenu);
    for (int i = 0; i < MAX_RECENT_WORKINGSET; ++i) {
        m_recent_workingset_action[i]->setShortcut(QKeySequence(tr((QString("Alt+Shift+%1").arg(i + 1)).toStdString().c_str())));
        recentMenu->addAction(m_recent_workingset_action[i]);
    }
    updateRecentFileActions();
    fileMenu->addSeparator();
    m_saveAction = fileMenu->addAction(styleManager->getIcon("save"), tr("&Save..."), this, &MainWindow::save, QKeySequence::Save);
    fileMenu->addAction(styleManager->getIcon("save-as"), tr("&Save As..."), this, &MainWindow::saveAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    // fileMenu->addAction(styleManager->getIcon("import"), tr("&Import..."), this, &MainWindow::importImageSequence);
    fileMenu->addAction(styleManager->getIcon("export"), tr("&Export..."), this, &MainWindow::exportImageSequence);
    fileMenu->addSeparator();
    QAction* propAction =
        fileMenu->addAction(styleManager->getIcon("configure"), tr("&Properties"), this, &MainWindow::setProjectProperties);
    fileMenu->addSeparator();
    fileMenu->addAction(styleManager->getIcon("exit"), tr("E&xit"), this, &MainWindow::close, QKeySequence::Quit);

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    QAction* undoAction = m_editor->undoStack()->createUndoAction(this, tr("&Undo"));
    undoAction->setShortcuts(QKeySequence::Undo);
    undoAction->setIcon(styleManager->getIcon("undo"));
    QAction* redoAction = m_editor->undoStack()->createRedoAction(this, tr("&Redo"));
    redoAction->setShortcuts(QKeySequence::Redo);
    redoAction->setIcon(styleManager->getIcon("redo"));
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(styleManager->getIcon("cut"), tr("&Cut"), m_editor, &Editor::cut, QKeySequence::Cut);
    editMenu->addAction(styleManager->getIcon("copy"), tr("C&opy"), m_editor, &Editor::copy, QKeySequence::Copy);
    editMenu->addAction(styleManager->getIcon("paste"), tr("&Paste"), m_editor, &Editor::paste, QKeySequence::Paste);
    editMenu->addSeparator();
    editMenu->addAction(styleManager->getIcon("selectAll"), tr("&Select All"), m_canvas, &TabletCanvas::selectAll, QKeySequence::SelectAll);
    editMenu->addAction(styleManager->getIcon("deselectAll"), tr("&Deselect"), m_editor, &Editor::deselectAll, QKeySequence(tr("Escape")));
    editMenu->addAction(styleManager->getIcon("deselectAll"), tr("Deselect in all layers"), m_editor, &Editor::deselectInAllLayers, QKeySequence(tr("Shift+Escape")));
    editMenu->addSeparator();
    editMenu->addAction(styleManager->getIcon("configure"), tr("Increase keyframe exposure"), m_editor, &Editor::increaseCurrentKeyExposure, QKeySequence(Qt::Key_Plus));
    editMenu->addAction(styleManager->getIcon("configure"), tr("Decrease keyframe exposure"), m_editor, &Editor::decreaseCurrentKeyExposure, QKeySequence(Qt::Key_Minus));
    editMenu->addSeparator();
    editMenu->addAction(styleManager->getIcon("configure"), "Preferences", this, &MainWindow::setPreferences, QKeySequence::Preferences);

    QMenu* actionsMenu = menuBar()->addMenu("&Actions");
    actionsMenu->addAction(styleManager->getIcon("onion"), tr("Onion skin"), m_editor, &Editor::toggleOnionSkin, QKeySequence(tr("O")));
    actionsMenu->addSection("Keyframe");
    actionsMenu->addAction(styleManager->getIcon("delete"), tr("Clear drawing"), m_editor, &Editor::clearCurrentFrame, QKeySequence(tr("K")));
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("New group"), m_editor, &Editor::drawInNewGroup, QKeySequence(tr("Return")));
    actionsMenu->addAction(styleManager->getIcon("delete"), tr("Delete group"), m_editor, &Editor::deleteGroup, QKeySequence::Delete);
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Split groups"), m_editor, &Editor::splitGridIntoSingleConnectedComponent,QKeySequence(tr("Ctrl+Return")));
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Convert inbetween to breakdown"), m_editor, &Editor::convertToBreakdown, QKeySequence(tr("B")));
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Copy selected groups to the next keyframe"), this, [&]() { m_editor->copyGroupToNextKeyFrame(false); }, QKeySequence(tr("C")));
    actionsMenu->addSection("Matching");
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Matching"), this, [&]() { m_editor->registerFromRestPosition(); }, QKeySequence(tr("M")));
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Matching from current state"), this, [&]() { m_editor->registerFromTargetPosition(); }, QKeySequence(tr("Ctrl+M")));
    // actionsMenu->addAction(styleManager->getIcon("fit"), tr("Regularize"), m_editor, &Editor::regularizeLattice, QKeySequence(tr("R")));
    actionsMenu->addAction(styleManager->getIcon("delete"), tr("Reset matching"), m_editor, &Editor::clearARAPWarp, QKeySequence(tr("Ctrl+K")));
    actionsMenu->addSection("Interpolation");
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Toggle cross-fade"), m_editor, &Editor::toggleCrossFade, QKeySequence(tr("Shift+C")));
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Fade-out"), m_editor, &Editor::makeGroupFadeOut, QKeySequence(tr("Shift+Q")));
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Smooth trajectory (in time)"), m_editor, &Editor::makeTrajectoryC1Continuous);
    actionsMenu->addSection("Misc. & Debug");
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Recompute inbetweens interval"), m_editor, &Editor::makeInbetweensDirty);
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Force clear cross-fade"), m_editor, &Editor::clearCrossFade);
    actionsMenu->addAction(styleManager->getIcon("fit"), tr("Debug report"), m_editor, &Editor::debugReport, QKeySequence(tr("Shift+I")));

    QToolBar* toolBar = new QToolBar("Menu", this);
    toolBar->setObjectName(QStringLiteral("menuBar"));
    toolBar->addAction(newAction);
    toolBar->addAction(openAction);
    toolBar->addAction(m_saveAction);
    toolBar->addAction(propAction);
    toolBar->addAction(undoAction);
    toolBar->addAction(redoAction);
    toolBar->addSeparator();
    QAction* timelineAction = m_timeLine->toggleViewAction();
    timelineAction->setIcon(styleManager->getIcon("timeline"));
    toolBar->addAction(timelineAction);
    QAction* onionAction = m_onionSkinsDock->toggleViewAction();
    onionAction->setIcon(styleManager->getIcon("onion"));
    toolBar->addAction(onionAction);
    addToolBar(Qt::TopToolBarArea, toolBar);
    QAction* groupsWidgetAction = m_groupsWidget->toggleViewAction();
    toolBar->addAction(groupsWidgetAction);

    QMenu* tabletMenu = menuBar()->addMenu(tr("&Tablet"));
    QMenu* lineWidthMenu = tabletMenu->addMenu(tr("&Line Width"));

    QAction* lineWidthPressureAction = lineWidthMenu->addAction(tr("&Pressure"));
    lineWidthPressureAction->setData(TabletCanvas::PressureValuator);
    lineWidthPressureAction->setCheckable(true);
    lineWidthPressureAction->setChecked(true);

    QAction* lineWidthTiltAction = lineWidthMenu->addAction(tr("&Tilt"));
    lineWidthTiltAction->setData(TabletCanvas::TiltValuator);
    lineWidthTiltAction->setCheckable(true);

    QAction* lineWidthFixedAction = lineWidthMenu->addAction(tr("&Fixed"));
    lineWidthFixedAction->setData(TabletCanvas::NoValuator);
    lineWidthFixedAction->setCheckable(true);

    QActionGroup* lineWidthGroup = new QActionGroup(this);
    lineWidthGroup->addAction(lineWidthPressureAction);
    lineWidthGroup->addAction(lineWidthTiltAction);
    lineWidthGroup->addAction(lineWidthFixedAction);
    connect(lineWidthGroup, &QActionGroup::triggered, this, &MainWindow::setLineWidthValuator);

    QMenu* alphaChannelMenu = tabletMenu->addMenu(tr("&Alpha Channel"));
    QAction* alphaChannelPressureAction = alphaChannelMenu->addAction(tr("&Pressure"));
    alphaChannelPressureAction->setData(TabletCanvas::PressureValuator);
    alphaChannelPressureAction->setCheckable(true);

    QAction* alphaChannelTangentialPressureAction = alphaChannelMenu->addAction(tr("T&angential Pressure"));
    alphaChannelTangentialPressureAction->setData(TabletCanvas::TangentialPressureValuator);
    alphaChannelTangentialPressureAction->setCheckable(true);
    alphaChannelTangentialPressureAction->setChecked(true);

    QAction* alphaChannelTiltAction = alphaChannelMenu->addAction(tr("&Tilt"));
    alphaChannelTiltAction->setData(TabletCanvas::TiltValuator);
    alphaChannelTiltAction->setCheckable(true);

    QAction* noAlphaChannelAction = alphaChannelMenu->addAction(tr("No Alpha Channel"));
    noAlphaChannelAction->setData(TabletCanvas::NoValuator);
    noAlphaChannelAction->setCheckable(true);

    QActionGroup* alphaChannelGroup = new QActionGroup(this);
    alphaChannelGroup->addAction(alphaChannelPressureAction);
    alphaChannelGroup->addAction(alphaChannelTangentialPressureAction);
    alphaChannelGroup->addAction(alphaChannelTiltAction);
    alphaChannelGroup->addAction(noAlphaChannelAction);
    connect(alphaChannelGroup, &QActionGroup::triggered, this, &MainWindow::setAlphaValuator);

    QMenu* colorSaturationMenu = tabletMenu->addMenu(tr("&Color Saturation"));

    QAction* colorSaturationVTiltAction = colorSaturationMenu->addAction(tr("&Vertical Tilt"));
    colorSaturationVTiltAction->setData(TabletCanvas::VTiltValuator);
    colorSaturationVTiltAction->setCheckable(true);

    QAction* colorSaturationHTiltAction = colorSaturationMenu->addAction(tr("&Horizontal Tilt"));
    colorSaturationHTiltAction->setData(TabletCanvas::HTiltValuator);
    colorSaturationHTiltAction->setCheckable(true);

    QAction* colorSaturationPressureAction = colorSaturationMenu->addAction(tr("&Pressure"));
    colorSaturationPressureAction->setData(TabletCanvas::PressureValuator);
    colorSaturationPressureAction->setCheckable(true);

    QAction* noColorSaturationAction = colorSaturationMenu->addAction(tr("&No Color Saturation"));
    noColorSaturationAction->setData(TabletCanvas::NoValuator);
    noColorSaturationAction->setCheckable(true);
    noColorSaturationAction->setChecked(true);

    QActionGroup* colorSaturationGroup = new QActionGroup(this);
    colorSaturationGroup->addAction(colorSaturationVTiltAction);
    colorSaturationGroup->addAction(colorSaturationHTiltAction);
    colorSaturationGroup->addAction(colorSaturationPressureAction);
    colorSaturationGroup->addAction(noColorSaturationAction);
    connect(colorSaturationGroup, &QActionGroup::triggered, this, &MainWindow::setSaturationValuator);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(styleManager->getIcon("zoom-in"), tr("Zoom In"), m_editor->view(), &ViewManager::scaleUp, QKeySequence(tr(">")));
    viewMenu->addAction(styleManager->getIcon("zoom-out"), tr("Zoom Out"), m_editor->view(), &ViewManager::scaleDown,
                        QKeySequence(tr("<")));
    viewMenu->addAction(styleManager->getIcon("zoom-original"), tr("1:1"), m_editor->view(), &ViewManager::resetScale,
                        QKeySequence(tr("Ctrl+0")));
    viewMenu->addSeparator();
    viewMenu->addAction(styleManager->getIcon("rotate-left"), tr("Rotate Counter Clockwise"), m_editor->view(),
                        &ViewManager::rotateCounterClockwise, QKeySequence(tr("Shift+PgUp")));
    viewMenu->addAction(styleManager->getIcon("rotate-right"), tr("Rotate Clockwise"), m_editor->view(), &ViewManager::rotateClockwise,
                        QKeySequence(tr("Shift+PgDown")));
    viewMenu->addAction(styleManager->getIcon("rotation-reset"), tr("Reset Rotation"), m_editor->view(), &ViewManager::resetRotate,
                        QKeySequence(tr("Shift+Home")));
    viewMenu->addSeparator();
    viewMenu->addAction(styleManager->getIcon("mirror-x"), tr("Flip Horizontally"), m_editor->view(), &ViewManager::flipHorizontal,
                        QKeySequence(tr("[")));
    viewMenu->addAction(styleManager->getIcon("mirror-y"), tr("Flip Vertically"), m_editor->view(), &ViewManager::flipVertical,
                        QKeySequence(tr("]")));

    m_windowsMenu = menuBar()->addMenu("&Windows");
    m_windowsMenu->addAction(toolBar->toggleViewAction());
    m_windowsMenu->addAction(m_timeLine->toggleViewAction());
    m_windowsMenu->addAction(m_historyDock->toggleViewAction());
    m_windowsMenu->addAction(m_onionSkinsDock->toggleViewAction());
    m_windowsMenu->addAction(m_colorBox->toggleViewAction());
    m_windowsMenu->addAction(m_groupsWidget->toggleViewAction());
    QStringList separate_windows("Options");
    QMetaEnum e = QMetaEnum::fromType<Tool::ToolType>();
    for (int i = 0; i < e.keyCount(); ++i) {
        separate_windows.append(e.valueToKey(e.value(i)));
    }
    m_dials_and_knobs = new DialsAndKnobs(this, m_windowsMenu, separate_windows);
    m_dials_and_knobs->toggleCategory(m_editor->tools()->currentTool());

    connect(m_editor->tools(), SIGNAL(toolChanged(Tool *)), m_dials_and_knobs, SLOT(toggleCategory(Tool *)));

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(tr("A&bout"), this, &MainWindow::about);
}

void MainWindow::createToolBar() {
    StyleManager* styleManager = m_editor->style();
    QToolBar* toolBar = new QToolBar("Tools", this);
    toolBar->setObjectName(QStringLiteral("toolBar"));
    m_colorAction = toolBar->addAction(tr("Color"));
    m_colorAction->setCheckable(false);
    updateColorIcon(m_editor->color()->frontColor());

    // DRAWING AND EDITING TOOLS
    QAction* drawToolAction = toolBar->addAction(styleManager->getIcon("pen"), tr("Pen"));
    drawToolAction->setData(Tool::Pen);
    drawToolAction->setShortcut(tr("P"));
    drawToolAction->setCheckable(true);
    QToolButton *drawButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(drawToolAction));
    drawButton->setPopupMode(QToolButton::InstantPopup);
    QAction *drawEndKeyframeToolAction = new QAction(styleManager->getIcon("pen"), "Draw in end keyframe");
    drawEndKeyframeToolAction->setData(Tool::DrawEndKeyframe);
    drawEndKeyframeToolAction->setShortcut(tr("Shift+P"));
    drawEndKeyframeToolAction->setCheckable(true);
    drawButton->addAction(drawEndKeyframeToolAction);
    QAction* eraserToolAction = toolBar->addAction(styleManager->getIcon("eraser"), tr("Eraser"));
    eraserToolAction->setData(Tool::Eraser);
    eraserToolAction->setShortcut(tr("E"));
    eraserToolAction->setCheckable(true);
    QAction* localMaskToolAction = toolBar->addAction(styleManager->getIcon("eraser"), tr("Visibility"));
    localMaskToolAction->setData(Tool::LocalMask);
    localMaskToolAction->setShortcut(tr("V"));
    localMaskToolAction->setCheckable(true);
    QAction* handToolAction = toolBar->addAction(styleManager->getIcon("move"),tr("Pan"));
    handToolAction->setData(Tool::Hand);
    handToolAction->setShortcut(tr("H"));
    handToolAction->setCheckable(true);

    toolBar->addSeparator();

    QAction* selectToolAction = toolBar->addAction(styleManager->getIcon("select"), tr("Select group"));
    selectToolAction->setData(Tool::Select);
    selectToolAction->setShortcut(tr("S"));
    selectToolAction->setCheckable(true);
    QToolButton *selectButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(selectToolAction));
    selectButton->setPopupMode(QToolButton::InstantPopup);
    QAction* lassoToolAction = new QAction(styleManager->getIcon("lasso"), tr("Create group"));
    lassoToolAction->setData(Tool::Lasso);
    lassoToolAction->setCheckable(true);
    lassoToolAction->setShortcut(tr("G"));
    selectButton->addAction(lassoToolAction);

    QAction* pickStrokesAction = new QAction(styleManager->getIcon("lasso"), tr("Copy strokes from onion skin"));
    pickStrokesAction->setData(Tool::CopyStrokes);
    pickStrokesAction->setCheckable(true);
    // pickStrokesAction->setShortcut(tr("G"));
    selectButton->addAction(pickStrokesAction);

    toolBar->addSeparator();

    // MATCHING TOOLS
    QAction* directMatchingToolAction = toolBar->addAction(styleManager->getIcon("warp"), tr("Direct matching"));
    directMatchingToolAction->setData(Tool::DirectMatching);
    directMatchingToolAction->setShortcut(tr("Shift+M"));
    directMatchingToolAction->setCheckable(true);
    QToolButton *matchingButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(directMatchingToolAction));
    matchingButton->setPopupMode(QToolButton::InstantPopup);
    QAction* rigidDeformToolAction = new QAction(styleManager->getIcon("warp"), tr("Rigid matching"));
    rigidDeformToolAction->setData(Tool::RigidDeform);
    rigidDeformToolAction->setShortcut(tr("Ctrl+W"));
    rigidDeformToolAction->setCheckable(true);
    matchingButton->addAction(rigidDeformToolAction);
    QAction* warpToolAction = toolBar->addAction(styleManager->getIcon("warp"), tr("Non-rigid matching"));
    warpToolAction->setData(Tool::Warp);
    warpToolAction->setShortcut(tr("W"));
    warpToolAction->setCheckable(true);
    matchingButton->addAction(warpToolAction);
    QAction* strokeDeformToolAction = new QAction(styleManager->getIcon("pen"), tr("Stroke-guided matching"));
    strokeDeformToolAction->setData(Tool::StrokeDeform);
    strokeDeformToolAction->setCheckable(true);
    matchingButton->addAction(strokeDeformToolAction);
    QAction* fillGridToolAction = new QAction(styleManager->getIcon("warp"), tr("Edit grid"));
    fillGridToolAction->setData(Tool::FillGrid);
    fillGridToolAction->setCheckable(true);
    matchingButton->addAction(fillGridToolAction);
    QAction* registrationLassoToolAction = new QAction(styleManager->getIcon("lasso"), tr("Select matching target"));
    registrationLassoToolAction->setData(Tool::RegistrationLasso);
    registrationLassoToolAction->setCheckable(true);
    matchingButton->addAction(registrationLassoToolAction);

    toolBar->addSeparator();

    // SPACING TOOLS
    QAction* moveFramesToolAction = toolBar->addAction(styleManager->getIcon("spacing"), tr("Move frames"));
    moveFramesToolAction->setData(Tool::MoveFrames);
    moveFramesToolAction->setShortcut(tr("I"));
    moveFramesToolAction->setCheckable(true);
    QToolButton *spacingButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(moveFramesToolAction));
    spacingButton->setPopupMode(QToolButton::InstantPopup);
    QAction* halvesToolAction = new QAction(styleManager->getIcon("halves"), tr("Halves spacing mode"));
    halvesToolAction->setData(Tool::Halves);
    halvesToolAction->setShortcut(tr("Ctrl+I"));
    halvesToolAction->setCheckable(true);
    spacingButton->addAction(halvesToolAction);
    QAction* spacingProxyToolAction = new QAction(styleManager->getIcon("spacing"), tr("Proxy spacing"));
    spacingProxyToolAction->setData(Tool::ProxySpacing);
    // spacingProxyToolAction->setShortcut(tr("Ctrl+I"));
    spacingProxyToolAction->setCheckable(true);
    spacingButton->addAction(spacingProxyToolAction);
    QAction* movePartialsToolAction = new QAction(styleManager->getIcon("spacing"), tr("Move partials"));
    movePartialsToolAction->setData(Tool::MovePartials);
    movePartialsToolAction->setCheckable(true);
    spacingButton->addAction(movePartialsToolAction);

    toolBar->addSeparator();

    // TRAJECTORY TOOLS
    QAction* trajectoryToolAction = toolBar->addAction(styleManager->getIcon("trajectory"), tr("Select trajectory"));
    trajectoryToolAction->setData(Tool::Traj);
    trajectoryToolAction->setShortcut(tr("T"));
    trajectoryToolAction->setCheckable(true);
    QToolButton *trajButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(trajectoryToolAction));
    trajButton->setPopupMode(QToolButton::InstantPopup);
    QAction* drawTrajectoryToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Draw trajectory"));
    drawTrajectoryToolAction->setData(Tool::DrawTraj);
    drawTrajectoryToolAction->setShortcut(tr("Shift+T"));
    drawTrajectoryToolAction->setCheckable(true);
    trajButton->addAction(drawTrajectoryToolAction);
    QAction* tangentToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Edit tangents"));
    tangentToolAction->setData(Tool::TrajTangent);
    tangentToolAction->setShortcut(tr("Ctrl+T"));
    tangentToolAction->setCheckable(true);
    trajButton->addAction(tangentToolAction);

    toolBar->addSeparator();

    // MASK TOOL
    QAction* groupOrderingToolAction = toolBar->addAction(styleManager->getIcon("ordering"), tr("Group ordering"));
    groupOrderingToolAction->setData(Tool::GroupOrdering);
    groupOrderingToolAction->setCheckable(true);
    groupOrderingToolAction->setShortcut(tr("Ctrl+G"));
    QToolButton *maskButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(groupOrderingToolAction));
    maskButton->setPopupMode(QToolButton::InstantPopup);
    QAction* maskPenToolAction = new QAction(styleManager->getIcon("pen"), tr("Mask pen"));
    maskPenToolAction->setData(Tool::MaskPen);
    // maskPenToolAction->setShortcut(tr("P"));
    maskPenToolAction->setCheckable(true);
    maskButton->addAction(maskPenToolAction);
    // QAction* visibilityToolAction = new QAction(styleManager->getIcon("pen"), tr("Visibility"));
    // visibilityToolAction->setData(Tool::Visibility);
    // visibilityToolAction->setCheckable(true);
    // maskButton->addAction(visibilityToolAction);

    toolBar->addSeparator();

    // DEBUG TOOL
    QAction* debugToolAction = toolBar->addAction(styleManager->getIcon("fit"), tr("Debug"));
    debugToolAction->setData(Tool::Debug);
    // debugToolAction->setShortcut(tr("D"));
    debugToolAction->setCheckable(true);

    // PIVOT TOOLS
    QAction* pivotCreationToolAction = toolBar->addAction(styleManager->getIcon("trajectory"), tr("Create pivot"));
    pivotCreationToolAction->setData(Tool::PivotCreation);
    // pivotCreationToolAction->setShortcut(tr(""));
    pivotCreationToolAction->setCheckable(true);
    QToolButton *pivotButton = dynamic_cast<QToolButton*>(toolBar->widgetForAction(pivotCreationToolAction));
    pivotButton->setPopupMode(QToolButton::InstantPopup);

    QAction * pivotEditToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Edit pivot"));
    pivotEditToolAction->setData(Tool::PivotEdit);
    pivotButton->addAction(pivotEditToolAction);
    QAction* pivotTangentToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Edit pivots tangents"));
    pivotTangentToolAction->setData(Tool::PivotTangent);
    pivotTangentToolAction->setCheckable(true);
    pivotButton->addAction(pivotTangentToolAction);
    QAction * pivotRotationToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Edit pivot rotation"));
    pivotRotationToolAction->setData(Tool::PivotRotation);
    pivotRotationToolAction->setCheckable(true);
    pivotButton->addAction(pivotRotationToolAction);    
    QAction* pivotScalingToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Edit pivot scaling"));
    pivotScalingToolAction->setData(Tool::PivotScaling);
    pivotScalingToolAction->setCheckable(true);
    pivotButton->addAction(pivotScalingToolAction);    
    QAction* pivotTranslationToolAction = new QAction(styleManager->getIcon("trajectory"), tr("Layer translation"));
    pivotTranslationToolAction->setData(Tool::PivotTranslation);
    pivotTranslationToolAction->setCheckable(true);
    pivotButton->addAction(pivotTranslationToolAction);

    m_toolGroup = new QActionGroup(this);
    m_toolGroup->addAction(drawToolAction);
    m_toolGroup->addAction(drawEndKeyframeToolAction);
    m_toolGroup->addAction(eraserToolAction);
    m_toolGroup->addAction(handToolAction);
    m_toolGroup->addAction(selectToolAction);
    m_toolGroup->addAction(lassoToolAction);
    m_toolGroup->addAction(directMatchingToolAction);
    m_toolGroup->addAction(rigidDeformToolAction);
    m_toolGroup->addAction(warpToolAction);
    m_toolGroup->addAction(strokeDeformToolAction);
    m_toolGroup->addAction(registrationLassoToolAction);
    m_toolGroup->addAction(trajectoryToolAction);
    m_toolGroup->addAction(drawTrajectoryToolAction);
    m_toolGroup->addAction(tangentToolAction);
    m_toolGroup->addAction(fillGridToolAction);
    m_toolGroup->addAction(pivotCreationToolAction);
    m_toolGroup->addAction(pivotEditToolAction);
    m_toolGroup->addAction(pivotTangentToolAction);
    m_toolGroup->addAction(pivotRotationToolAction);
    m_toolGroup->addAction(pivotScalingToolAction);
    m_toolGroup->addAction(pivotTranslationToolAction);
    m_toolGroup->addAction(moveFramesToolAction);
    m_toolGroup->addAction(halvesToolAction);
    m_toolGroup->addAction(maskPenToolAction);
    m_toolGroup->addAction(groupOrderingToolAction);
    m_toolGroup->addAction(debugToolAction);
    m_toolGroup->addAction(spacingProxyToolAction);
    m_toolGroup->addAction(movePartialsToolAction);
    m_toolGroup->addAction(localMaskToolAction);
    m_toolGroup->addAction(pickStrokesAction);
    // m_toolGroup->addAction(visibilityToolAction);
    drawToolAction->setChecked(true);

    addToolBar(Qt::TopToolBarArea, toolBar);

    m_windowsMenu->addAction(toolBar->toggleViewAction());

    // Connect the ToolManager signals with their corresponding QAction
    connect(m_toolGroup, &QActionGroup::triggered, this, &MainWindow::setToolValuator);
    connect(m_editor->tools(), &ToolsManager::penSelected, this, [&]{ m_toolGroup->actions().at(0); });
    connect(m_editor->tools(), &ToolsManager::drawEndKeyframeSelected, this, [&]{ m_toolGroup->actions().at(1); });
    connect(m_editor->tools(), &ToolsManager::eraserSelected, this, [&]{ m_toolGroup->actions().at(2); });
    connect(m_editor->tools(), &ToolsManager::handSelected, this, [&]{ m_toolGroup->actions().at(3); });
    connect(m_editor->tools(), &ToolsManager::selectSelected, this, [&]{ m_toolGroup->actions().at(4); });
    connect(m_editor->tools(), &ToolsManager::lassoSelected, this, [&]{ m_toolGroup->actions().at(5); });
    connect(m_editor->tools(), &ToolsManager::directMatchingSelected, this, [&]{ m_toolGroup->actions().at(6); });
    connect(m_editor->tools(), &ToolsManager::deformSelected, this, [&]{ m_toolGroup->actions().at(7); });
    connect(m_editor->tools(), &ToolsManager::warpSelected, this, [&]{ m_toolGroup->actions().at(8); });
    connect(m_editor->tools(), &ToolsManager::strokeDeformSelected, this, [&]{ m_toolGroup->actions().at(9); });
    connect(m_editor->tools(), &ToolsManager::registrationLassoSelected, this, [&]{ m_toolGroup->actions().at(10); });
    connect(m_editor->tools(), &ToolsManager::trajectorySelected, this, [&]{ m_toolGroup->actions().at(11); });
    connect(m_editor->tools(), &ToolsManager::drawTrajectorySelected, this, [&]{ m_toolGroup->actions().at(12); });
    connect(m_editor->tools(), &ToolsManager::tangentSelected, this, [&]{ m_toolGroup->actions().at(13); });
    connect(m_editor->tools(), &ToolsManager::fillGridSelected, this, [&]{ m_toolGroup->actions().at(14); });
    connect(m_editor->tools(), &ToolsManager::pivotCreationSelected, this, [&]{ m_toolGroup->actions().at(15); });
    connect(m_editor->tools(), &ToolsManager::pivotEditSelected, this, [&]{ m_toolGroup->actions().at(16); });
    connect(m_editor->tools(), &ToolsManager::pivotTangentSelected, this, [&]{ m_toolGroup->actions().at(17); });
    connect(m_editor->tools(), &ToolsManager::pivotRotationSelected, this, [&]{ m_toolGroup->actions().at(18); });
    connect(m_editor->tools(), &ToolsManager::pivotScalingSelected, this, [&]{ m_toolGroup->actions().at(19); });
    connect(m_editor->tools(), &ToolsManager::pivotTranslationSelected, this, [&]{ m_toolGroup->actions().at(20); });
    connect(m_editor->tools(), &ToolsManager::moveFramesSelected, this, [&]{ m_toolGroup->actions().at(21); });
    connect(m_editor->tools(), &ToolsManager::halvesSelected, this, [&]{ m_toolGroup->actions().at(22); });
    connect(m_editor->tools(), &ToolsManager::maskPenSelected, this, [&]{ m_toolGroup->actions().at(23); });
    connect(m_editor->tools(), &ToolsManager::groupOrderingSelected, this, [&]{ m_toolGroup->actions().at(24); });
    connect(m_editor->tools(), &ToolsManager::debugSelected, this, [&]{ m_toolGroup->actions().at(25); });
    connect(m_editor->tools(), &ToolsManager::proxySpacingSelected, this, [&]{ m_toolGroup->actions().at(26); });
    connect(m_editor->tools(), &ToolsManager::movePartialsSelected, this, [&]{ m_toolGroup->actions().at(27); });
    connect(m_editor->tools(), &ToolsManager::localMaskSelected, this, [&]{ m_toolGroup->actions().at(28); });
    connect(m_editor->tools(), &ToolsManager::pickStrokesSelected, this, [&]{ m_toolGroup->actions().at(29); });
    // connect(m_editor->tools(), &ToolsManager::visibilitySelected, this, [&]{ m_toolGroup->actions().at(30); });
}

void MainWindow::updateColorIcon(const QColor& c) {
    QPixmap pixmap(24, 24);
    if (!pixmap.isNull()) {
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing, false);
        painter.setPen(QColor(100, 100, 100));
        painter.setBrush(c);
        painter.drawRect(1, 1, 20, 20);
    }
    m_colorAction->setIcon(QIcon(pixmap));
}

void MainWindow::readSettings() {
    QSettings settings("manao", "Frite");
    restoreGeometry(settings.value("WindowGeometry").toByteArray());
    restoreState(settings.value("WindowState").toByteArray());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) {
        event->ignore();
        return;
    }

    m_onionSkinsDock->saveSettings();

    QSettings settings("manao", "Frite");
    settings.setValue("WindowGeometry", saveGeometry());
    settings.setValue("WindowState", saveState());

    m_editor->undoStack()->clear();

    event->accept();
}

void MainWindow::updateRecentFileActions() {
    QSettings settings("manao", "Frite");
    QStringList files = settings.value("recentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), int(MAX_RECENT_WORKINGSET));

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 %2").arg(i + 1).arg(files[i]);
        m_recent_workingset_action[i]->setText(text);
        m_recent_workingset_action[i]->setData(files[i]);
        m_recent_workingset_action[i]->setVisible(true);
        m_recent_workingset_action[i]->setShortcut(QKeySequence(tr(((QString("Alt+Shift+%1").arg(i + 1))).toStdString().c_str())));
    }
    for (int j = numRecentFiles; j < MAX_RECENT_WORKINGSET; ++j) m_recent_workingset_action[j]->setVisible(false);
}

void MainWindow::updateZoomLabel() {
    QLabel *zoomLabel = qobject_cast<QLabel *>(statusBar()->children().back());
    if (zoomLabel != nullptr) {
        qreal zoom = m_editor->view()->scaling() * 100.;
        zoomLabel->setText(QString("Zoom: %0%1").arg(zoom, 0, 'f', 1).arg("%"));
        m_canvas->updateCursor();
        m_canvas->update();
    }
}

void MainWindow::makeTimeLineConnections() {
    PlaybackManager* playbackManager = m_editor->playback();

    TimeControls* timeControls = m_timeLine->timeControls();
    connect(timeControls, &TimeControls::endClick, playbackManager, &PlaybackManager::gotoEndFrame);
    connect(timeControls, &TimeControls::startClick, playbackManager, &PlaybackManager::gotoStartFrame);
    connect(timeControls, &TimeControls::prevKeyClick, playbackManager, &PlaybackManager::gotoPrevKey);
    connect(timeControls, &TimeControls::nextKeyClick, playbackManager, &PlaybackManager::gotoNextKey);
    connect(timeControls, &TimeControls::prevFrameClick, playbackManager, &PlaybackManager::gotoPrevFrame);
    connect(timeControls, &TimeControls::nextFrameClick, playbackManager, &PlaybackManager::gotoNextFrame);
    connect(timeControls, &TimeControls::fpsChanged, playbackManager, &PlaybackManager::setFps);
    connect(timeControls, &TimeControls::loopClick, playbackManager, &PlaybackManager::toggleLoop);
    connect(timeControls, &TimeControls::loopControlClick, playbackManager, &PlaybackManager::toggleRangedPlayback);
    connect(timeControls, &TimeControls::loopStartClick, playbackManager, &PlaybackManager::setRangedStartFrame);
    connect(timeControls, &TimeControls::loopEndClick, playbackManager, &PlaybackManager::setRangedEndFrame);
    timeControls->toggleLoopControl(true);
    playbackManager->setRangedStartFrame(timeControls->getRangeStart());
    playbackManager->setRangedEndFrame(timeControls->getRangeEnd());
    timeControls->toggleLoopControl(false);
    playbackManager->setFps(timeControls->getFps());

    connect(playbackManager, &PlaybackManager::playStateChanged, timeControls, &TimeControls::updatePlayState);

    connect(m_timeLine, &TimeLine::currentFrameChanged, playbackManager, &PlaybackManager::gotoFrame);
    connect(m_timeLine, &TimeLine::currentLayerChanged, m_editor, &Editor::setCurrentLayer);

    connect(playbackManager, &PlaybackManager::frameChanged, m_timeLine, &TimeLine::updateContent);

    connect(m_editor, &Editor::updateTimeLine, m_timeLine, &TimeLine::updateContent);

    LayerManager* layerManager = m_editor->layers();

    connect(m_timeLine, &TimeLine::newLayer, layerManager, &LayerManager::addLayer);
    connect(m_timeLine, &TimeLine::deleteCurrentLayer, layerManager, &LayerManager::deleteCurrentLayer);

    connect(layerManager, &LayerManager::layerCountChanged, m_timeLine, &TimeLine::updateContent);
    connect(layerManager, &LayerManager::layerCountChanged, m_timeLine, &TimeLine::updateLayerView);

    connect(m_editor->view(), &ViewManager::viewChanged, this, &MainWindow::updateZoomLabel);
    connect(m_editor, &Editor::currentFrameChanged, m_canvas, &TabletCanvas::updateFrame);

    connect(m_editor->undoStack(), &QUndoStack::cleanChanged, this, &MainWindow::updateTitleSaveState);
}

void MainWindow::makeGroupsWidgetConnections() {
    connect(m_editor, &Editor::currentFrameChanged, m_groupsWidget, &GroupsWidget::keyframeChanged);

    connect(m_editor->layers(), &LayerManager::currentLayerChanged, m_groupsWidget, &GroupsWidget::layerChanged);

    connect(m_canvas, &TabletCanvas::frameModified, m_groupsWidget, &GroupsWidget::keyframeChanged);
    connect(m_canvas, &TabletCanvas::groupModified, m_groupsWidget, &GroupsWidget::updateGroup);
    connect(m_canvas, &TabletCanvas::groupsModified, m_groupsWidget, &GroupsWidget::updateGroups);
    connect(m_canvas, &TabletCanvas::groupListModified, m_groupsWidget, &GroupsWidget::refreshGroups);
}
