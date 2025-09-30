/*
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 */

#ifndef PROJECTPROPERTIESDIALOG_H
#define PROJECTPROPERTIESDIALOG_H

#include <QDialog>
#include <QSpinBox>

class QSpinBox;

class ProjectPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    ProjectPropertiesDialog(QWidget *parent, int width, int height);
    int  getWidth() { return m_widthBox->value(); }
    void setWidth(int w) { m_widthBox->setValue(w); }
    int  getHeight() { return m_heightBox->value(); }
    void setHeight(int h) { m_heightBox->setValue(h); }
protected:
    QSpinBox *m_widthBox, *m_heightBox;
};

#endif // PROJECTPROPERTIESDIALOG_H
