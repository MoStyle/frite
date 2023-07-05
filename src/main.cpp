/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2008-2009 Mj Mendoza IV
 * SPDX-FileCopyrightText: 2012-2014 Matthew Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtWidgets>
#include <QSurfaceFormat>

#include "mainwindow.h"
#include "tabletapplication.h"
#include "tabletcanvas.h"

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    if (MainWindow::s_textEdit == nullptr || type == QtFatalMsg) {
        QByteArray localMsg = msg.toLocal8Bit();
        switch (type) {
            case QtInfoMsg:
                fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
                break;
            case QtDebugMsg:
                fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
                break;
            case QtWarningMsg:
                fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
                break;
            case QtCriticalMsg:
                fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
                break;
            case QtFatalMsg:
                fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
                abort();
        }
    } else {
        switch (type) {
            case QtInfoMsg:
            case QtDebugMsg:
                MainWindow::s_textEdit->append("<font color=grey>" + QTime::currentTime().toString("H:mm:ss.zzz") + "</font>  " + msg);
                break;
            case QtWarningMsg:
                MainWindow::s_textEdit->append("<font color=grey>" + QTime::currentTime().toString("H:mm:ss.zzz") + "</font>  <font color=orange>Warning:</font> " + msg);
                break;
            case QtCriticalMsg:
                MainWindow::s_textEdit->append("<font color=grey>" + QTime::currentTime().toString("H:mm:ss.zzz") + "</font>  <font color=red>Critical:</font> " + msg);
                break;
            default:
                break;
        }
    }
}

int main(int argv, char *args[]) {
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(8);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    qInstallMessageHandler(myMessageOutput);

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
