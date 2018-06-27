#ifndef TSDIALOG_H
#define TSDIALOG_H

#include <QDialog>

namespace Ui {
class tsdialog;
}

class tsdialog : public QDialog
{
    Q_OBJECT

public:
    explicit tsdialog(QWidget *parent = 0);
    ~tsdialog();

private slots:

    void on_pbtnOk_clicked();

private:
    Ui::tsdialog *ui;

signals:
    void set_value(bool);
};

#endif // TSDIALOG_H
