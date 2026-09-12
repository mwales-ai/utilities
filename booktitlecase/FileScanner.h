#ifndef FILESCANNER_H
#define FILESCANNER_H

#include "BookGroup.h"

#include <QString>
#include <QStringList>
#include <QList>

/** Scan a directory for book files and group them by stem (case-insensitive).
 *  A group is started by a recognized book extension (pdf, epub, mobi); any
 *  other file with the same base name — a zip of source code, errata, a cover —
 *  is added to that group so it gets renamed along with the book. */
class FileScanner
{
public:
    FileScanner();

    void                  setExtensions(const QStringList & exts);
    QList<BookGroup>      scan(const QString & dirPath) const;

private:
    QStringList theExtensions;
};

#endif
