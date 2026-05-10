#ifndef FILESCANNER_H
#define FILESCANNER_H

#include "BookGroup.h"

#include <QString>
#include <QStringList>
#include <QList>

/** Scan a directory for book files and group them by stem (case-insensitive).
 *  Recognized extensions: pdf, epub, mobi. */
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
