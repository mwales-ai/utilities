#include "WordSplitter.h"
#include "Dictionary.h"

#include <iostream>

#define SplitDebug if(0) std::cout

WordSplitter::WordSplitter(const Dictionary *tech, const Dictionary *system)
    : theTech(tech)
    , theSystem(system)
    , theMinWordLength(2)
{
}

void WordSplitter::setMinWordLength(int n)
{
    theMinWordLength = n;
}

bool WordSplitter::isWord(const QString & candidate) const
{
    // Single-letter "words" are usually noise from the system dict
    // (every letter shows up as a Roman numeral or initial). Allow only
    // the well-known one-letter English words.
    if (candidate.length() == 1)
        return candidate == "a" || candidate == "i";

    if (candidate.length() < theMinWordLength)
        return false;

    if (theTech && theTech->contains(candidate))
        return true;
    if (theSystem && theSystem->contains(candidate))
        return true;
    return false;
}

QStringList WordSplitter::split(const QString & input) const
{
    QStringList result;
    QString s = input.toLower();
    int n = s.length();
    int i = 0;

    while (i < n)
    {
        bool found = false;
        // Greedy longest match: try j from end down to i+1
        for (int j = n; j > i; --j)
        {
            QString candidate = s.mid(i, j - i);
            if (isWord(candidate))
            {
                result.append(candidate);
                SplitDebug << "  match: " << candidate.toStdString() << std::endl;
                i = j;
                found = true;
                break;
            }
        }
        if (!found)
        {
            SplitDebug << "  no match at position " << i
                       << " for '" << s.toStdString() << "'" << std::endl;
            return QStringList();
        }
    }

    return result;
}
