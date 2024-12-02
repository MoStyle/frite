/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2013-2014 Matt Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation;

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

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
