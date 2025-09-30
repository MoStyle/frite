/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2008-2009 Mj Mendoza IV
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2024 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtWidgets>
#include <QSurfaceFormat>
#include <QtGlobal>
#include <QtDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <iostream>

#include "mainwindow.h"
#include "tabletapplication.h"
#include "tabletcanvas.h"

// QFile outFile(QString("log_") + QString(QDateTime::currentDateTime().toString("dd-MM-hh-mm-ss").toLocal8Bit()) + QString(".txt"));

// void messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
// {
//     QString txt;
//     switch (type) {
//     case QtDebugMsg:
//         txt = QString("%1 [%2]").arg(msg, QString(QDateTime::currentDateTime().toString("hh:mm:ss").toLocal8Bit()));
//         break;
//     case QtWarningMsg:
//         txt = QString("Warning: %1").arg(msg);
//         break;
//     case QtCriticalMsg:
//         txt = QString("Critical: %1").arg(msg);
//         break;
//     case QtFatalMsg:
//         txt = QString("Fatal: %1").arg(msg);
//         abort();
//     }

//     outFile.open(QIODevice::WriteOnly | QIODevice::Append);
//     QTextStream ts(&outFile);
//     ts << txt << Qt::endl;
//     outFile.close();
// }

int main(int argv, char *args[]) {
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(8);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    // qInstallMessageHandler(messageOutput);

    TabletApplication app(argv, args);
    TabletCanvas *canvas = new TabletCanvas;
    app.setCanvas(canvas);

    QIcon icon;
    icon.addFile(QStringLiteral(":/images/fries.png"), QSize(), QIcon::Normal, QIcon::Off);
    app.setWindowIcon(icon);

    MainWindow mainWindow(canvas);
    mainWindow.show();
    return app.exec();
}
