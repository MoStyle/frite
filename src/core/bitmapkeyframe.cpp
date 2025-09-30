/*
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#include <cmath>

#include "bitmapkeyframe.h"
#include "dialsandknobs.h"

extern dkInt k_cellSize;

BitmapKeyFrame::BitmapKeyFrame() {
  m_image = std::make_shared<QImage>(); // null image
  m_bounds = QRectF(0, 0, 0, 0);
}

BitmapKeyFrame::BitmapKeyFrame(const QRectF &rectangle, const QImage &image) {
  m_bounds = rectangle.normalized();
  m_extendable = true;
  m_image = std::make_shared<QImage>(image);
  if (m_image->width() != rectangle.width() ||
      m_image->height() != rectangle.height()) {
    qDebug() << "Error instancing bitmapKey: " << m_image->width()
             << "!=" << rectangle.width() << " || " << m_image->height()
             << "!=" << rectangle.height();
  }
}

BitmapKeyFrame::BitmapKeyFrame(const QString &path, const QPoint &topLeft) {
  m_image = std::make_shared<QImage>(path);
  if (m_image->isNull()) {
    qDebug() << "ERROR: Image " << path << " not loaded";
  }
  m_bounds = QRectF(topLeft, m_image->size());
}

BitmapKeyFrame &BitmapKeyFrame::operator=(const BitmapKeyFrame &a) {
  m_bounds = a.m_bounds;
  m_image = std::make_shared<QImage>(*a.m_image);
  m_startPoints = a.m_startPoints;
  return *this;
}

void BitmapKeyFrame::setImage(QImage *img) {
  Q_CHECK_PTR(img);
  m_image.reset(img);
}

// Convert an image to grayscale and return it as a new image
QImage BitmapKeyFrame::grayscaled(const QImage &image) {
  QImage img = image;
  int pixels = img.width() * img.height();
  unsigned int *data = (unsigned int *)img.bits();
  for (int i = 0; i < pixels; ++i) {
    int val = qGray(data[i]);
    data[i] = qRgba(val, val, val, qAlpha(data[i]));
  }
  return img;
}

// Tint an image with the specified color and return it as a new image
QImage BitmapKeyFrame::tinted(const QImage &image, const QColor &color,
                              qreal tintFactor) {
  QImage resultImage(image.size(), QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&resultImage);
  painter.drawImage(0, 0, grayscaled(image));
  painter.setCompositionMode(QPainter::CompositionMode_Screen);
  painter.fillRect(resultImage.rect(), color);
  painter.setOpacity(1. - tintFactor);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.drawImage(0, 0, image);
  painter.end();
  resultImage.setAlphaChannel(image.convertToFormat(QImage::Format_Alpha8));

  return resultImage;
}

QString BitmapKeyFrame::fileName(int layer, int frame) const {
  QString layerNumberString = QString::number(layer);
  QString frameNumberString = QString::number(frame);
  while (layerNumberString.length() < 3)
    layerNumberString.prepend("0");
  while (frameNumberString.length() < 3)
    frameNumberString.prepend("0");
  return layerNumberString + "." + frameNumberString + ".png";
}

bool BitmapKeyFrame::load(QDomElement &element, const QString &path, Editor *editor) {
  int x = element.attribute("topLeftX").toInt();
  int y = element.attribute("topLeftY").toInt();
  if (element.hasAttribute("src")) {
    QString dataPath =
        path + "/" +
        element.attribute(
            "src"); // the file is supposed to be in the data directory
    QFileInfo fi(dataPath);
    if (!fi.exists()) {
      qWarning("BitmapImage could not be loaded from \"%s\"", qPrintable(path));
    }
    m_image = std::make_shared<QImage>(path);
    if (m_image->isNull()) {
      qDebug() << "ERROR: Image " << path << " not loaded";
    }
    m_bounds = QRectF(QPoint(x, y), m_image->size());

    QDomElement sp = element.firstChildElement();
    if (!sp.isNull()) {
      QString string = sp.text();

      QTextStream pos(&string);
      for (int j = 0; j < sp.attribute("size").toInt(); ++j) {
        int x, y;
        pos >> x >> y;
        m_startPoints.append(QPoint(x, y));
      }
    }
  }
  return true;
}

bool BitmapKeyFrame::save(QDomDocument &doc, QDomElement &root,
                          const QString &path, int layer, int frame) const {
  QDomElement keyElt = doc.createElement("bitmapkeyframe");
  keyElt.setAttribute("frame", frame);
  QString filename = fileName(layer, frame);
  if (!m_image->isNull())
    keyElt.setAttribute("src", filename);
  keyElt.setAttribute("topLeftX", m_bounds.x());
  keyElt.setAttribute("topLeftY", m_bounds.y());

  QDomElement sp = doc.createElement("stroke");
  sp.setAttribute("size", m_startPoints.size());
  keyElt.appendChild(sp);

  QString string;
  QTextStream startPos(&string);
  for (QPoint p : m_startPoints) {
    startPos << p.x() << " " << p.y() << " ";
  }
  QDomText txt = doc.createTextNode(string);
  sp.appendChild(txt);

  QString filePath = QDir(path).filePath(filename);
  if (!m_image->save(filePath) && !m_image->isNull()) {
    qWarning("BitmapImage could not be saved in \"%s\"", qPrintable(filename));
    return false;
  }

  root.appendChild(keyElt);
  return true;
}

BitmapKeyFrame BitmapKeyFrame::copy() {
  BitmapKeyFrame result = BitmapKeyFrame(m_bounds, QImage(*m_image));
  result.m_startPoints = m_startPoints;
  return result;
}

BitmapKeyFrame BitmapKeyFrame::copy(QRectF rectangle) {
  QRectF intersection2 = rectangle.translated(-topLeft());
  BitmapKeyFrame result =
      BitmapKeyFrame(rectangle, m_image->copy(intersection2.toRect()));
  result.m_startPoints = m_startPoints;
  return result;
}

void BitmapKeyFrame::paste(BitmapKeyFrame *bitmapKey) {
  paste(bitmapKey, QPainter::CompositionMode_SourceOver);
}

void BitmapKeyFrame::paste(BitmapKeyFrame *bitmapKey,
                           QPainter::CompositionMode cm) {
  QRectF newBoundaries;
  if (m_image->width() == 0 || m_image->height() == 0) {
    newBoundaries = bitmapKey->m_bounds;
  } else {
    newBoundaries = m_bounds.united(bitmapKey->m_bounds);
  }
  extend(newBoundaries);

  QImage *image2 = bitmapKey->image();

  QPainter painter(m_image.get());
  painter.setCompositionMode(cm);
  painter.drawImage(bitmapKey->m_bounds.topLeft() - m_bounds.topLeft(),
                    *image2);
  painter.end();
}

void BitmapKeyFrame::transform(QRectF newBoundaries, bool smoothTransform) {
  m_bounds = newBoundaries;
  newBoundaries.moveTopLeft(QPoint(0, 0));
  QImage *newImage =
      new QImage(m_bounds.size().toSize(), QImage::Format_ARGB32_Premultiplied);
  QPainter painter(newImage);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, smoothTransform);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(newImage->rect(), QColor(0, 0, 0, 0));
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.drawImage(newBoundaries, *m_image);
  painter.end();
  m_image.reset(newImage);
}

void BitmapKeyFrame::extend(QRectF rectangle) {
  if (!m_extendable)
    return;
  if (rectangle.width() <= 0)
    rectangle.setWidth(1);
  if (rectangle.height() <= 0)
    rectangle.setHeight(1);
  if (m_bounds.contains(rectangle)) {
    // nothing
  } else {
    QRectF newBoundaries = m_bounds.united(rectangle).normalized();
    QImage *newImage = new QImage(newBoundaries.size().toSize(),
                                  QImage::Format_ARGB32_Premultiplied);
    newImage->fill(qRgba(0, 0, 0, 0));
    if (!newImage->isNull()) {
      QPainter painter(newImage);
      painter.drawImage(m_bounds.topLeft() - newBoundaries.topLeft(), *m_image);
      painter.end();
    }
    m_image.reset(newImage);
    m_bounds = newBoundaries;
  }
}

// Erase grid when clearing
void BitmapKeyFrame::clear() {
  m_image = std::make_shared<QImage>(); // null image
  m_bounds = QRectF(0, 0, 0, 0);
}

void BitmapKeyFrame::clear(QRectF rectangle) {
  QRectF clearRectangle = m_bounds.intersected(rectangle);
  clearRectangle.moveTopLeft(clearRectangle.topLeft() - topLeft());

  QPainter painter(m_image.get());
  painter.setCompositionMode(QPainter::CompositionMode_Clear);
  painter.fillRect(clearRectangle, QColor(0, 0, 0, 0));
  painter.end();
  m_startPoints.clear();
}
