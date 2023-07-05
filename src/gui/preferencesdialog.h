/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    PreferencesDialog(QWidget* parent);
    ~PreferencesDialog();

signals:
    void frameSizeChanged(int value);
    void fontSizeChanged(int value);

private slots:
    void styleChanged(const QString &text);
    void frameSizeChange(int value);
    void fontSizeChange(int value);

private:
    QComboBox* m_styleBox;
    QSlider*   m_frameSize;
    QSpinBox*  mFontSize;
};

#endif // PREFERENCESDIALOG_H
