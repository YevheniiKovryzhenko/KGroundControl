#ifndef ADDTARGETDIALOG_H
#define ADDTARGETDIALOG_H

#include <QDialog>

namespace Ui {
class AddTargetDialog;
}

class AddTargetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddTargetDialog(QWidget *parent = nullptr);
    ~AddTargetDialog();

private slots:

    void on_btn_serial_clicked();

private:
    Ui::AddTargetDialog *ui;
};

#endif // ADDTARGETDIALOG_H
