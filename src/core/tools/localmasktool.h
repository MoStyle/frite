/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef __LOCALMASKTOOL_H__
#define __LOCALMASKTOOL_H__

#include "tool.h"
#include "polyline.h"

class LocalMaskTool : public Tool {
    Q_OBJECT
public:
    LocalMaskTool(QObject *parent, Editor *editor);
    virtual ~LocalMaskTool() {}

    Tool::ToolType toolType() const override;

    QCursor makeCursor(float scaling=1.0f) const override;

    void toggled(bool on) override;
    void pressed(const EventInfo& info) override;
    void moved(const EventInfo& info) override;
    void released(const EventInfo& info) override;
    void wheel(const WheelEventInfo& info) override;
    void keyReleased(QKeyEvent *event) override;
    void drawUI(QPainter &painter, VectorKeyFrame *key) override;
    void drawGL(VectorKeyFrame *key, qreal alpha) override;
    void setValidingCusters(bool b);
    bool validatingClusters() const { return m_validatingClusters; }

private:
    void paint(const EventInfo& info);
    void projectVisibility(const EventInfo &info);
    void updateGradient(const EventInfo &info);

    QFontMetrics m_fontMetrics;
    bool m_pressed, m_pressedFirstFrameNumber, m_pressedLastFrameNumber, m_projected, m_onFrameNumber;
    Frite::Polyline m_polyline;
    Point::VectorType m_firstProjectedPoint, m_lastProjectedPoint;
    QPointF m_pressedPos;
    QRect m_firstPointRect, m_lastPointRect;
    double m_alpha, m_sign, m_firstProjectionVisibility, m_lastProjectionVisibility, m_firstProjectionParam, m_lastProjectionParam;
    VectorKeyFrame *m_prevKeyFrame;
    QHash<unsigned int, double> m_savedVisibility;

    bool m_validatingClusters;
};

#endif // __LOCALMASKTOOL_H__