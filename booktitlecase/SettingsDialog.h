#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui
{
    class SettingsDialog;
}

/** Configure the system and tech dictionary paths. */
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    QString  systemDictPath() const;
    QString  techDictPath() const;

    void     setSystemDictPath(const QString & path);
    void     setTechDictPath(const QString & path);

private slots:
    void     onBrowseSystem();
    void     onBrowseTech();

private:
    Ui::SettingsDialog *ui;
};

#endif
