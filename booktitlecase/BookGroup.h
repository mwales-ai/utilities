#ifndef BOOKGROUP_H
#define BOOKGROUP_H

#include "PdfTitleFinder.h"   // TitleCandidate

#include <QString>
#include <QStringList>
#include <QList>

/** A set of files that share a stem but differ by extension.
 *  e.g. animalfarm.pdf + animalfarm.epub + animalfarm.mobi -> one group. */
struct BookGroup
{
    QString      directory;        // absolute path to the containing dir
    QString      originalStem;     // e.g. "animalfarm" (case from disk)
    QStringList  extensions;       // e.g. ["pdf", "epub", "mobi"]
    QStringList  originalNames;    // exact filenames as they exist on disk
    QString      suggestedName;    // e.g. "Animal Farm" (no extension)
    bool         splitOk;          // true if word splitter succeeded
    bool         checked;          // user wants to rename this group

    QString      embeddedTitle;    // title read from inside the doc ("" if none)
    QString      metaSource;       // "epub", "pdf", or "" — where it came from
    bool         fromMetadata;     // suggestedName came from the embedded title

    QList<TitleCandidate>  contentCandidates;  // title spellings found in PDF text
    QString                contentTitle;       // resolved spelling ("" if none)
    bool                   contentConflict;    // >1 equally-common spelling found
    bool                   fromContent;        // suggestedName came from PDF text
};

#endif
