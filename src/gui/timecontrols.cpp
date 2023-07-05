/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtGui>
#include <QLabel>

#include "timecontrols.h"
#include "playbackmanager.h"
#include "editor.h"
#include "timeline.h"
#include "stylemanager.h"


TimeControls::TimeControls(TimeLine *parent, Editor* editor)
    : QToolBar(parent), m_editor(editor)
{
    QSettings settings("manao","Frite");

    QFont f = QApplication::font();
    f.setPointSize(10);

    fpsBox = new QSpinBox();
    fpsBox->setFont(f);
    fpsBox->setFixedHeight(26);
    fpsBox->setValue(settings.value("fps").toInt());
    fpsBox->setMinimum(1);
    fpsBox->setMaximum(90);
    fpsBox->setSuffix(" fps");
    fpsBox->setToolTip("Frames per second");
    fpsBox->setFocusPolicy(Qt::ClickFocus);

    loopStart = new QSpinBox();
    loopStart->setFont(f);
    loopStart->setFixedHeight(26);
    loopStart->setValue(settings.value("loopStart").toInt());
    loopStart->setMinimum(1);
    loopStart->setMaximum(parent->getFrameLength() - 1);
    loopStart->setToolTip("Start of loop");
    loopStart->setEnabled(false);
    loopStart->setFocusPolicy(Qt::ClickFocus);

    loopEnd = new QSpinBox();
    loopEnd->setFont(f);
    loopEnd->setFixedHeight(26);
    loopEnd->setValue(settings.value("loopEnd").toInt());
    loopEnd->setMinimum(2);
    loopEnd->setMaximum(parent->getFrameLength());
    loopEnd->setToolTip("End of loop");
    loopEnd->setEnabled(false);
    loopEnd->setFocusPolicy(Qt::ClickFocus);

    loopControl = new QCheckBox("Range");
    loopControl->setFont(f);
    loopControl->setFixedHeight(26);
    loopControl->setToolTip(tr("Play range"));
    loopControl->setCheckable(true);

    StyleManager* styleManager = m_editor->style();
    QIcon playIcon;
    playIcon.addFile(styleManager->getResourcePath("play"),QSize(),QIcon::Normal,QIcon::Off);
    playIcon.addFile(styleManager->getResourcePath("stop"),QSize(),QIcon::Normal,QIcon::On);

    playButton = new QToolButton();
    playButton->setIcon(playIcon);
    playButton->setToolTip("Play");
    loopButton = new QToolButton();
    loopButton->setIcon(styleManager->getIcon("loop"));
    loopButton->setToolTip("Loop");
    endplayButton = new QToolButton();
    endplayButton->setIcon(styleManager->getIcon("lastframe"));
    endplayButton->setToolTip("Last Frame");
    endplayButton->setShortcut(QKeySequence(tr("End")));
    startplayButton = new QToolButton();
    startplayButton->setIcon(styleManager->getIcon("firstframe"));
    startplayButton->setToolTip("First Frame");
    startplayButton->setShortcut(QKeySequence(tr("Home")));
    nextKeyButton = new QToolButton();
    nextKeyButton->setIcon(styleManager->getIcon("nextkeyframe"));
    nextKeyButton->setToolTip("Next Keyframe");
    nextKeyButton->setShortcut(QKeySequence(tr("Ctrl+Right")));
    prevKeyButton = new QToolButton();
    prevKeyButton->setIcon(styleManager->getIcon("prevkeyframe"));
    prevKeyButton->setToolTip("Previous Keyframe");
    prevKeyButton->setShortcut(QKeySequence(tr("Ctrl+Left")));
    nextFrameButton = new QToolButton();
    nextFrameButton->setIcon(styleManager->getIcon("nextframe"));
    nextFrameButton->setToolTip("Next Frame");
    nextFrameButton->setShortcut(QKeySequence(tr("Right")));
    prevFrameButton = new QToolButton();
    prevFrameButton->setIcon(styleManager->getIcon("prevframe"));
    prevFrameButton->setToolTip("Previous Frame");
    prevFrameButton->setShortcut(QKeySequence(tr("Left")));

    QLabel* separator = new QLabel();
    separator->setPixmap(QPixmap(":images/separator.png"));
    separator->setFixedSize(QSize(37,31));

    playButton->setShortcut(QKeySequence(Qt::Key_Space));
    playButton->setCheckable(true);
    loopButton->setCheckable(true);

    addWidget(separator);
    addWidget(startplayButton);
    addWidget(prevKeyButton);
    addWidget(prevFrameButton);
    addWidget(playButton);
    addWidget(nextFrameButton);
    addWidget(nextKeyButton);
    addWidget(endplayButton);
    addWidget(loopButton);
    addWidget(loopControl);
    addWidget(loopStart);
    addWidget(loopEnd);
    addWidget(fpsBox);

    connect(playButton,      SIGNAL(toggled(bool)),     this, SLOT(playClicked(bool)));
    connect(endplayButton,   SIGNAL(clicked()),         this, SIGNAL(endClick()));
    connect(startplayButton, SIGNAL(clicked()),         this, SIGNAL(startClick()));
    connect(nextKeyButton,   SIGNAL(clicked()),         this, SIGNAL(nextKeyClick()));
    connect(prevKeyButton,   SIGNAL(clicked()),         this, SIGNAL(prevKeyClick()));
    connect(nextFrameButton, SIGNAL(clicked()),         this, SIGNAL(nextFrameClick()));
    connect(prevFrameButton, SIGNAL(clicked()),         this, SIGNAL(prevFrameClick()));
    connect(loopButton,      SIGNAL(toggled(bool)),     this, SIGNAL(loopClick(bool)));
    connect(loopControl,     SIGNAL(toggled(bool)),     this, SIGNAL(loopControlClick(bool)));
    connect(fpsBox,          SIGNAL(valueChanged(int)), this, SIGNAL(fpsChanged(int)));
    connect(loopStart,       SIGNAL(valueChanged(int)), this, SLOT(preLoopStartClick(int)) );
    connect(loopEnd,         SIGNAL(valueChanged(int)), this, SIGNAL(loopEndClick(int)) );

    connect( loopControl, &QCheckBox::toggled, loopStart, &QSpinBox::setEnabled );
    connect( loopControl, &QCheckBox::toggled, loopEnd, &QSpinBox::setEnabled );
}

TimeControls::~TimeControls()
{
    QSettings settings("manao","Frite");
    settings.setValue("fps", fpsBox->value());
    settings.setValue("loopStart", loopStart->value());
    settings.setValue("loopEnd", loopEnd->value());
}

void TimeControls::setFps ( int value )
{
    fpsBox->setValue(value);
}

void TimeControls::playClicked(bool)
{
    if (m_editor->playback()->isPlaying()) {
        m_editor->playback()->stop();
        playButton->setChecked(false);
    } else {
        m_editor->playback()->play();
        playButton->setChecked(true);
    }
}

void TimeControls::updatePlayState(bool)
{
    const QSignalBlocker blocker(playButton);
    if (m_editor->playback()->isPlaying()) {
        playButton->setChecked(true);
    } else {
        playButton->setChecked(false);
    }
}

void TimeControls::toggleLoop(bool checked)
{
    loopButton->setChecked(checked);
}

void TimeControls::toggleLoopControl(bool checked)
{
    loopControl->setChecked(checked);
}

void TimeControls::setLoopStart(int value)
{
    loopStart->setValue(value);
}

void TimeControls::stopPlaying()
{
    playButton->setChecked(false);
}

void TimeControls::preLoopStartClick(int i) {
    if(i >= loopEnd->value())
        loopEnd->setValue(i + 1);
    loopEnd->setMinimum(i + 1);

    emit loopStartClick(i);
}

void TimeControls::updateLength(int frameLength) {
    loopStart->setMaximum(frameLength - 1);
    loopEnd->setMaximum(frameLength);
}
