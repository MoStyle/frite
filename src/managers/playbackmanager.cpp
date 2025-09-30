/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "playbackmanager.h"

#include <QTimer>
#include "editor.h"
#include "layermanager.h"
#include "layer.h"

PlaybackManager::PlaybackManager(QObject* parent) : BaseManager(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &PlaybackManager::timerTick);
}

PlaybackManager::~PlaybackManager() {}

void PlaybackManager::setCurrentFrame(int i) {
    if (m_currentFrame != i) {
        m_currentFrame = i;
        emit frameChanged(i);
    }
}

bool PlaybackManager::isPlaying() { return m_timer->isActive() || m_playing; }

void PlaybackManager::gotoStartFrame() { gotoFrame(m_startFrame); }

void PlaybackManager::gotoEndFrame() { gotoFrame(m_endFrame); }

void PlaybackManager::gotoPrevKey() {
    int prevKey = editor()->layers()->currentLayer()->getPreviousKeyFramePosition(m_currentFrame);
    if (m_currentFrame > prevKey) gotoFrame(prevKey);
}

void PlaybackManager::gotoNextKey() {
    int nextKey = editor()->layers()->currentLayer()->getNextKeyFramePosition(m_currentFrame);
    if (m_currentFrame < nextKey) gotoFrame(nextKey);
}

void PlaybackManager::gotoPrevFrame() {
    if (m_currentFrame > m_startFrame) gotoFrame(m_currentFrame - 1);
}

void PlaybackManager::gotoNextFrame() {
    if (m_currentFrame < m_endFrame) gotoFrame(m_currentFrame + 1);
}

void PlaybackManager::gotoFrame(int f) {
    m_currentFrame = f;
    editor()->scrubTo(f);
}

void PlaybackManager::play() {
    int projectLength = editor()->layers()->maxFrame();

    m_startFrame = (m_isRangedPlayback) ? m_markInFrame : 1;
    m_endFrame = (m_isRangedPlayback) ? m_markOutFrame : projectLength;

    if (m_currentFrame >= m_endFrame - 1) {
        editor()->scrubTo(m_startFrame);
    }

    m_timer->setInterval(int(1000.0f / m_fps));
    m_timer->start();
    emit playStateChanged(true);
}

void PlaybackManager::stop() {
    m_timer->stop();
    emit playStateChanged(false);
}

void PlaybackManager::setFps(int fps) {
    if (m_fps != fps) m_fps = fps;
}

void PlaybackManager::timerTick() {
    if (!m_isLooping && m_currentFrame >= m_endFrame - 1) {
        stop();
    } else if (m_isLooping && m_currentFrame >= m_endFrame) {
        editor()->scrubTo(m_startFrame);
    } else {
        editor()->scrubForward();
    }
}
