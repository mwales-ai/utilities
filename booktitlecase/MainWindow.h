#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "BookGroup.h"
#include "Dictionary.h"
#include "TitleCaser.h"

#include <QMainWindow>
#include <QList>

namespace Ui
{
    class MainWindow;
}

class QTableWidgetItem;

/** Main GUI: scan a folder, suggest title-cased names, rename with checks. */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void  onChooseFolder();
    void  onSettings();
    void  onCheckAll();
    void  onUncheckAll();
    void  onInvertChecks();
    void  onRename();
    void  onCellChanged(int row, int column);

private:
    void          loadDictionaries();
    void          rescanCurrentFolder();
    void          populateEmbeddedTitles();
    void          regenerateSuggestions();
    void          rebuildTable();
    void          setRowFromGroup(int row, const BookGroup & g);
    QString       buildSuggestion(const QString & stem) const;
    QString       titleFromEmbedded(const QString & rawTitle) const;
    QString       sanitizeForFilename(const QString & name) const;
    void          setStatusInfo(const QString & msg);
    QString       defaultTechDictPath() const;

    Ui::MainWindow   *ui;

    Dictionary        theSystemDict;
    Dictionary        theTechDict;
    TitleCaser        theTitleCaser;

    QList<BookGroup>  theGroups;
    QString           theCurrentFolder;

    QString           theSystemDictPath;
    QString           theTechDictPath;

    bool              theUseEmbedded;   // read titles from PDF/EPUB contents
    bool              theSuppressCellSignal;
};

#endif
