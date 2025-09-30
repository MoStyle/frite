/*
 * SPDX-FileCopyrightText: 2013-2018 Matt Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _DIALS_AND_KNOBS_UI_H
#define _DIALS_AND_KNOBS_UI_H

#include <QtWidgets>

#include "dialsandknobs.h"

class FileNameLineEdit : public QLineEdit
{
    Q_OBJECT

  public:
    FileNameLineEdit(const QString& name) : _name(name) {}

  signals:
    void updateFilename(const QString& name);
    
  public slots:
    void setFromBrowser();
    void callUpdateFilename();

  private:
    QString _name;
};

class UpdatingTextEdit : public QTextEdit
{
    Q_OBJECT

  signals:
    void sendText(const QString& name);
    
  public slots:
    void callSendText();
    void updateText(const QString& value);
};

class ArbitraryPrecisionSpinBox : public QDoubleSpinBox
{
  public:
    ArbitraryPrecisionSpinBox(QWidget* parent = 0);

    virtual QString textFromValue( double value ) const;
};

class DockScrollArea : public QWidget
{
    Q_OBJECT
    
  public:
    DockScrollArea(QWidget* parent = 0);
    
    virtual QSize sizeHint();
    QLayout* childLayout() { return _scroller_child.layout(); }
    void setChildLayout(QLayout* layout);

  protected:
    QScrollArea _scroller;
    QWidget _scroller_child;
};

class ValueLabel : public QLabel
{
    Q_OBJECT
    
  public:
    ValueLabel(dkValue* dk_value, QWidget* parent = 0);
    
  public slots:
    void stickyToggled(bool toggle);
    
  protected:
    void contextMenuEvent(QContextMenuEvent *event);
    
  protected:
    dkValue* _dk_value;
};


#endif // _DIALS_AND_KNOBS_UI_H_
