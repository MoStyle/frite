/*
 * SPDX-FileCopyrightText: 2005-2007 Patrick Corrieri & Pascal Naidon
 * SPDX-FileCopyrightText: 2013-2014 Matt Chiawen Chang
 * SPDX-FileCopyrightText: 2018-2023 Pierre Benard <pierre.g.benard@inria.fr>
 * SPDX-FileCopyrightText: 2021-2023 Melvin Even <melvin.even@inria.fr>
 *
 * SPDX-License-Identifier: CECILL-2.1
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filemanager.h"
#include "editor.h"
#include "dialsandknobs.h"
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
    m_mainXMLFile = filename;

    QFile file(m_mainXMLFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to load \"%s\".", qPrintable(m_mainXMLFile));
        return false;
    }
    m_currentFileName = QFileInfo(m_filePath).baseName();

    QDomDocument xmlDoc;
    QString parse_errors;
    if (!xmlDoc.setContent(&file, &parse_errors)) {
        qWarning("Failed to parse \"%s\".", qPrintable(m_mainXMLFile));
        return false;
    }
    file.close();

    QDomDocumentType type = xmlDoc.doctype();
    if (!(type.name() == "FriteDocument")) {
        qWarning("Invalid Frite project");
        return false;
    }

    QDomElement root = xmlDoc.documentElement();
    if (root.isNull()) {
        qWarning("Invalid Frite project");
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

    m_mainXMLFile = filename;

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

    return true;
}
