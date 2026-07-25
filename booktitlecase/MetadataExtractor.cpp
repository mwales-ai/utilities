#include "MetadataExtractor.h"

#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QStringList>

#include <iostream>

#define MetaDebug if(0) std::cout

// PDF whitespace per the spec: space, tab, CR, LF, form feed, null.
static bool isPdfWhitespace(char c)
{
    return c == ' '  || c == '\t' || c == '\r'
        || c == '\n' || c == '\f' || c == '\0';
}

static bool isHexDigit(char c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

MetadataExtractor::MetadataExtractor()
{
}

QString MetadataExtractor::extractTitle(const QString & filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext == "epub")
        return extractEpubTitle(filePath);
    if (ext == "pdf")
        return extractPdfTitle(filePath);

    return QString();
}

// ---------------------------------------------------------------------------
// EPUB
// ---------------------------------------------------------------------------

QString MetadataExtractor::extractEpubTitle(const QString & path) const
{
    QByteArray container = readZipEntry(path, "META-INF/container.xml");
    if (container.isEmpty())
    {
        MetaDebug << "epub: no container.xml in " << path.toStdString() << std::endl;
        return QString();
    }

    QString opfPath = opfPathFromContainer(container);
    if (opfPath.isEmpty())
        return QString();

    QByteArray opf = readZipEntry(path, opfPath);
    if (opf.isEmpty())
        return QString();

    QString title = collapseWhitespace(titleFromOpf(opf));
    MetaDebug << "epub title: " << title.toStdString() << std::endl;
    return title;
}

QByteArray MetadataExtractor::readZipEntry(const QString & archivePath,
                                           const QString & entryName) const
{
    // "unzip -p" writes a single archive member straight to stdout.
    QStringList args;
    args << "-p" << archivePath << entryName;
    return runProcess("unzip", args);
}

QString MetadataExtractor::opfPathFromContainer(const QByteArray & containerXml) const
{
    QXmlStreamReader xml(containerXml);
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("rootfile"))
        {
            QString fullPath = xml.attributes().value("full-path").toString();
            if (!fullPath.isEmpty())
                return fullPath;
        }
    }
    return QString();
}

QString MetadataExtractor::titleFromOpf(const QByteArray & opfXml) const
{
    // dc:title lives in the Dublin Core namespace; QXmlStreamReader::name()
    // returns the local name "title" regardless of the prefix used.
    QXmlStreamReader xml(opfXml);
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("title"))
            return xml.readElementText(QXmlStreamReader::IncludeChildElements);
    }
    return QString();
}

// ---------------------------------------------------------------------------
// PDF
// ---------------------------------------------------------------------------

QString MetadataExtractor::extractPdfTitle(const QString & path) const
{
    // pdfinfo understands compressed metadata and XMP, so try it first.
    QString viaTool = pdfTitleViaPdfinfo(path);
    if (!viaTool.isEmpty())
        return viaTool;

    // Fallback: scan the raw bytes ourselves (no external dependency).
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QByteArray data = f.readAll();
    f.close();

    return pdfTitleFromBytes(data);
}

QString MetadataExtractor::pdfTitleViaPdfinfo(const QString & path) const
{
    QStringList args;
    args << "-enc" << "UTF-8" << path;
    QByteArray out = runProcess("pdfinfo", args);
    if (out.isEmpty())
        return QString();

    QString text = QString::fromUtf8(out);
    QStringList lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i)
    {
        const QString & line = lines.at(i);
        if (line.startsWith("Title:"))
        {
            QString title = line.mid(6).trimmed();  // strip "Title:"
            return collapseWhitespace(title);
        }
    }
    return QString();
}

QString MetadataExtractor::pdfTitleFromBytes(const QByteArray & data) const
{
    const QByteArray key = "/Title";
    int searchFrom = 0;

    while (true)
    {
        int idx = data.indexOf(key, searchFrom);
        if (idx < 0)
            break;

        int p = idx + key.length();
        while (p < data.size() && isPdfWhitespace(data.at(p)))
            p++;

        if (p >= data.size())
            break;

        char c = data.at(p);

        if (c == '(')
        {
            // Literal string: read up to the matching unescaped ')'.
            int start = p + 1;
            int q = start;
            int depth = 1;
            bool escaped = false;
            while (q < data.size())
            {
                char cc = data.at(q);
                if (escaped)
                {
                    escaped = false;
                }
                else if (cc == '\\')
                {
                    escaped = true;
                }
                else if (cc == '(')
                {
                    depth++;
                }
                else if (cc == ')')
                {
                    depth--;
                    if (depth == 0)
                        break;
                }
                q++;
            }
            QString title = collapseWhitespace(decodePdfLiteral(data.mid(start, q - start)));
            if (!title.isEmpty())
                return title;
        }
        else if (c == '<' && (p + 1 >= data.size() || data.at(p + 1) != '<'))
        {
            // Hex string (a lone '<', not the '<<' that opens a dictionary).
            int start = p + 1;
            int q = data.indexOf('>', start);
            if (q > start)
            {
                QString title = collapseWhitespace(decodePdfHex(data.mid(start, q - start)));
                if (!title.isEmpty())
                    return title;
            }
        }

        searchFrom = idx + key.length();
    }

    return QString();
}

QString MetadataExtractor::decodePdfLiteral(const QByteArray & raw) const
{
    QByteArray out;
    int i = 0;
    int n = raw.size();

    while (i < n)
    {
        char c = raw.at(i);
        if (c != '\\')
        {
            out.append(c);
            i++;
            continue;
        }

        i++;                    // consume the backslash
        if (i >= n)
            break;

        char e = raw.at(i);
        switch (e)
        {
        case 'n':  out.append('\n'); i++; break;
        case 'r':  out.append('\r'); i++; break;
        case 't':  out.append('\t'); i++; break;
        case 'b':  out.append('\b'); i++; break;
        case 'f':  out.append('\f'); i++; break;
        case '(':  out.append('(');  i++; break;
        case ')':  out.append(')');  i++; break;
        case '\\': out.append('\\'); i++; break;

        case '\r':              // backslash before EOL = line continuation
            i++;
            if (i < n && raw.at(i) == '\n')
                i++;
            break;
        case '\n':
            i++;
            break;

        default:
            if (e >= '0' && e <= '7')
            {
                // Octal escape: up to three digits.
                int val = 0;
                int digits = 0;
                while (i < n && digits < 3 && raw.at(i) >= '0' && raw.at(i) <= '7')
                {
                    val = val * 8 + (raw.at(i) - '0');
                    i++;
                    digits++;
                }
                out.append(static_cast<char>(val & 0xFF));
            }
            else
            {
                out.append(e);  // unknown escape: keep the literal char
                i++;
            }
            break;
        }
    }

    return decodePdfBytes(out);
}

QString MetadataExtractor::decodePdfHex(const QByteArray & hex) const
{
    QByteArray clean;
    for (int i = 0; i < hex.size(); ++i)
    {
        char c = hex.at(i);
        if (isHexDigit(c))
            clean.append(c);
    }
    if (clean.size() % 2 != 0)
        clean.append('0');      // odd digit count: pad per the spec

    return decodePdfBytes(QByteArray::fromHex(clean));
}

QString MetadataExtractor::decodePdfBytes(const QByteArray & bytes) const
{
    // A leading UTF-16BE byte-order mark marks a Unicode string.
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes.at(0)) == 0xFE &&
        static_cast<unsigned char>(bytes.at(1)) == 0xFF)
    {
        QString result;
        for (int i = 2; i + 1 < bytes.size(); i += 2)
        {
            unsigned int hi = static_cast<unsigned char>(bytes.at(i));
            unsigned int lo = static_cast<unsigned char>(bytes.at(i + 1));
            result.append(QChar((hi << 8) | lo));
        }
        return result;
    }

    // Otherwise it is PDFDocEncoding, which overlaps Latin-1 for our purposes.
    return QString::fromLatin1(bytes);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QByteArray MetadataExtractor::runProcess(const QString & program,
                                         const QStringList & args) const
{
    QProcess proc;
    proc.start(program, args);

    if (!proc.waitForStarted(3000))
    {
        MetaDebug << "runProcess: could not start " << program.toStdString()
                  << " (is it installed?)" << std::endl;
        return QByteArray();
    }
    if (!proc.waitForFinished(10000))
    {
        proc.kill();
        proc.waitForFinished(2000);
        MetaDebug << "runProcess: timed out running " << program.toStdString() << std::endl;
        return QByteArray();
    }

    return proc.readAllStandardOutput();
}

QString MetadataExtractor::collapseWhitespace(const QString & s) const
{
    return s.simplified();
}
