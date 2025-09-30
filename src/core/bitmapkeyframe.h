/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef BITMAP_KEYFRAME_H
#define BITMAP_KEYFRAME_H

#include <QPainter>
#include <QtXml>
#include <memory>

#include "keyframe.h"

class BitmapKeyFrame : public KeyFrame {
public:
  BitmapKeyFrame();
  BitmapKeyFrame(const QRectF &boundaries, const QImage &image);
  BitmapKeyFrame(const QString &path, const QPoint &topLeft);

  BitmapKeyFrame &operator=(const BitmapKeyFrame &a);
  // Image
  QImage *image() { return m_image.get(); }
  void setImage(QImage *pImg);

  virtual bool load(QDomElement &element, const QString &path, Editor *editor);
  virtual bool save(QDomDocument &doc, QDomElement &root, const QString &path,
                    int layer, int frame) const;

  BitmapKeyFrame copy();
  BitmapKeyFrame copy(QRectF rectangle);
  void paste(BitmapKeyFrame *);
  void paste(BitmapKeyFrame *, QPainter::CompositionMode cm);

  virtual void transform(QRectF newBoundaries, bool smoothTransform);

  void extend(QRectF rectangle);

  void clear();
  void clear(QRectF rectangle);

  void insertStartPoint(QPoint pos) { m_startPoints.push_back(pos); }
  QList<QPoint> &startPoints() { return m_startPoints; }

private:
  QString fileName(int layer, int frame) const;
  QImage grayscaled(const QImage &image);
  QImage tinted(const QImage &image, const QColor &color, qreal tintFactor);

  std::shared_ptr<QImage> m_image;
  QList<QPoint> m_startPoints;

  bool m_extendable = true;
};

#endif
