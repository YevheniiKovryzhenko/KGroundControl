#ifndef RELAYDIALOG_H
#define RELAYDIALOG_H

#include <QDialog>

namespace Ui {
class RelayDialog;
}

class RelayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RelayDialog(QWidget *parent, QString src_port_name, QVector<QString> available_port_names);
    ~RelayDialog();

signals:
    bool selected_items(QString &src_port_name, QVector<QString> &available_port_names);

private slots:

    void on_accept_reject_bar_accepted();

    void on_accept_reject_bar_rejected();

private:
    Ui::RelayDialog *ui;
    QString port_name;
};

#endif // RELAYDIALOG_H
