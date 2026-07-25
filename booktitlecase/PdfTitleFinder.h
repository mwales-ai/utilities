#ifndef PDFTITLEFINDER_H
#define PDFTITLEFINDER_H

#include <QString>
#include <QList>
#include <QVector>

/** One way the title appears spelled out in the document body.
 *  e.g. text="Animal Farm", count=3, firstPos=86 (index in extracted text). */
struct TitleCandidate
{
    QString  text;      // original spelling/spacing/casing from the book
    int      count;     // how many times this exact spelling was found
    int      firstPos;  // character offset of the earliest occurrence
};

/** Recovers the correct spacing of a book title from the PDF's own body text.
 *
 *  The idea: many PDFs carry a wrong /Title in their metadata, but the real
 *  title is spelled out on the title page and in running headers. The filename
 *  is usually the run-together title ("animalfarm"), so if we strip whitespace
 *  and punctuation from both the filename and the extracted text and look for a
 *  match, the matching region of the ORIGINAL text tells us the real spacing
 *  ("Animal Farm").
 *
 *  Returns distinct spellings ranked by frequency (then by earliest position),
 *  so the caller can pick a winner or detect a conflict (a tie at the top). */
class PdfTitleFinder
{
public:
    PdfTitleFinder();

    void  setMaxPages(int n);

    QList<TitleCandidate>  findCandidates(const QString & pdfPath,
                                          const QString & filenameStem) const;

private:
    QString  extractText(const QString & pdfPath) const;

    // Build a lowercase, alphanumeric-only version of s. When origIndex is
    // given, it receives the original character offset for each kept char.
    QString  normalizeKeepAlnum(const QString & s,
                                QVector<int> *origIndex) const;

    int  theMaxPages;   // only scan this many leading pages (titles live early)
};

#endif
