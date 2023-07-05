/*
 * SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "onion_skins_docker.h"

#include <QSlider>
#include <QFrame>
#include <QPainter>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QSet>
#include <QSettings>
#include <QSizePolicy>

#include "equalizer/equalizer_widget.h"
#include "editor.h"
#include "tabletcanvas.h"

OnionSkinsDocker::OnionSkinsDocker(QWidget *parent, Editor *editor)
    : QDockWidget(tr("Onion Skins"), parent), m_editor(editor) {
    setObjectName("Onion Skins");
    QWidget *mainWidget = new QWidget(this);

    QLabel *tintLabel = new QLabel(tr("Tint: "));
    m_doubleTintFactor = new QSpinBox();
    m_doubleTintFactor->setMinimum(0);
    m_doubleTintFactor->setMaximum(100);
    m_doubleTintFactor->setSuffix("%");
    connect(m_doubleTintFactor, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_editor,
            &Editor::setTintFactor);

    QLabel *modeLabel = new QLabel(tr("Mode: "));
    m_mode = new QComboBox();
    m_mode->addItem("Keys");
    m_mode->addItem("Frames");
    connect(m_mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), m_editor,
            &Editor::setEqMode);

    QLabel *previousLabel = new QLabel(tr(" Previous"));
    m_btnBackwardColor = new QPushButton();
    m_btnBackwardColor->setToolTip(tr("Tint color for past frames"));
    connect(m_btnBackwardColor, &QPushButton::pressed, this, &OnionSkinsDocker::btnBackwardColorPressed);

    QLabel *nextLabel = new QLabel(tr("Next "));
    m_btnForwardColor = new QPushButton();
    m_btnForwardColor->setToolTip(tr("Tint color for future frames"));
    connect(m_btnForwardColor, &QPushButton::pressed, this, &OnionSkinsDocker::btnForwardColorPressed);

    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->addStretch();
    hLayout->addWidget(m_btnBackwardColor);
    hLayout->addWidget(previousLabel);
    hLayout->addStretch();
    hLayout->addWidget(modeLabel);
    hLayout->addWidget(m_mode);
    hLayout->addStretch();
    hLayout->addWidget(tintLabel);
    hLayout->addWidget(m_doubleTintFactor);
    hLayout->addStretch();
    hLayout->addWidget(nextLabel);
    hLayout->addWidget(m_btnForwardColor);
    hLayout->addStretch();

    m_equalizerWidget = new EqualizerWidget(10, this);
    connect(m_equalizerWidget, SIGNAL(sigConfigChanged()), this, SLOT(changed()));

    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->setSpacing(0);

    vLayout->addLayout(hLayout);
    vLayout->addWidget(m_equalizerWidget);

    mainWidget->setLayout(vLayout);
    setWidget(mainWidget);

    m_colorDialog = new QColorDialog(this);

    loadSettings();
    updateColorIcon(m_editor->backwardColor(), m_btnBackwardColor);
    updateColorIcon(m_editor->forwardColor(), m_btnForwardColor);
}

OnionSkinsDocker::~OnionSkinsDocker() {}

void OnionSkinsDocker::saveSettings() {
    QSettings settings("manao", "Frite");
    EqualizerValues v = m_equalizerWidget->getValues();

    settings.beginGroup("onionskin");

    settings.setValue("maxDistance", v.maxDistance);

    settings.beginWriteArray("equalizer");
    for (int i = -v.maxDistance; i <= v.maxDistance; i++) {
        settings.setArrayIndex(i + v.maxDistance);
        settings.setValue("opacity", v.value[i]);
        settings.setValue("state", v.state[i]);
    }
    settings.endArray();

    settings.setValue("maxDistance", v.maxDistance);
    settings.setValue("backwardColor", m_editor->backwardColor());
    settings.setValue("forwardColor", m_editor->forwardColor());
    settings.setValue("tintFactor", m_doubleTintFactor->value());
    settings.setValue("mode", m_mode->currentIndex());

    settings.endGroup();
}

void OnionSkinsDocker::btnBackwardColorPressed() {
    m_colorDialog->setCurrentColor(m_editor->backwardColor());
    if (m_colorDialog->exec() == QDialog::Accepted) {
        m_editor->setBackwardColor(m_colorDialog->currentColor());
        updateColorIcon(m_editor->backwardColor(), m_btnBackwardColor);
    }
}

void OnionSkinsDocker::btnForwardColorPressed() {
    m_colorDialog->setCurrentColor(m_editor->forwardColor());
    if (m_colorDialog->exec() == QDialog::Accepted) {
        m_editor->setForwardColor(m_colorDialog->currentColor());
        updateColorIcon(m_editor->forwardColor(), m_btnForwardColor);
    }
}

void OnionSkinsDocker::updateColorIcon(const QColor &c, QPushButton *button) {
    QPixmap pixmap(24, 24);
    if (!pixmap.isNull()) {
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing, false);
        painter.setPen(QColor(100, 100, 100));
        painter.setBrush(c);
        painter.drawRect(1, 1, 20, 20);
    }
    button->setIcon(pixmap);
}

void OnionSkinsDocker::slotToggleOnionSkins() { m_equalizerWidget->toggleMasterSwitch(); }

void OnionSkinsDocker::changed() { m_editor->setEqValues(m_equalizerWidget->getValues()); }

void OnionSkinsDocker::loadSettings() {
    QSignalBlocker blocker(m_equalizerWidget);

    QSettings settings("manao", "Frite");
    settings.beginGroup("onionskin");

    EqualizerValues v;
    v.maxDistance = settings.value("maxDistance", 10).toInt();

    settings.beginReadArray("equalizer");
    for (int i = -v.maxDistance; i <= v.maxDistance; i++) {
        settings.setArrayIndex(i + v.maxDistance);
        v.value.insert(i, settings.value("opacity").toInt());
        v.state.insert(i, settings.value("state").toBool());
    }
    settings.endArray();

    m_editor->setBackwardColor(settings.value("backwardColor", QColor(Qt::darkGreen)).value<QColor>());
    m_editor->setForwardColor(settings.value("forwardColor", QColor(Qt::darkMagenta)).value<QColor>());
    m_editor->setTintFactor(settings.value("tintFactor", 100).toInt());
    m_doubleTintFactor->setValue(m_editor->tintFactor());
    int mode = settings.value("mode", 0).toInt();
    m_editor->setEqMode(static_cast<EqualizedMode>(mode));
    m_mode->setCurrentIndex(mode);

    settings.endGroup();

    m_equalizerWidget->setValues(v);
    m_editor->setEqValues(v);
}
