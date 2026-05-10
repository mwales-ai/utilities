#ifndef BOOKGROUP_H
#define BOOKGROUP_H

#include <QString>
#include <QStringList>

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
};

#endif
