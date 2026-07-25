#ifndef METADATAEXTRACTOR_H
#define METADATAEXTRACTOR_H

#include <QString>
#include <QByteArray>

/** Reads the real book title from inside a PDF or EPUB document.
 *
 *  EPUB: an epub is a zip archive. We read META-INF/container.xml to find the
 *        OPF package file, then read the <dc:title> element out of it.
 *  PDF:  we ask pdfinfo for the Title, and if that is unavailable we scan the
 *        raw bytes for the /Title entry of the Info dictionary (both the
 *        literal "(...)" and hex "<...>" string forms).
 *
 *  Every method returns an empty string when no title can be found, so the
 *  caller can fall back to guessing a title from the filename. */
class MetadataExtractor
{
public:
    MetadataExtractor();

    /** Dispatch on file extension. Empty result means "no embedded title". */
    QString  extractTitle(const QString & filePath) const;

    QString  extractEpubTitle(const QString & path) const;
    QString  extractPdfTitle(const QString & path) const;

private:
    // EPUB helpers
    QByteArray  readZipEntry(const QString & archivePath,
                             const QString & entryName) const;
    QString     opfPathFromContainer(const QByteArray & containerXml) const;
    QString     titleFromOpf(const QByteArray & opfXml) const;

    // PDF helpers
    QString     pdfTitleViaPdfinfo(const QString & path) const;
    QString     pdfTitleFromBytes(const QByteArray & data) const;
    QString     decodePdfLiteral(const QByteArray & raw) const;
    QString     decodePdfHex(const QByteArray & hex) const;
    QString     decodePdfBytes(const QByteArray & bytes) const;

    // General helpers
    QByteArray  runProcess(const QString & program,
                           const QStringList & args) const;
    QString     collapseWhitespace(const QString & s) const;
};

#endif
