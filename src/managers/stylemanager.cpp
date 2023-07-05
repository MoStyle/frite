/*
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stylemanager.h"
#include "macosx/platformhandler.h"

#include <QIcon>
#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QSettings>
#include <QFont>

StyleManager::StyleManager(QObject* parent) : QObject(parent)
{
    QSettings settings("manao", "Frite");
    QString style = settings.value("GUIStyle", "Light").toString();
#ifdef Q_OS_MAC
    if(style == "Auto") {
        if(PlatformHandler::isDarkMode()){
            switchToDark();
            qDebug("Auto dark");
        }else{
            switchToLight();
            qDebug("Auto light");
        }
    }
#endif

    if(style == "Light")
        switchToLight();
    else if(style == "Dark")
        switchToDark();
}

QString StyleManager::getResourcePath(const QString & name)
{
    return QString(":/%1/%2").arg(m_isLightStyle ? "dark" : "light").arg(name);
}

QIcon StyleManager::getIcon(const QString & name)
{
    return QIcon(getResourcePath(name));
}

void StyleManager::switchToDark()
{
    qApp->setStyle(QStyleFactory::create("fusion"));
    QPalette pal;

    pal.setColor(QPalette::WindowText, QColor("#e7e7e7"));
    pal.setColor(QPalette::Button, QColor("#232323"));
    pal.setColor(QPalette::Light, QColor("#484848"));
    pal.setColor(QPalette::Midlight, QColor("#808080"));
    pal.setColor(QPalette::Dark, QColor("#c8c8c8"));
    pal.setColor(QPalette::Mid, QColor("#a0a0a0"));
    pal.setColor(QPalette::Text, QColor("#e7e7e7"));
    pal.setColor(QPalette::BrightText, QColor("#ff0000"));
    pal.setColor(QPalette::ButtonText, QColor("#e7e7e7"));
    pal.setColor(QPalette::Base, QColor("#3C3C3C"));
    pal.setColor(QPalette::Window, QColor("#3C3C3C"));
    pal.setColor(QPalette::Shadow, QColor("#696969"));
    pal.setColor(QPalette::Highlight, QColor("#9F3740"));
    pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    pal.setColor(QPalette::Link, QColor("#007af4"));
    pal.setColor(QPalette::LinkVisited, QColor("#a57aff"));
    pal.setColor(QPalette::AlternateBase, QColor("#515151"));
    pal.setColor(QPalette::NoRole, QColor("#000000"));
    pal.setColor(QPalette::ToolTipBase, QColor("#aaaaaa"));
    pal.setColor(QPalette::ToolTipText, QColor("#e7e7e7"));

    pal.setColor(QPalette::Inactive, QPalette::WindowText, QColor("#e7e7e7"));
    pal.setColor(QPalette::Inactive, QPalette::Button, QColor("#232323"));
    pal.setColor(QPalette::Inactive, QPalette::Light, QColor("#484848"));
    pal.setColor(QPalette::Inactive, QPalette::Midlight, QColor("#808080"));
    pal.setColor(QPalette::Inactive, QPalette::Dark, QColor("#b8b8b8"));
    pal.setColor(QPalette::Inactive, QPalette::Mid, QColor("#a0a0a0"));
    pal.setColor(QPalette::Inactive, QPalette::Text, QColor("#e7e7e7"));
    pal.setColor(QPalette::Inactive, QPalette::BrightText, QColor("#ff0000"));
    pal.setColor(QPalette::Inactive, QPalette::ButtonText, QColor("#e7e7e7"));
    pal.setColor(QPalette::Inactive, QPalette::Base, QColor("#3C3C3C"));
    pal.setColor(QPalette::Inactive, QPalette::Window, QColor("#3C3C3C"));
    pal.setColor(QPalette::Inactive, QPalette::Shadow, QColor("#696969"));
    pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor("#DC4150"));
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor("#ffffff"));
    pal.setColor(QPalette::Inactive, QPalette::Link, QColor("#007af4"));
    pal.setColor(QPalette::Inactive, QPalette::LinkVisited, QColor("#a57aff"));
    pal.setColor(QPalette::Inactive, QPalette::AlternateBase, QColor("#515151"));
    pal.setColor(QPalette::Inactive, QPalette::NoRole, QColor("#000000"));
    pal.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor("#aaaaaa"));
    pal.setColor(QPalette::Inactive, QPalette::ToolTipText, QColor("#e7e7e7"));

    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#606060"));
    pal.setColor(QPalette::Disabled, QPalette::Button, QColor("#232323"));
    pal.setColor(QPalette::Disabled, QPalette::Light, QColor("#606060"));
    pal.setColor(QPalette::Disabled, QPalette::Midlight, QColor("#404040"));
    pal.setColor(QPalette::Disabled, QPalette::Dark, QColor("#202020"));
    pal.setColor(QPalette::Disabled, QPalette::Mid, QColor("#a0a0a0"));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#606060"));
    pal.setColor(QPalette::Disabled, QPalette::BrightText, QColor("#ff0000"));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#e7e7e7"));
    pal.setColor(QPalette::Disabled, QPalette::Base, QColor("#444444"));
    pal.setColor(QPalette::Disabled, QPalette::Window, QColor("#3C3C3C"));
    pal.setColor(QPalette::Disabled, QPalette::Shadow, QColor("#000000"));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#ffffff"));
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#ffffff"));
    pal.setColor(QPalette::Disabled, QPalette::Link, QColor("#007af4"));
    pal.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor("#a57aff"));
    pal.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor("#515151"));
    pal.setColor(QPalette::Disabled, QPalette::NoRole, QColor("#000000"));
    pal.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor("#aaaaaa"));
    pal.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor("#e7e7e7"));

    qApp->setPalette(pal);
    m_isLightStyle = false;
}

void StyleManager::switchToLight() {
    qApp->setStyle(QStyleFactory::create("fusion"));

    QPalette pal = qApp->style()->standardPalette();
    pal.setColor(QPalette::Window, QColor("#f6f6f6"));
    pal.setColor(QPalette::WindowText, QColor("#545657"));
    pal.setColor(QPalette::Button, QColor("#e4e4e4"));
    pal.setColor(QPalette::ButtonText, QColor("#545657"));
    pal.setColor(QPalette::Base, QColor("#ffffff"));
    pal.setColor(QPalette::AlternateBase, QColor("#eeeeee"));
    pal.setColor(QPalette::Text, QColor("#545657"));
    pal.setColor(QPalette::Highlight, QColor("#DC7A84"));
    pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    pal.setColor(QPalette::Light, QColor("#fafafa"));
    pal.setColor(QPalette::Midlight, QColor("#d6d6d6"));
    pal.setColor(QPalette::Dark, QColor("#AFAFAF"));
    pal.setColor(QPalette::Mid, QColor("#a0a2a4"));
    pal.setColor(QPalette::Shadow, QColor("#585a5c"));

    qApp->setPalette(pal);
    m_isLightStyle = true;
}
