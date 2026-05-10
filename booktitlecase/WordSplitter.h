#ifndef WORDSPLITTER_H
#define WORDSPLITTER_H

#include <QString>
#include <QStringList>

class Dictionary;

/** Splits a run-together lowercase string ("animalfarm") into a list of
 *  words ("animal", "farm") using two dictionaries.
 *
 *  Tech dictionary is consulted first, then the system dictionary.
 *  The splitter is greedy from the left, picking the longest word at each
 *  position. Returns an empty list if no full split is possible. */
class WordSplitter
{
public:
    WordSplitter(const Dictionary *tech, const Dictionary *system);

    QStringList split(const QString & input) const;

    void setMinWordLength(int n);

private:
    bool isWord(const QString & candidate) const;

    const Dictionary *theTech;
    const Dictionary *theSystem;
    int               theMinWordLength;
};

#endif
