#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "FileScanner.h"
#include "SettingsDialog.h"
#include "WordSplitter.h"
#include "MetadataExtractor.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
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

void MainWindow::regenerateSuggestions()
{
    for (int i = 0; i < theGroups.size(); ++i)
    {
        BookGroup & g = theGroups[i];

        // An embedded title from the document itself beats guessing from the
        // filename, so prefer it whenever we managed to read one.
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

        g.fromMetadata = false;
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
    if (g.fromMetadata)
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
    if (dlg.exec() != QDialog::Accepted)
        return;

    theSystemDictPath = dlg.systemDictPath();
    theTechDictPath   = dlg.techDictPath();
    theUseEmbedded    = dlg.useEmbedded();
    loadDictionaries();
    if (!theGroups.isEmpty())
    {
        populateEmbeddedTitles();
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
