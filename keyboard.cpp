#include "keyboard.h"
#include "ui_keyboard.h"
#include <QMessageBox>
#include <QStyle>
#include <QDesktopWidget>

//--------------------------------------------------------------
keyboard::keyboard(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::keyboard)
{
    ui->setupUi(this);

    ip = false;

    this->setWindowFlags(Qt::BypassWindowManagerHint);
    this->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                        this->size(), qApp->desktop()->availableGeometry()));
}
//--------------------------------------------------------------
keyboard::~keyboard()
{
    delete ui;
}
//--------------------------------------------------------------
void keyboard::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
//--------------------------------------------------------------
void keyboard::SetInputIntValues()
{
    ui->pbtn_point->setEnabled(false);
    ui->lineEdit->setMaxLength(8);
}
//--------------------------------------------------------------
//задать маску для ввода IP адреса
void keyboard::SetIPMask()
{
    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";

    QRegExp ipRegex ("^" + ipRange
                     + "\\." + ipRange
                     + "\\." + ipRange
                     + "\\." + ipRange + "$");

    QRegExpValidator *ipValidator = new QRegExpValidator(ipRegex, this);

    ui->lineEdit->setValidator(ipValidator);

    ui->lineEdit->setMaxLength(32);
    ip = true;
}
//--------------------------------------------------------------
void keyboard::on_pbtn_cancel_clicked()
{
    close();
}
//--------------------------------------------------------------
void keyboard::on_pbtn_Ok_clicked()
{
    bool ok;
    double value;

    if(!ip){
        value = ui->lineEdit->text().toDouble(&ok);

        if(ok && value>0)
            emit set_value(value);
        else{
            QMessageBox msg(this);
            msg.addButton(trUtf8("OK"), QMessageBox::AcceptRole);
            msg.setText(trUtf8("Введено неверное значение."));
            msg.setWindowTitle(trUtf8("Ошибка"));
            msg.setIcon(QMessageBox::Critical);
            msg.setWindowFlags(Qt::BypassWindowManagerHint);
            msg.exec();
        }
    }
    else
        emit set_address(ui->lineEdit->text());
}
//--------------------------------------------------------------
void keyboard::on_pbtn_0_clicked()
{
    ui->lineEdit->insert("0");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_1_clicked()
{
    ui->lineEdit->insert("1");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_2_clicked()
{
    ui->lineEdit->insert("2");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_3_clicked()
{
    ui->lineEdit->insert("3");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_4_clicked()
{
    ui->lineEdit->insert("4");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_5_clicked()
{
    ui->lineEdit->insert("5");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_6_clicked()
{
    ui->lineEdit->insert("6");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_7_clicked()
{
    ui->lineEdit->insert("7");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_8_clicked()
{
    ui->lineEdit->insert("8");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_9_clicked()
{
    ui->lineEdit->insert("9");
}
//--------------------------------------------------------------
void keyboard::on_pbtn_del_clicked()
{
    ui->lineEdit->clear();
}
//--------------------------------------------------------------
void keyboard::on_pbtn_bkspc_clicked()
{
    ui->lineEdit->backspace();
}
//--------------------------------------------------------------
void keyboard::on_pbtn_point_clicked()
{
    ui->lineEdit->insert(QString("."));
}
//--------------------------------------------------------------
