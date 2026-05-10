#include "TitleCaser.h"

TitleCaser::TitleCaser()
{
    QStringList defaults = defaultSmallWords();
    for (int i = 0; i < defaults.size(); ++i)
        theSmallWords.insert(defaults.at(i).toLower());
}

void TitleCaser::setSmallWords(const QStringList & smallWords)
{
    theSmallWords.clear();
    for (int i = 0; i < smallWords.size(); ++i)
        theSmallWords.insert(smallWords.at(i).toLower());
}

QStringList TitleCaser::defaultSmallWords()
{
    QStringList list;
    list << "a"   << "an"  << "and" << "as"  << "at"
         << "but" << "by"  << "for" << "if"  << "in"
         << "nor" << "of"  << "on"  << "or"  << "per"
         << "the" << "to"  << "up"  << "via" << "vs";
    return list;
}

QString TitleCaser::capitalize(const QString & word) const
{
    if (word.isEmpty())
        return word;
    QString out = word.toLower();
    out[0] = out[0].toUpper();
    return out;
}

QString TitleCaser::titleCase(const QStringList & words) const
{
    if (words.isEmpty())
        return QString();

    QStringList out;
    int last = words.size() - 1;

    for (int i = 0; i < words.size(); ++i)
    {
        QString w = words.at(i).toLower();
        bool isSmall = theSmallWords.contains(w);
        bool isFirstOrLast = (i == 0) || (i == last);

        if (isSmall && !isFirstOrLast)
            out.append(w);
        else
            out.append(capitalize(w));
    }

    return out.join(" ");
}
