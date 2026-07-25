#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    connect(ui->theBrowseSystemButton, &QPushButton::clicked,
            this,                      &SettingsDialog::onBrowseSystem);
    connect(ui->theBrowseTechButton,   &QPushButton::clicked,
            this,                      &SettingsDialog::onBrowseTech);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

QString SettingsDialog::systemDictPath() const
{
    return ui->theSystemDictEdit->text();
}

QString SettingsDialog::techDictPath() const
{
    return ui->theTechDictEdit->text();
}

bool SettingsDialog::useEmbedded() const
{
    return ui->theUseEmbeddedCheck->isChecked();
}

bool SettingsDialog::useContentMatch() const
{
    return ui->theUseContentMatchCheck->isChecked();
}

void SettingsDialog::setSystemDictPath(const QString & path)
{
    ui->theSystemDictEdit->setText(path);
}

void SettingsDialog::setTechDictPath(const QString & path)
{
    ui->theTechDictEdit->setText(path);
}

void SettingsDialog::setUseEmbedded(bool on)
{
    ui->theUseEmbeddedCheck->setChecked(on);
}

void SettingsDialog::setUseContentMatch(bool on)
{
    ui->theUseContentMatchCheck->setChecked(on);
}

void SettingsDialog::onBrowseSystem()
{
    QString cur = ui->theSystemDictEdit->text();
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Select System Dictionary"),
                                                cur,
                                                tr("Text Files (*.txt *);;All Files (*)"));
    if (!path.isEmpty())
        ui->theSystemDictEdit->setText(path);
}

void SettingsDialog::onBrowseTech()
{
    QString cur = ui->theTechDictEdit->text();
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Select Tech Dictionary"),
                                                cur,
                                                tr("Text Files (*.txt *);;All Files (*)"));
    if (!path.isEmpty())
        ui->theTechDictEdit->setText(path);
}
