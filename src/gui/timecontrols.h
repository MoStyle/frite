/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef TIMECONTROL_H
#define TIMECONTROL_H

#include <QtWidgets>

class Editor;
class TimeLine;

class TimeControls : public QToolBar
{
    Q_OBJECT

public:
    TimeControls(TimeLine *parent, Editor* editor);
    ~TimeControls();

    int getFps() const { return fpsBox->value(); }
    void setFps(int value);
    void setLoopStart(int value);
    void stopPlaying();
    int getRangeStart() const { return loopControl->isChecked() ? loopStart->value() : -1; }
    int getRangeEnd() const { return loopControl->isChecked() ? loopEnd->value() : -1; }
    void updateLength(int frameLength);

signals:
    void prevKeyClick();
    void nextKeyClick();
    void prevFrameClick();
    void nextFrameClick();
    void endClick();
    void startClick();
    void loopClick(bool);
    void loopControlClick(bool);
    void fpsChanged(int);

    void loopToggled(bool);
    void loopControlToggled(bool);

    void loopStartClick(int);
    void loopEndClick(int);


public slots:
    void updatePlayState(bool);
    void playClicked(bool);
    void toggleLoop(bool);
    void toggleLoopControl(bool);
    void preLoopStartClick(int i);

private:
    QToolButton* playButton;
    QToolButton* endplayButton;
    QToolButton* startplayButton;
    QToolButton* nextKeyButton;
    QToolButton* prevKeyButton;
    QToolButton* nextFrameButton;
    QToolButton* prevFrameButton;
    QToolButton* loopButton;
    QSpinBox* fpsBox;
    QCheckBox* loopControl;
    QSpinBox* loopStart;
    QSpinBox* loopEnd;

    Editor* m_editor;
};

#endif
