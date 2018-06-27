#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QDialog>

namespace Ui {
class keyboard;
}

class keyboard : public QDialog
{
    Q_OBJECT

public:
    explicit keyboard(QWidget *parent = 0);
    ~keyboard();

protected:
    void changeEvent(QEvent *e);

public slots:
    void SetInputIntValues();
    void SetIPMask();

private slots:
    void on_pbtn_cancel_clicked();

    void on_pbtn_Ok_clicked();

    void on_pbtn_0_clicked();

    void on_pbtn_1_clicked();

    void on_pbtn_2_clicked();

    void on_pbtn_3_clicked();

    void on_pbtn_4_clicked();

    void on_pbtn_5_clicked();

    void on_pbtn_6_clicked();

    void on_pbtn_7_clicked();

    void on_pbtn_8_clicked();

    void on_pbtn_9_clicked();

    void on_pbtn_del_clicked();

    void on_pbtn_bkspc_clicked();

    void on_pbtn_point_clicked();

private:
    Ui::keyboard *ui;
    bool ip;

signals:
    void set_value(double);
    void set_address(QString);
    void input_int();
};

#endif // KEYBOARD_H
