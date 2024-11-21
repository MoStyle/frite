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

#include "filemanager.h"
#include "editor.h"
#include "dialsandknobs.h"
#include <JlCompress.h>
#include <QDomElement>
#include <QDebug>

FileManager::FileManager(QObject *parent)
    :  QObject(parent)
    , m_currentFileName("untitled")
{
}

bool FileManager::load(const QString& filename, Editor *editor, DialsAndKnobs *dk)
{
    if (!QFile::exists(filename)) {
        qWarning("Failed to load \"%s\".", qPrintable(filename));
        return false;
    }

    m_filePath = filename;
    if(m_filePath.endsWith(".fries")) {
        createWorkingDir();
        unzip(filename, m_workingDirPath);
    } else {
        m_mainXMLFile = filename;
    }

    QFile file(m_mainXMLFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to load \"%s\".", qPrintable(m_mainXMLFile));
        removeTmpDirectory(m_lastTempFolder);
        return false;
    }
    m_currentFileName = QFileInfo(m_filePath).baseName();

    QDomDocument xmlDoc;
    QString parse_errors;
    if (!xmlDoc.setContent(&file, &parse_errors)) {
        qWarning("Failed to parse \"%s\".", qPrintable(m_mainXMLFile));
        removeTmpDirectory(m_lastTempFolder);
        return false;
    }
    file.close();

    QDomDocumentType type = xmlDoc.doctype();
    if (!(type.name() == "FriteDocument")) {
        qWarning("Invalid Frite project");
        removeTmpDirectory(m_lastTempFolder);
        return false;
    }

    QDomElement root = xmlDoc.documentElement();
    if (root.isNull()) {
        qWarning("Invalid Frite project");
        removeTmpDirectory(m_lastTempFolder);
        return false;
    }

    QDomElement editorElt = root.firstChildElement("editor");

    if (!editor->load(editorElt, m_dataDirPath)) {
        qWarning("Failed to load \"%s\".", qPrintable(filename));
    }

    QDomElement dkElt = root.firstChildElement("dials_and_knobs");
    if (dkElt.isNull()) {
        qWarning("Open project: no dials_and_knobs node found.\n");
    } else {
        dk->load(dkElt);
    }

    return true;
}

bool FileManager::save(const QString& filename, Editor *editor, DialsAndKnobs *dk)
{
    QFileInfo fileInfo(filename);
    if (fileInfo.isDir()) {
         qWarning("The file path you have specified (\"%s\") points to a directory, so the file cannot be saved.",qPrintable(fileInfo.absoluteFilePath()));
         return false;
    }
    if (fileInfo.exists() && !fileInfo.isWritable()) {
        qWarning("The file path you have specified (\"%s\") cannot be written to, so the file cannot be saved. Please make sure that you have sufficent permissions to save to that location and try again.",qPrintable(fileInfo.absoluteFilePath()));
        return false;
    }

    if(filename.endsWith(".fries")) {
        if(!QDir(m_workingDirPath).exists()) {
            createWorkingDir();
        }

        QFileInfo dataInfo(m_dataDirPath);
        if (!dataInfo.exists()) {
            QDir dir(m_dataDirPath); // the directory where filePath is or will be saved
            // creates a directory with the same name +".data"
            if(!dir.mkpath(m_dataDirPath)) {
                qWarning("Cannot create the data directory at temporary location \"%s\". Please make sure that you have sufficent permissions to save to that location and try again.", qPrintable(m_dataDirPath));
                return false;
            }
        }
        if(!dataInfo.isDir()) {
            qWarning("Cannot use the data directory at temporary location \"%s\" since it is a file. Please move or delete that file and try again.",qPrintable(dataInfo.absoluteFilePath()));
            return false;
        }
    } else {
        m_mainXMLFile = filename;
    }

    // Save main XML file
    QScopedPointer<QFile> file(new QFile(m_mainXMLFile));
    if (!file->open(QFile::WriteOnly | QFile::Text)) {
        qWarning("Failed to load \"%s\".", qPrintable(m_mainXMLFile));
        return false;
    }

    m_currentFileName = fileInfo.baseName();

    QDomDocument xmlDoc("FriteDocument");
    QDomElement root = xmlDoc.createElement("document");
    xmlDoc.appendChild(root);

    // Save editor and layers
    editor->save(xmlDoc, root, m_dataDirPath);

    // Save dials and knobs
    dk->save(xmlDoc, root);

    QTextStream out(file.data());

    const int indentSize = 2;
    xmlDoc.save(out, indentSize);

    if(filename.endsWith(".fries")) {
        if (!JlCompress::compressDir(filename, m_workingDirPath))
        {
            qWarning("Failed to compress file.");
            return false;
        }
    }

    m_filePath = filename;

    return true;
}

void FileManager::createWorkingDir()
{
    QString strFolderName;
    if (m_filePath.isEmpty()) {
        strFolderName = "Default";
    } else {
        QFileInfo fileInfo(m_filePath);
        strFolderName = fileInfo.completeBaseName();
    }
    QString strWorkingDir = QDir::tempPath() + "/Frite/" + strFolderName + ".Y2xD/";

    QDir dir(QDir::tempPath());
    dir.mkpath(strWorkingDir);

    m_workingDirPath = strWorkingDir;

    QDir dataDir(strWorkingDir + "data");
    dataDir.mkpath(".");

    m_dataDirPath = dataDir.absolutePath();
    m_mainXMLFile = QDir(m_workingDirPath).filePath("main.xml");
}

void FileManager::deleteWorkingDir()
{
    QDir dataDir(m_workingDirPath);
    dataDir.removeRecursively();
}

bool FileManager::removeTmpDirectory(const QString& dirName)
{
    if (dirName.isEmpty()) {
        return false;
    }

    QDir dir(dirName);

    if (!dir.exists()) {
        return false;
    }

    bool result = dir.removeRecursively();

    return result;
}

void FileManager::unzip(const QString& strZipFile, const QString& strUnzipTarget)
{
    // --removes an old decompression directory first
    removeTmpDirectory(strUnzipTarget);

    // --creates a new decompression directory
    JlCompress::extractDir(strZipFile, strUnzipTarget);

    m_lastTempFolder = strUnzipTarget;
}
