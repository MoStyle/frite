/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2013-2014 Matt Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OBJECTSAVELOADER_H
#define OBJECTSAVELOADER_H

#include <QObject>
#include <QString>
#include <QDomElement>

class Editor;
class DialsAndKnobs;

class FileManager : public QObject
{
    Q_OBJECT

public:
    FileManager(QObject* parent = 0);

    bool load(const QString& fileName, Editor *editor, DialsAndKnobs *dk);
    bool save(const QString& filename, Editor *editor, DialsAndKnobs *dk);
    
    void createWorkingDir();
    void deleteWorkingDir();

    void resetFileName() { m_currentFileName = "untitled"; }

    QString fileName() const { return m_currentFileName; }
    QString filePath() const { return m_filePath; }

private:
    bool removeTmpDirectory(const QString &dirName);
    void unzip(const QString& strZipFile, const QString& strUnzipTarget);

private:
    QString m_currentFileName;
    QString m_lastTempFolder;

    QString m_filePath;       //< where this object come from. (empty if new project)
    QString m_workingDirPath; //< the folder that pclx will uncompress to.
    QString m_dataDirPath;    //< the folder which contains all bitmap & vector image & sound files.
    QString m_mainXMLFile;    //< the location of main.xml
};

#endif // OBJECTSAVELOADER_H
