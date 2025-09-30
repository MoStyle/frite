/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtWidgets>

#include "timecontrols.h"
#include "timelinecells.h"
#include "timeline.h"
#include "layer.h"
#include "layermanager.h"
#include "stylemanager.h"
#include "editor.h"

TimeLine::TimeLine(Editor *editor, QWidget *parent) : QDockWidget(parent, Qt::Tool)
{
    setObjectName(tr("Timeline"));
    QWidget* timeLineContent = new QWidget(this);
    m_editor = editor;
    connect(m_editor, &Editor::currentFrameChanged, this, &TimeLine::updateFrame);

    m_layersNames = new TimeLineCells(this, editor, TYPE_LAYER_ATTR);
    m_tracks = new TimeLineCells(this, editor, TYPE_TRACKS);
    connect(m_layersNames, SIGNAL(mouseMovedY(int)), m_layersNames, SLOT(setMouseMoveY(int)));
    connect(m_layersNames, SIGNAL(mouseMovedY(int)), m_tracks, SLOT(setMouseMoveY(int)));
    connect(m_tracks, &TimeLineCells::lengthChanged, this, &TimeLine::updateLength );

    m_hScrollBar = new QScrollBar(Qt::Horizontal);
    m_vScrollBar = new QScrollBar(Qt::Vertical);
    m_vScrollBar->setMinimum(0);
    m_vScrollBar->setMaximum(1);
    m_vScrollBar->setPageStep(1);

    QWidget* leftWidget = new QWidget();
    leftWidget->setMinimumWidth(120);
    QWidget* rightWidget = new QWidget();

    QWidget* leftToolBar = new QWidget();
    leftToolBar->setFixedHeight(31);
    QWidget* rightToolBar = new QWidget();
    rightToolBar->setFixedHeight(31);

    // --- left widget ---
    // --------- layer buttons ---------
    QToolBar* layerButtons = new QToolBar(this);

    QLabel* layerLabel = new QLabel(tr("Layers: "));
    layerLabel->setIndent(5);
    QFont f = QApplication::font();
    f.setPointSize(10);
    layerLabel->setFont(f);

    StyleManager* styleManager = m_editor->style();
    QToolButton* addLayerButton = new QToolButton(this);
    addLayerButton->setIcon(styleManager->getIcon("add"));
    addLayerButton->setToolTip("Add Layer");
    addLayerButton->setFixedSize(21,21);
    addLayerButton->setShortcut(QKeySequence(Qt::META|Qt::Key_N));

    QToolButton* removeLayerButton = new QToolButton(this);
    removeLayerButton->setIcon(styleManager->getIcon("remove"));
    removeLayerButton->setToolTip("Remove Layer");
    removeLayerButton->setFixedSize(21,21);

    layerButtons->addWidget(layerLabel);
    layerButtons->addWidget(addLayerButton);
    layerButtons->addWidget(removeLayerButton);

    QHBoxLayout* leftToolBarLayout = new QHBoxLayout();
    leftToolBarLayout->setAlignment(Qt::AlignLeft);
    leftToolBarLayout->setContentsMargins(0,0,0,0);
    leftToolBarLayout->addWidget(layerButtons);
    leftToolBar->setLayout(leftToolBarLayout);

    QGridLayout* leftLayout = new QGridLayout();
    leftLayout->addWidget(leftToolBar,0,0);
    leftLayout->addWidget(m_layersNames,1,0);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(0);
    leftWidget->setLayout(leftLayout);

    // --- right widget ---
    // --------- key buttons ---------
    QToolBar* keyButtons = new QToolBar(this);
    QLabel* keyLabel = new QLabel(tr("Keys:"));
    keyLabel->setFont(f);
    keyLabel->setIndent(5);

    QToolButton* addKeyButton = new QToolButton(this);
    addKeyButton->setIcon(styleManager->getIcon("add"));
    addKeyButton->setToolTip("Add Frame");
    addKeyButton->setFixedSize(21,21);

    QToolButton* removeKeyButton = new QToolButton(this);
    removeKeyButton->setIcon(styleManager->getIcon("remove"));
    removeKeyButton->setToolTip("Remove Frame");
    removeKeyButton->setFixedSize(21,21);

    QToolButton* duplicateKeyButton = new QToolButton(this);
    duplicateKeyButton->setIcon(styleManager->getIcon("duplicate"));
    duplicateKeyButton->setToolTip("Duplicate Frame");
    duplicateKeyButton->setFixedSize(21,21);

    keyButtons->addWidget(keyLabel);
    keyButtons->addWidget(addKeyButton);
    keyButtons->addWidget(removeKeyButton);
    keyButtons->addWidget(duplicateKeyButton);

    // --------- Time controls ---------
    m_timeControls = new TimeControls(this, m_editor);
    m_timeControls->setFocusPolicy(Qt::NoFocus);
    updateLength();

    QHBoxLayout* rightToolBarLayout = new QHBoxLayout();
    rightToolBarLayout->addWidget(keyButtons);
    rightToolBarLayout->addStretch(1);
    rightToolBarLayout->addWidget(m_timeControls);
    rightToolBarLayout->setContentsMargins(0,0,0,0);
    rightToolBarLayout->setSpacing(0);
    rightToolBar->setLayout(rightToolBarLayout);

    QGridLayout* rightLayout = new QGridLayout();
    rightLayout->addWidget(rightToolBar,0,0);
    rightLayout->addWidget(m_tracks,1,0);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->setSpacing(0);
    rightWidget->setLayout(rightLayout);

    // --- Splitter ---
    QSplitter* splitter = new QSplitter(parent);
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setSizes(QList<int>() << 100 << 600);

    QGridLayout* lay = new QGridLayout();
    lay->addWidget(splitter,0,0);
    lay->addWidget(m_vScrollBar,0,1);
    lay->addWidget(m_hScrollBar,1,0);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);
    timeLineContent->setLayout(lay);
    setWidget(timeLineContent);

    setWindowFlags(Qt::WindowStaysOnTopHint);
    setWindowTitle("Timeline");
    setFloating(false);

    connect(this,SIGNAL(fontSizeChange(int)), m_tracks, SLOT(fontSizeChange(int)));
    connect(this,SIGNAL(frameSizeChange(int)), m_tracks, SLOT(frameSizeChange(int)));
    connect(this,SIGNAL(labelChange(int)), m_tracks, SLOT(labelChange(int)));

    connect(m_hScrollBar,SIGNAL(valueChanged(int)), m_tracks, SLOT(hScrollChange(int)));
    connect(m_vScrollBar,SIGNAL(valueChanged(int)), m_tracks, SLOT(vScrollChange(int)));
    connect(m_vScrollBar,SIGNAL(valueChanged(int)), m_layersNames, SLOT(vScrollChange(int)));

    connect(addKeyButton,       &QToolButton::clicked, m_editor, &Editor::addKey);
    connect(duplicateKeyButton, &QToolButton::clicked, m_editor, &Editor::duplicateKey);
    connect(removeKeyButton,    &QToolButton::clicked, m_editor, &Editor::removeKey);

    connect(addLayerButton, SIGNAL(clicked()), this, SIGNAL(newLayer()));
    connect(removeLayerButton, SIGNAL(clicked()), this, SIGNAL(deleteCurrentLayer()));
    connect(m_layersNames, SIGNAL(currentLayerChanged(int)), this, SIGNAL(currentLayerChanged(int)));
    connect(m_tracks, SIGNAL(currentFrameChanged(int)), this, SIGNAL(currentFrameChanged(int)));

    scrubbing = false;
    m_lastUpdatedFrame = 1;
}

int TimeLine::getFrameLength()
{
    return m_tracks->getFrameLength();
}

void TimeLine::resizeEvent(QResizeEvent*)
{
    updateLayerView();
}

void TimeLine::wheelEvent(QWheelEvent* event)
{
    if(event->modifiers() & Qt::ShiftModifier)
        m_hScrollBar->event(event);
    else
        m_vScrollBar->event(event);
}

void TimeLine::updateFrame(int frameNumber)
{
    Q_ASSERT(m_tracks);

    m_tracks->updateFrame(m_lastUpdatedFrame);
    m_tracks->updateFrame(frameNumber);

    m_lastUpdatedFrame = frameNumber;
}

void TimeLine::updateLayerView()
{
    m_vScrollBar->setPageStep((height()-m_tracks->getOffsetY()-m_hScrollBar->height())/m_tracks->getLayerHeight() -2);
    m_vScrollBar->setMinimum(0);
    m_vScrollBar->setMaximum(qMax(0, m_editor->layers()->layersCount() - m_vScrollBar->pageStep()));
    update();
    updateContent();
}

void TimeLine::updateLength()
{
    int frameLength = getFrameLength();
    m_hScrollBar->setMaximum( qMax( 0, frameLength - m_tracks->width() / m_tracks->getFrameSize() ) );
    m_timeControls->updateLength(frameLength);
    update();
    updateContent();
}

void TimeLine::updateContent()
{
    m_layersNames->updateContent();
    m_tracks->updateContent();
    update();
}
