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

    QStringList nameFilters;
    for (int i = 0; i < theExtensions.size(); ++i)
        nameFilters << QString("*.%1").arg(theExtensions.at(i));

    QFileInfoList entries = dir.entryInfoList(nameFilters,
                                              QDir::Files | QDir::NoDotAndDotDot,
                                              QDir::Name);

    // Group by lowercased stem so case differences merge into one entry.
    QMap<QString, BookGroup> groups;

    for (int i = 0; i < entries.size(); ++i)
    {
        const QFileInfo & fi = entries.at(i);
        QString stem = fi.completeBaseName();
        QString ext  = fi.suffix().toLower();
        QString key  = stem.toLower();

        if (!groups.contains(key))
        {
            BookGroup g;
            g.directory     = dir.absolutePath();
            g.originalStem  = stem;
            g.checked       = true;
            g.splitOk       = false;
            groups.insert(key, g);
        }

        BookGroup & g = groups[key];
        g.extensions.append(ext);
        g.originalNames.append(fi.fileName());
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
