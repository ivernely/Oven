#ifndef REPORTDIALOG_H
#define REPORTDIALOG_H

#include <QDialog>
#include <QDir>

namespace Ui {
class ReportDialog;
}

class ReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportDialog(QWidget *parent = 0);
    ~ReportDialog();

    void LoadDataFiles(QDir path);

private:
    Ui::ReportDialog *ui;

signals:
    void set_data_file(QString);
private slots:
    void on_pbOk_clicked();
    void on_pbCancel_clicked();
    void on_pbUp_clicked();
    void on_pbDown_clicked();
};

#endif // REPORTDIALOG_H
