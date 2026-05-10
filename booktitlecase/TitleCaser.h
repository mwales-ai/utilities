#ifndef TITLECASER_H
#define TITLECASER_H

#include <QString>
#include <QStringList>
#include <QSet>

/** Capitalize a list of words to title case.
 *
 *  Small words (a, an, and, of, the, ...) are left lowercase EXCEPT when
 *  they appear as the first or last word of the title. */
class TitleCaser
{
public:
    TitleCaser();

    void     setSmallWords(const QStringList & smallWords);
    QString  titleCase(const QStringList & words) const;

    static QStringList defaultSmallWords();

private:
    QString  capitalize(const QString & word) const;

    QSet<QString> theSmallWords;
};

#endif
