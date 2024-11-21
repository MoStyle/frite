#ifndef PLAYBACKMANAGER_H
#define PLAYBACKMANAGER_H

#include "basemanager.h"

class QTimer;

class PlaybackManager : public BaseManager {
    Q_OBJECT
   public:
    PlaybackManager(QObject* parent);
    ~PlaybackManager();

    void setCurrentFrame(int i);
    int currentFrame() { return m_currentFrame; }

    bool isPlaying();
    bool isLooping() { return m_isLooping; }

    void play();
    void stop();

    int fps() { return m_fps; }
    int startFrame() { return m_startFrame; }
    void setStartFrame(int f) { m_startFrame = f; }
    int endFrame() { return m_endFrame; }
    void setEndFrame(int f) { m_endFrame = f; }

    int markInFrame() { return m_markInFrame; }
    int markOutFrame() { return m_markOutFrame; }

    void setPlaying(bool playing) { m_playing = playing; }

   public slots:
    void gotoFrame(int f);
    void setFps(int fps);
    void gotoStartFrame();
    void gotoEndFrame();
    void gotoPrevKey();
    void gotoNextKey();
    void gotoPrevFrame();
    void gotoNextFrame();
    void toggleLoop(bool b) { m_isLooping = b; }
    void toggleRangedPlayback(bool b) { m_isRangedPlayback = b; }
    void setRangedStartFrame(int frame) { m_markInFrame = frame; }
    void setRangedEndFrame(int frame) { m_markOutFrame = frame; }

   signals:
    void frameChanged(int);
    void playStateChanged(bool isPlaying);

   private:
    void timerTick();

    int m_currentFrame = 1;
    int m_startFrame = 1;
    int m_endFrame = 60;

    bool m_isLooping = false;
    bool m_playing = false;

    bool m_isRangedPlayback = false;
    int m_markInFrame = 1;
    int m_markOutFrame = 10;

    int m_fps = 24;

    QTimer* m_timer = nullptr;
};

#endif  // PLAYBACKMANAGER_H
