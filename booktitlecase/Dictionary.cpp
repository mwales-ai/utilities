#include "Dictionary.h"

#include <QFile>
#include <QTextStream>

#include <iostream>

#define DictDebug if(0) std::cout

Dictionary::Dictionary()
{
}

bool Dictionary::load(const QString & path)
{
    theWords.clear();
    thePath = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        DictDebug << "Dictionary: failed to open " << path.toStdString() << std::endl;
        return false;
    }

    QTextStream in(&f);
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed().toLower();
        if (line.isEmpty())
            continue;
        if (line.startsWith('#'))
            continue;
        theWords.insert(line);
    }

    DictDebug << "Dictionary: loaded " << theWords.size()
              << " words from " << path.toStdString() << std::endl;
    return true;
}

bool Dictionary::contains(const QString & word) const
{
    return theWords.contains(word.toLower());
}

int Dictionary::wordCount() const
{
    return theWords.size();
}

QString Dictionary::path() const
{
    return thePath;
}
