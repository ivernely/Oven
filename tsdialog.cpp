#include "tsdialog.h"
#include "mainwindow.h"
#include "ui_tsdialog.h"
#include <QStyle>
#include <QDesktopWidget>

//--------------------------------------------------------------
tsdialog::tsdialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::tsdialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::BypassWindowManagerHint);
    this->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                       this->size(), qApp->desktop()->availableGeometry()));
}
//--------------------------------------------------------------
tsdialog::~tsdialog()
{
    delete ui;
}
//--------------------------------------------------------------
void tsdialog::on_pbtnOk_clicked()
{
    emit set_value(ui->rbtnSetTemp->isChecked());
    close();
}
//--------------------------------------------------------------
