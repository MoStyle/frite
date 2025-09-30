/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __DIRECTMATCHINGTOOL_H__
#define __DIRECTMATCHINGTOOL_H__

#include "tool.h"
#include "uvhash.h"

class Group; 

class DirectMatchingTool : public Tool {
    Q_OBJECT
public:
    DirectMatchingTool(QObject *parent, Editor *editor);
    virtual ~DirectMatchingTool();

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;
    
    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void doublepressed(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void keyReleased(QKeyEvent *event) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;

private:
    QPointF m_firstPos, m_curPos;
    QHash<int, UVInfo> m_pinUVs;
    bool m_addPinCommand;
    bool m_addCorrespondencePinCommand;
    bool m_movingExistingPin;
    QHash<int, UVInfo> m_existingPinMoved;
};

#endif // __DIRECTMATCHINGTOOL_H__