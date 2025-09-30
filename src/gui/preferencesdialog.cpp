/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2017-2023 Pierre Benard <pierre.g.benard@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "preferencesdialog.h"

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSettings>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent,Qt::CustomizeWindowHint|Qt::WindowCloseButtonHint)
{
    setWindowTitle(tr("Preferences"));

    QSettings settings("manao", "Frite");
    QString style = settings.value("GUIStyle", "Light").toString();

    QLabel* styleLabel = new QLabel(tr("Style:"));
    m_styleBox = new QComboBox();
    m_styleBox->addItem(tr("Light"));
    m_styleBox->addItem(tr("Dark"));
#ifdef Q_OS_MAC
    m_styleBox->addItem(tr("Auto"));
#endif
    if(style == "Dark")
        m_styleBox->setCurrentIndex(1);
    else if(style == "Auto")
        m_styleBox->setCurrentIndex(2);
    connect(m_styleBox, SIGNAL(currentTextChanged(const QString &)), this, SLOT(styleChanged(const QString &)));
    QLabel* messageLabel = new QLabel(tr("<i>Restart required</i>"));

    QHBoxLayout* styleLayout = new QHBoxLayout;
    styleLayout->addWidget(styleLabel);
    styleLayout->addWidget(m_styleBox);

    QLabel* frameSizeLabel = new QLabel(tr("Frame size:"));
    m_frameSize = new QSlider(Qt::Horizontal, this);
    m_frameSize->setRange(4, 50);
    m_frameSize->setValue(settings.value("frameSize").toInt());
    connect(m_frameSize, SIGNAL(valueChanged(int)), this, SLOT(frameSizeChange(int)));
    QLabel* frameSizeValue = new QLabel();
    frameSizeValue->setNum(m_frameSize->value());
    connect(m_frameSize, SIGNAL(valueChanged(int)), frameSizeValue, SLOT(setNum(int)));

    QHBoxLayout* sliderLayout = new QHBoxLayout;
    sliderLayout->addWidget(m_frameSize);
    sliderLayout->addWidget(frameSizeValue);

    QLabel* fontSizeLabel = new QLabel(tr("Font size:"));
    mFontSize = new QSpinBox();
    mFontSize->setRange(4, 20);
    mFontSize->setValue(settings.value("labelFontSize").toInt());
    mFontSize->setFixedWidth(50);
    connect(mFontSize, SIGNAL(valueChanged(int)), this, SLOT(fontSizeChange(int)));

    QHBoxLayout* fontLayout = new QHBoxLayout;
    fontLayout->addWidget(fontSizeLabel);
    fontLayout->addWidget(mFontSize);

    QPushButton* closeButton = new QPushButton(tr("Close"));
    closeButton->setDefault(true);
    connect(closeButton, &QPushButton::clicked, this, &PreferencesDialog::close);

    QHBoxLayout* buttonsLayout = new QHBoxLayout;
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(closeButton);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(styleLayout);
    mainLayout->addWidget(messageLabel);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(frameSizeLabel);
    mainLayout->addLayout(sliderLayout);
    mainLayout->addLayout(fontLayout);
    mainLayout->addStretch(1);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(buttonsLayout);
    setLayout(mainLayout);
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::styleChanged(const QString &text)
{
    QSettings settings("manao", "Frite");
    settings.setValue("GUIStyle", text);
}

void PreferencesDialog::frameSizeChange(int value)
{
    QSettings settings("manao", "Frite");
    settings.setValue("frameSize", value);
    emit frameSizeChanged(value);
}

void PreferencesDialog::fontSizeChange(int value)
{
    QSettings settings("manao", "Frite");
    settings.setValue("labelFontSize", value);
    emit fontSizeChanged(value);
}
