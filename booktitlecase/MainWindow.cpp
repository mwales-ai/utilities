#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "FileScanner.h"
#include "SettingsDialog.h"
#include "WordSplitter.h"

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

void MainWindow::regenerateSuggestions()
{
    for (int i = 0; i < theGroups.size(); ++i)
    {
        BookGroup & g = theGroups[i];
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

    QString statusText = g.splitOk ? tr("OK") : tr("no split — edit manually");
    QTableWidgetItem *st = new QTableWidgetItem(statusText);
    st->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->theTable->setItem(row, COL_STATUS, st);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.setSystemDictPath(theSystemDictPath);
    dlg.setTechDictPath(theTechDictPath);
    if (dlg.exec() != QDialog::Accepted)
        return;

    theSystemDictPath = dlg.systemDictPath();
    theTechDictPath   = dlg.techDictPath();
    loadDictionaries();
    if (!theGroups.isEmpty())
    {
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
