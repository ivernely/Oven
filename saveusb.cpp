#include "saveusb.h"
#include "ui_saveusb.h"
#include <QStyle>
#include <QDesktopWidget>

//--------------------------------------------------------------
SaveUsb::SaveUsb(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SaveUsb)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::BypassWindowManagerHint);
    this->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                       this->size(), qApp->desktop()->availableGeometry()));
}
//--------------------------------------------------------------
SaveUsb::~SaveUsb()
{
    delete ui;
}
//--------------------------------------------------------------
//заполнение списка файлов из корня флешки
void SaveUsb::FillList(QDir path)
{
    QStringList list = path.entryList();

    ui->listWidget->addItems(list);
}
//--------------------------------------------------------------
void SaveUsb::on_pbCancel_clicked()
{
    close();
}
//--------------------------------------------------------------
void SaveUsb::on_pbSave_clicked()
{
    emit save_to_usb();
}
//--------------------------------------------------------------
