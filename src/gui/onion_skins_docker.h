/*
 *  Copyright (c) 2015 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
    void resizeEvent(QResizeEvent *event) { }

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
