#ifndef SAVEUSB_H
#define SAVEUSB_H

#include <QDialog>
#include <QDir>

namespace Ui {
class SaveUsb;
}

class SaveUsb : public QDialog
{
    Q_OBJECT

public:
    explicit SaveUsb(QWidget *parent = 0);
    ~SaveUsb();

    void FillList(QDir path);
private slots:
    void on_pbCancel_clicked();

    void on_pbSave_clicked();

private:
    Ui::SaveUsb *ui;

signals:
    void save_to_usb();
};

#endif // SAVEUSB_H
