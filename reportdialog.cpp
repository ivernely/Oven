#include "reportdialog.h"
#include "ui_reportdialog.h"
#include <QStyle>
#include <QDesktopWidget>

//--------------------------------------------------------------
ReportDialog::ReportDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReportDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::BypassWindowManagerHint);
    this->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                       this->size(), qApp->desktop()->availableGeometry()));
}
//--------------------------------------------------------------
ReportDialog::~ReportDialog()
{
    delete ui;
}
//--------------------------------------------------------------
//загрузка списка файлов из директории
void ReportDialog::LoadDataFiles(QDir path)
{
    QStringList data_files = path.entryList(QStringList("*.csv")<<"*.xls");

    ui->listWidget->addItems(data_files);
}
//--------------------------------------------------------------
void ReportDialog::on_pbOk_clicked()
{
    set_data_file(ui->listWidget->currentItem()->text());
}
//--------------------------------------------------------------
void ReportDialog::on_pbCancel_clicked()
{
    close();
}
//--------------------------------------------------------------
//переместиться на строку вверх
void ReportDialog::on_pbUp_clicked()
{
    int index = ui->listWidget->currentIndex().row();
    if(index > 0){
        index--;
        ui->listWidget->setCurrentRow(index);
    }
}
//--------------------------------------------------------------
//переместиться на строку вних
void ReportDialog::on_pbDown_clicked()
{
    int index = ui->listWidget->currentIndex().row();
    int cnt = ui->listWidget->count();

    index++;
    if(index < cnt)
        ui->listWidget->setCurrentRow(index);
}
//--------------------------------------------------------------
