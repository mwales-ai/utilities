#include "FileScanner.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>

#include <iostream>

#define ScanDebug if(0) std::cout

FileScanner::FileScanner()
{
    theExtensions << "pdf" << "epub" << "mobi";
}

void FileScanner::setExtensions(const QStringList & exts)
{
    theExtensions = exts;
}

QList<BookGroup> FileScanner::scan(const QString & dirPath) const
{
    QList<BookGroup> result;

    QDir dir(dirPath);
    if (!dir.exists())
    {
        ScanDebug << "FileScanner: dir doesn't exist: "
                  << dirPath.toStdString() << std::endl;
        return result;
    }

    // Everything in the folder, not just the book extensions — companion files
    // (zips of source code, errata, covers) are matched by base name below.
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot,
                                              QDir::Name);

    // Group by lowercased stem so case differences merge into one entry.
    QMap<QString, BookGroup> groups;
    QStringList claimed;   // filenames already placed in a group

    // First pass: only a recognized book extension starts a group.
    for (int i = 0; i < entries.size(); ++i)
    {
        const QFileInfo & fi = entries.at(i);
        QString ext  = fi.suffix().toLower();
        if (!theExtensions.contains(ext))
            continue;

        QString stem = fi.completeBaseName();
        QString key  = stem.toLower();

        if (!groups.contains(key))
        {
            BookGroup g;
            g.directory     = dir.absolutePath();
            g.originalStem  = stem;
            g.checked          = true;
            g.splitOk          = false;
            g.fromMetadata     = false;
            g.contentConflict  = false;
            g.fromContent      = false;
            groups.insert(key, g);
        }

        BookGroup & g = groups[key];
        g.extensions.append(ext);
        g.originalNames.append(fi.fileName());
        claimed.append(fi.fileName());
    }

    // Second pass: any other file sharing a group's base name rides along, so
    // it gets renamed with the book. Longest matching stem wins, so
    // "animalfarm.part2.zip" prefers an "animalfarm.part2" group over
    // "animalfarm" when both exist.
    for (int i = 0; i < entries.size(); ++i)
    {
        QString name = entries.at(i).fileName();
        if (claimed.contains(name))
            continue;

        QString lowerName = name.toLower();
        QString bestKey;

        QMapIterator<QString, BookGroup> it(groups);
        while (it.hasNext())
        {
            it.next();
            if (!lowerName.startsWith(it.key() + "."))
                continue;
            if (it.key().size() > bestKey.size())
                bestKey = it.key();
        }

        if (bestKey.isEmpty())
            continue;

        // Everything past the stem is the extension, so ".tar.gz" or
        // ".Source Code.zip" stays whole. Case is kept as-is here: unlike a
        // plain book extension, the tail can carry meaningful wording.
        QString companionExt = name.mid(bestKey.size() + 1);

        BookGroup & g = groups[bestKey];
        g.extensions.append(companionExt);
        g.originalNames.append(name);
        claimed.append(name);

        ScanDebug << "FileScanner: companion " << name.toStdString()
                  << " -> group " << bestKey.toStdString() << std::endl;
    }

    QMapIterator<QString, BookGroup> it(groups);
    while (it.hasNext())
    {
        it.next();
        result.append(it.value());
    }

    ScanDebug << "FileScanner: found " << result.size()
              << " groups in " << dirPath.toStdString() << std::endl;
    return result;
}
