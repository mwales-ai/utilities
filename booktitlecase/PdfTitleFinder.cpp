#include "PdfTitleFinder.h"

#include <QProcess>
#include <QMap>
#include <QChar>

#include <iostream>

#define FindDebug if(0) std::cout

// Titles reliably appear within the first pages (cover, title page, early
// running headers), so we cap the text extraction for speed on big libraries.
static const int DEFAULT_MAX_PAGES = 25;

static bool isAlnum(QChar c)
{
    return c.isLetterOrNumber();
}

PdfTitleFinder::PdfTitleFinder()
    : theMaxPages(DEFAULT_MAX_PAGES)
{
}

void PdfTitleFinder::setMaxPages(int n)
{
    theMaxPages = n;
}

QString PdfTitleFinder::extractText(const QString & pdfPath) const
{
    // "pdftotext -l N ... file -" writes the first N pages of text to stdout,
    // decompressing content streams for us.
    QStringList args;
    args << "-l" << QString::number(theMaxPages)
         << "-enc" << "UTF-8"
         << "-q"
         << pdfPath << "-";

    QProcess proc;
    proc.start("pdftotext", args);
    if (!proc.waitForStarted(3000))
    {
        FindDebug << "pdftotext not available" << std::endl;
        return QString();
    }
    if (!proc.waitForFinished(20000))
    {
        proc.kill();
        proc.waitForFinished(2000);
        FindDebug << "pdftotext timed out on " << pdfPath.toStdString() << std::endl;
        return QString();
    }

    return QString::fromUtf8(proc.readAllStandardOutput());
}

QString PdfTitleFinder::normalizeKeepAlnum(const QString & s,
                                           QVector<int> *origIndex) const
{
    QString out;
    out.reserve(s.size());
    for (int i = 0; i < s.size(); ++i)
    {
        QChar c = s.at(i);
        if (isAlnum(c))
        {
            out.append(c.toLower());
            if (origIndex)
                origIndex->append(i);
        }
    }
    return out;
}

QList<TitleCandidate> PdfTitleFinder::findCandidates(const QString & pdfPath,
                                                     const QString & filenameStem) const
{
    QList<TitleCandidate> result;

    QString needle = normalizeKeepAlnum(filenameStem, nullptr);
    if (needle.isEmpty())
        return result;

    QString text = extractText(pdfPath);
    if (text.isEmpty())
        return result;

    QVector<int> idx;
    QString norm = normalizeKeepAlnum(text, &idx);

    int nlen = needle.length();

    // Group identical spellings together; QMap keeps them ordered but we
    // re-sort by frequency below, so ordering here does not matter.
    QMap<QString, TitleCandidate> found;

    int from = 0;
    while (true)
    {
        int p = norm.indexOf(needle, from);
        if (p < 0)
            break;

        int os = idx.at(p);
        int oe = idx.at(p + nlen - 1);

        // Word-boundary guard: the characters just outside the match in the
        // ORIGINAL text must not be alphanumeric, so we don't match a filename
        // that merely sits inside a longer word ("art" inside "Bartholomew").
        bool leftOk  = (os == 0)              || !isAlnum(text.at(os - 1));
        bool rightOk = (oe + 1 >= text.size()) || !isAlnum(text.at(oe + 1));

        if (leftOk && rightOk)
        {
            // Collapse internal whitespace/newlines so a title split across two
            // lines ("Animal\nFarm") becomes a clean "Animal Farm".
            QString orig = text.mid(os, oe - os + 1).simplified();
            if (!orig.isEmpty())
            {
                if (found.contains(orig))
                {
                    found[orig].count++;
                }
                else
                {
                    TitleCandidate cand;
                    cand.text     = orig;
                    cand.count    = 1;
                    cand.firstPos = os;
                    found.insert(orig, cand);
                }
            }
        }

        from = p + nlen;
    }

    result = found.values();

    // Rank: most frequent first, breaking ties by earliest appearance.
    for (int i = 0; i < result.size(); ++i)
    {
        for (int j = i + 1; j < result.size(); ++j)
        {
            bool swap = false;
            if (result.at(j).count > result.at(i).count)
                swap = true;
            else if (result.at(j).count == result.at(i).count &&
                     result.at(j).firstPos < result.at(i).firstPos)
                swap = true;

            if (swap)
                result.swapItemsAt(i, j);
        }
    }

    FindDebug << "PdfTitleFinder: '" << filenameStem.toStdString() << "' -> "
              << result.size() << " candidate(s)" << std::endl;
    return result;
}
