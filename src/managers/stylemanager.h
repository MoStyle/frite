/*
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QObject>
#include <QIcon>

class StyleManager : public QObject {
    Q_OBJECT
   public:
    StyleManager(QObject* parent = 0);

    virtual ~StyleManager() {}

    bool isLightStyle() const { return m_isLightStyle; }
    void setDarkStyle(bool b) { m_isLightStyle = b; }

    QString getResourcePath(const QString& name);
    QIcon getIcon(const QString& name);

    void switchToDark();
    void switchToLight();

   private:
    bool m_isLightStyle;
};

#endif  // STYLEMANAGER_H