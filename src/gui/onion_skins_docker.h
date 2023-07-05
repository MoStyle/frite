/*
 * SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ONION_SKINS_DOCKER_H
#define ONION_SKINS_DOCKER_H

#include <QDockWidget>
#include <QResizeEvent>
class QColorDialog;
class QSpinBox;
class QComboBox;
class QPushButton;
class EqualizerWidget;
class Editor;

class OnionSkinsDocker : public QDockWidget
{
    Q_OBJECT

public:
    explicit OnionSkinsDocker(QWidget *parent = 0, Editor* editor = 0);
    ~OnionSkinsDocker();

    void saveSettings();

protected:
    void resizeEvent(QResizeEvent *event) { qDebug() << "resizing: " << event->size(); qDebug() << "min size: " << minimumSize(); }

private:
    EqualizerWidget *m_equalizerWidget;
    QPushButton *m_btnBackwardColor, *m_btnForwardColor;
    QColorDialog* m_colorDialog;
    QSpinBox* m_doubleTintFactor;
    QComboBox* m_mode;

    Editor* m_editor;

private:
    void loadSettings();
    void updateColorIcon(const QColor &c, QPushButton *button);

private Q_SLOTS:
    void changed();
    void slotToggleOnionSkins();
    void btnBackwardColorPressed();
    void btnForwardColorPressed();
};

#endif // ONION_SKINS_DOCKER_H
