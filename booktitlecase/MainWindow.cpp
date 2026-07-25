#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "FileScanner.h"
#include "SettingsDialog.h"
#include "WordSplitter.h"
#include "MetadataExtractor.h"
#include "PdfTitleFinder.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QCoreApplication>
#include <QApplication>

#include <iostream>

#define MwDebug if(0) std::cout

// Table column indices
#define COL_CHECK    0
#define COL_ORIGINAL 1
#define COL_EXTS     2
#define COL_SUGGEST  3
#define COL_STATUS   4

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , theConflictStrategy(STRAT_UNSET)
    , theSuppressCellSignal(false)
{
    ui->setupUi(this);

    QStringList headers;
    headers << "Use" << "Original" << "Extensions" << "Suggested" << "Status";
    ui->theTable->setColumnCount(headers.size());
    ui->theTable->setHorizontalHeaderLabels(headers);
    ui->theTable->horizontalHeader()->setStretchLastSection(false);
    ui->theTable->horizontalHeader()->setSectionResizeMode(COL_ORIGINAL, QHeaderView::Stretch);
    ui->theTable->horizontalHeader()->setSectionResizeMode(COL_SUGGEST,  QHeaderView::Stretch);
    ui->theTable->verticalHeader()->setVisible(false);

    QSettings s;
    theSystemDictPath = s.value("systemDictPath", "/usr/share/dict/words").toString();
    theTechDictPath   = s.value("techDictPath",   defaultTechDictPath()).toString();
    theCurrentFolder  = s.value("lastFolder",     QDir::homePath()).toString();
    theUseEmbedded    = s.value("useEmbedded",    true).toBool();
    theUseContentMatch = s.value("useContentMatch", true).toBool();

    connect(ui->theChooseFolderButton, &QPushButton::clicked,
            this,                      &MainWindow::onChooseFolder);
    connect(ui->theSettingsButton,     &QPushButton::clicked,
            this,                      &MainWindow::onSettings);
    connect(ui->theCheckAllButton,     &QPushButton::clicked,
            this,                      &MainWindow::onCheckAll);
    connect(ui->theUncheckAllButton,   &QPushButton::clicked,
            this,                      &MainWindow::onUncheckAll);
    connect(ui->theInvertButton,       &QPushButton::clicked,
            this,                      &MainWindow::onInvertChecks);
    connect(ui->theRenameButton,       &QPushButton::clicked,
            this,                      &MainWindow::onRename);
    connect(ui->theTable,              &QTableWidget::cellChanged,
            this,                      &MainWindow::onCellChanged);

    loadDictionaries();
    setStatusInfo(tr("Ready. Choose a folder to begin."));
}

MainWindow::~MainWindow()
{
    QSettings s;
    s.setValue("systemDictPath", theSystemDictPath);
    s.setValue("techDictPath",   theTechDictPath);
    s.setValue("lastFolder",     theCurrentFolder);
    s.setValue("useEmbedded",    theUseEmbedded);
    s.setValue("useContentMatch", theUseContentMatch);
    delete ui;
}

QString MainWindow::defaultTechDictPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString p = appDir + "/tech_dict.txt";
    if (QFile::exists(p))
        return p;
    // Fallback: source-tree path so dev runs work
    return "tech_dict.txt";
}

void MainWindow::loadDictionaries()
{
    bool sysOk  = theSystemDict.load(theSystemDictPath);
    bool techOk = theTechDict.load(theTechDictPath);

    if (!sysOk)
    {
        QMessageBox::warning(this, tr("Dictionary"),
                             tr("Could not load system dictionary:\n%1\n\n"
                                "Use Settings to choose a different file.")
                                 .arg(theSystemDictPath));
    }
    if (!techOk)
    {
        QMessageBox::warning(this, tr("Tech Dictionary"),
                             tr("Could not load tech dictionary:\n%1\n\n"
                                "Use Settings to choose a different file.")
                                 .arg(theTechDictPath));
    }

    MwDebug << "Dicts loaded: sys=" << theSystemDict.wordCount()
            << " tech=" << theTechDict.wordCount() << std::endl;
}

void MainWindow::onChooseFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Choose Folder"),
                                                    theCurrentFolder);
    if (dir.isEmpty())
        return;
    theCurrentFolder = dir;
    rescanCurrentFolder();
}

void MainWindow::rescanCurrentFolder()
{
    if (theCurrentFolder.isEmpty())
        return;

    FileScanner scanner;
    theGroups = scanner.scan(theCurrentFolder);
    populateEmbeddedTitles();
    populateContentMatches();
    resolveContentConflicts();
    regenerateSuggestions();
    rebuildTable();

    setStatusInfo(tr("%1: %2 group(s)")
                      .arg(theCurrentFolder)
                      .arg(theGroups.size()));
}

QString MainWindow::buildSuggestion(const QString & stem) const
{
    WordSplitter splitter(&theTechDict, &theSystemDict);
    QStringList parts = splitter.split(stem);
    if (parts.isEmpty())
        return QString();
    return theTitleCaser.titleCase(parts);
}

// Read the real title from inside each EPUB/PDF once, and cache it on the
// group. Prefer epub (clean metadata) over pdf when a group has both.
void MainWindow::populateEmbeddedTitles()
{
    for (int i = 0; i < theGroups.size(); ++i)
    {
        theGroups[i].embeddedTitle.clear();
        theGroups[i].metaSource.clear();
    }

    if (!theUseEmbedded)
        return;

    MetadataExtractor extractor;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    setStatusInfo(tr("Reading titles from document contents..."));

    for (int i = 0; i < theGroups.size(); ++i)
    {
        BookGroup & g = theGroups[i];

        // Look at epub before pdf so the cleaner source wins.
        QStringList order;
        order << "epub" << "pdf";
        for (int k = 0; k < order.size(); ++k)
        {
            int idx = g.extensions.indexOf(order.at(k));
            if (idx < 0)
                continue;

            QString path = QDir(g.directory).absoluteFilePath(g.originalNames.at(idx));
            QString title = extractor.extractTitle(path);
            if (!title.isEmpty())
            {
                g.embeddedTitle = title;
                g.metaSource    = order.at(k);
                break;
            }
        }
    }

    QApplication::restoreOverrideCursor();
}

// For each PDF, extract its text and find spellings of the title that match
// the filename. Cached on the group; only groups without cached candidates are
// scanned, so re-running after a settings change stays cheap.
void MainWindow::populateContentMatches()
{
    if (!theUseContentMatch)
        return;

    PdfTitleFinder finder;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    setStatusInfo(tr("Scanning PDF text for matching titles..."));

    for (int i = 0; i < theGroups.size(); ++i)
    {
        BookGroup & g = theGroups[i];
        if (!g.contentCandidates.isEmpty())
            continue;   // already scanned this session

        int idx = g.extensions.indexOf("pdf");
        if (idx < 0)
            continue;

        QString path = QDir(g.directory).absoluteFilePath(g.originalNames.at(idx));
        g.contentCandidates = finder.findCandidates(path, g.originalStem);
    }

    QApplication::restoreOverrideCursor();
}

// Turn the ranked candidate lists into a single resolved title per group.
// A "conflict" is a tie for the most common spelling; the first time one is
// seen we ask the user how to resolve conflicts for the rest of the session.
void MainWindow::resolveContentConflicts()
{
    for (int i = 0; i < theGroups.size(); ++i)
    {
        theGroups[i].contentTitle.clear();
        theGroups[i].contentConflict = false;
    }

    if (!theUseContentMatch)
        return;

    // Flag conflicts and remember the first one to use as a concrete example.
    int exampleIdx = -1;
    for (int i = 0; i < theGroups.size(); ++i)
    {
        BookGroup & g = theGroups[i];
        const QList<TitleCandidate> & c = g.contentCandidates;
        if (c.size() >= 2 && c.at(1).count == c.at(0).count)
        {
            g.contentConflict = true;
            if (exampleIdx < 0)
                exampleIdx = i;
        }
    }

    if (exampleIdx >= 0 && theConflictStrategy == STRAT_UNSET)
        theConflictStrategy = promptConflictStrategy(theGroups.at(exampleIdx));

    for (int i = 0; i < theGroups.size(); ++i)
        theGroups[i].contentTitle = resolveCandidates(theGroups[i]);
}

// Ask, once per session, how to break ties between equally-common spellings.
int MainWindow::promptConflictStrategy(const BookGroup & example)
{
    QStringList spellings;
    const QList<TitleCandidate> & c = example.contentCandidates;
    for (int i = 0; i < c.size() && i < 4; ++i)
        spellings << QString("\"%1\"").arg(c.at(i).text);

    QMessageBox box(this);
    box.setWindowTitle(tr("Title Spelling Conflict"));
    box.setIcon(QMessageBox::Question);
    box.setText(tr("Some PDFs spell the title more than one way, with no clear "
                   "winner."));
    box.setInformativeText(tr("For \"%1\" the choices are: %2\n\n"
                              "How should ties be resolved for the rest of this "
                              "session?")
                               .arg(example.originalStem, spellings.join(", ")));

    QPushButton *bTitle = box.addButton(tr("Use Title Case"),   QMessageBox::AcceptRole);
    QPushButton *bFirst = box.addButton(tr("Keep First Found"), QMessageBox::AcceptRole);
    QPushButton *bAsk   = box.addButton(tr("Choose Per Book"),  QMessageBox::AcceptRole);
    box.setDefaultButton(bTitle);
    box.exec();

    if (box.clickedButton() == bFirst)
        return STRAT_FIRST;
    if (box.clickedButton() == bAsk)
        return STRAT_ASK_EACH;
    return STRAT_TITLE_CASE;
}

// Pick a single spelling for one group, applying the session strategy when
// there is a conflict. The result is sanitized for use as a filename.
QString MainWindow::resolveCandidates(const BookGroup & g)
{
    const QList<TitleCandidate> & c = g.contentCandidates;
    if (c.isEmpty())
        return QString();

    QString picked;

    if (!g.contentConflict)
    {
        picked = c.at(0).text;                 // clear frequency winner
    }
    else
    {
        switch (theConflictStrategy)
        {
        case STRAT_FIRST:
        {
            // Candidates are ranked so the earliest among the tied ones is
            // already first, but be explicit for clarity.
            int best = 0;
            for (int i = 1; i < c.size(); ++i)
                if (c.at(i).firstPos < c.at(best).firstPos)
                    best = i;
            picked = c.at(best).text;
            break;
        }
        case STRAT_ASK_EACH:
        {
            QStringList items;
            for (int i = 0; i < c.size(); ++i)
                items << c.at(i).text;

            bool ok = false;
            QString choice = QInputDialog::getItem(
                this, tr("Choose Title"),
                tr("Multiple spellings found for \"%1\":").arg(g.originalStem),
                items, 0, false, &ok);
            picked = (ok && !choice.isEmpty()) ? choice : c.at(0).text;
            break;
        }
        case STRAT_TITLE_CASE:
        default:
            picked = titleCaseString(c.at(0).text);
            break;
        }
    }

    return sanitizeForFilename(picked);
}

QString MainWindow::titleCaseString(const QString & s) const
{
    QStringList words = s.split(' ', Qt::SkipEmptyParts);
    if (words.isEmpty())
        return s;
    return theTitleCaser.titleCase(words);
}

// Choose the suggested name for every group, in priority order:
//   1. EPUB metadata      — the most reliable source when present
//   2. PDF body-text match — recovers spacing even when metadata is wrong
//   3. PDF metadata        — often wrong, but better than nothing
//   4. dictionary splitter — last-resort guess from the filename
void MainWindow::regenerateSuggestions()
{
    for (int i = 0; i < theGroups.size(); ++i)
    {
        BookGroup & g = theGroups[i];
        g.fromMetadata = false;
        g.fromContent  = false;

        bool haveEpubMeta = theUseEmbedded && !g.embeddedTitle.isEmpty()
                            && g.metaSource == "epub";

        // 1. EPUB metadata.
        if (haveEpubMeta)
        {
            QString name = sanitizeForFilename(titleFromEmbedded(g.embeddedTitle));
            if (!name.isEmpty())
            {
                g.splitOk       = true;
                g.fromMetadata  = true;
                g.suggestedName = name;
                continue;
            }
        }

        // 2. Title recovered from the PDF's own body text.
        if (theUseContentMatch && !g.contentTitle.isEmpty())
        {
            g.splitOk       = true;
            g.fromContent   = true;
            g.suggestedName = g.contentTitle;
            continue;
        }

        // 3. Any remaining (PDF) metadata title.
        if (theUseEmbedded && !g.embeddedTitle.isEmpty())
        {
            QString name = sanitizeForFilename(titleFromEmbedded(g.embeddedTitle));
            if (!name.isEmpty())
            {
                g.splitOk       = true;
                g.fromMetadata  = true;
                g.suggestedName = name;
                continue;
            }
        }

        // 4. Dictionary word-splitter.
        QString sug = buildSuggestion(g.originalStem);
        if (sug.isEmpty())
        {
            g.splitOk       = false;
            g.suggestedName = g.originalStem;  // editable fallback
        }
        else
        {
            g.splitOk       = true;
            g.suggestedName = sug;
        }
    }
}

// The embedded title is authoritative, so keep its casing as-is UNLESS it is
// clearly unformatted (all uppercase or all lowercase), in which case run it
// through the title caser like a filename-derived suggestion.
QString MainWindow::titleFromEmbedded(const QString & rawTitle) const
{
    bool hasUpper = false;
    bool hasLower = false;
    for (int i = 0; i < rawTitle.size(); ++i)
    {
        if (rawTitle.at(i).isUpper())
            hasUpper = true;
        else if (rawTitle.at(i).isLower())
            hasLower = true;
    }

    bool wellCased = hasUpper && hasLower;
    if (wellCased)
        return rawTitle;

    QStringList words = rawTitle.split(' ', Qt::SkipEmptyParts);
    if (words.isEmpty())
        return rawTitle;
    return theTitleCaser.titleCase(words);
}

// Strip characters that are illegal in filenames on common filesystems so the
// title can actually be used as a name.
QString MainWindow::sanitizeForFilename(const QString & name) const
{
    QString out = name;
    const QString illegal = "/\\:*?\"<>|";
    for (int i = 0; i < out.size(); ++i)
    {
        if (illegal.contains(out.at(i)) || out.at(i) < ' ')
            out[i] = ' ';
    }

    out = out.simplified();

    // Windows dislikes trailing dots and spaces on names.
    while (out.endsWith('.') || out.endsWith(' '))
        out.chop(1);

    return out;
}

void MainWindow::rebuildTable()
{
    theSuppressCellSignal = true;
    ui->theTable->setRowCount(0);
    ui->theTable->setRowCount(theGroups.size());
    for (int i = 0; i < theGroups.size(); ++i)
        setRowFromGroup(i, theGroups.at(i));
    ui->theTable->resizeColumnToContents(COL_CHECK);
    ui->theTable->resizeColumnToContents(COL_EXTS);
    ui->theTable->resizeColumnToContents(COL_STATUS);
    theSuppressCellSignal = false;
}

void MainWindow::setRowFromGroup(int row, const BookGroup & g)
{
    QTableWidgetItem *check = new QTableWidgetItem();
    check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    check->setCheckState(g.checked ? Qt::Checked : Qt::Unchecked);
    ui->theTable->setItem(row, COL_CHECK, check);

    QTableWidgetItem *orig = new QTableWidgetItem(g.originalStem);
    orig->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->theTable->setItem(row, COL_ORIGINAL, orig);

    QTableWidgetItem *ext = new QTableWidgetItem(g.extensions.join(", "));
    ext->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->theTable->setItem(row, COL_EXTS, ext);

    QTableWidgetItem *sug = new QTableWidgetItem(g.suggestedName);
    sug->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    ui->theTable->setItem(row, COL_SUGGEST, sug);

    QString statusText;
    if (g.fromContent)
        statusText = g.contentConflict ? tr("from PDF text (conflict resolved)")
                                       : tr("from PDF text");
    else if (g.fromMetadata)
        statusText = tr("from %1 metadata").arg(g.metaSource);
    else if (g.splitOk)
        statusText = tr("OK");
    else
        statusText = tr("no split — edit manually");

    QTableWidgetItem *st = new QTableWidgetItem(statusText);
    st->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->theTable->setItem(row, COL_STATUS, st);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.setSystemDictPath(theSystemDictPath);
    dlg.setTechDictPath(theTechDictPath);
    dlg.setUseEmbedded(theUseEmbedded);
    dlg.setUseContentMatch(theUseContentMatch);
    if (dlg.exec() != QDialog::Accepted)
        return;

    theSystemDictPath  = dlg.systemDictPath();
    theTechDictPath    = dlg.techDictPath();
    theUseEmbedded     = dlg.useEmbedded();
    theUseContentMatch = dlg.useContentMatch();
    loadDictionaries();
    if (!theGroups.isEmpty())
    {
        populateEmbeddedTitles();
        populateContentMatches();
        resolveContentConflicts();
        regenerateSuggestions();
        rebuildTable();
    }
}

void MainWindow::onCheckAll()
{
    theSuppressCellSignal = true;
    for (int i = 0; i < theGroups.size(); ++i)
    {
        theGroups[i].checked = true;
        QTableWidgetItem *it = ui->theTable->item(i, COL_CHECK);
        if (it)
            it->setCheckState(Qt::Checked);
    }
    theSuppressCellSignal = false;
}

void MainWindow::onUncheckAll()
{
    theSuppressCellSignal = true;
    for (int i = 0; i < theGroups.size(); ++i)
    {
        theGroups[i].checked = false;
        QTableWidgetItem *it = ui->theTable->item(i, COL_CHECK);
        if (it)
            it->setCheckState(Qt::Unchecked);
    }
    theSuppressCellSignal = false;
}

void MainWindow::onInvertChecks()
{
    theSuppressCellSignal = true;
    for (int i = 0; i < theGroups.size(); ++i)
    {
        theGroups[i].checked = !theGroups[i].checked;
        QTableWidgetItem *it = ui->theTable->item(i, COL_CHECK);
        if (it)
            it->setCheckState(theGroups[i].checked ? Qt::Checked : Qt::Unchecked);
    }
    theSuppressCellSignal = false;
}

void MainWindow::onCellChanged(int row, int column)
{
    if (theSuppressCellSignal)
        return;
    if (row < 0 || row >= theGroups.size())
        return;

    QTableWidgetItem *it = ui->theTable->item(row, column);
    if (!it)
        return;

    if (column == COL_CHECK)
    {
        theGroups[row].checked = (it->checkState() == Qt::Checked);
    }
    else if (column == COL_SUGGEST)
    {
        theGroups[row].suggestedName = it->text();
    }
}

void MainWindow::onRename()
{
    int renamed = 0;
    int skipped = 0;
    int errors  = 0;
    QStringList errorList;

    for (int i = 0; i < theGroups.size(); ++i)
    {
        const BookGroup & g = theGroups.at(i);
        if (!g.checked)
        {
            skipped++;
            continue;
        }
        QString newStem = g.suggestedName.trimmed();
        if (newStem.isEmpty() || newStem == g.originalStem)
        {
            skipped++;
            continue;
        }

        QDir d(g.directory);
        for (int j = 0; j < g.originalNames.size(); ++j)
        {
            QString oldName = g.originalNames.at(j);
            QString ext     = g.extensions.at(j);
            QString newName = QString("%1.%2").arg(newStem, ext);

            QString oldPath = d.absoluteFilePath(oldName);
            QString newPath = d.absoluteFilePath(newName);

            if (oldPath == newPath)
            {
                skipped++;
                continue;
            }
            if (QFile::exists(newPath))
            {
                errors++;
                errorList << tr("Target already exists: %1").arg(newName);
                continue;
            }
            if (QFile::rename(oldPath, newPath))
            {
                renamed++;
            }
            else
            {
                errors++;
                errorList << tr("Failed: %1 -> %2").arg(oldName, newName);
            }
        }
    }

    QString msg = tr("Renamed %1 file(s). Skipped %2 group(s). Errors: %3.")
                      .arg(renamed).arg(skipped).arg(errors);
    if (errors > 0)
        msg += "\n\n" + errorList.join("\n");

    QMessageBox::information(this, tr("Rename Complete"), msg);

    rescanCurrentFolder();
}

void MainWindow::setStatusInfo(const QString & msg)
{
    statusBar()->showMessage(msg);
}
