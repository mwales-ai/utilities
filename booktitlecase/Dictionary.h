#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <QString>
#include <QSet>

/** Loads a flat one-word-per-line dictionary file into a fast lookup set.
 *  All lookups are case-insensitive (words are stored lowercased). */
class Dictionary
{
public:
    Dictionary();

    bool      load(const QString & path);
    bool      contains(const QString & word) const;
    int       wordCount() const;
    QString   path() const;

private:
    QSet<QString>  theWords;
    QString        thePath;
};

#endif
