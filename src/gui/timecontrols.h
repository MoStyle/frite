/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation;

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

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
