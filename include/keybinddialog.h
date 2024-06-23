#ifndef KEYBINDDIALOG_H
#define KEYBINDDIALOG_H

#include <QDialog>

namespace Ui {
class KeyBindDialog;
}

class KeyBindDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyBindDialog(QWidget *parent = nullptr);
    ~KeyBindDialog();

    static QString key_map(QKeyEvent* event);

signals:
    void pass_key_string(QString key);

private slots:
    void reset(void);
    void accept(void);

    bool eventFilter(QObject *object, QEvent *event);
    void keyPressEvent__(QKeyEvent* event);

private:
    Ui::KeyBindDialog *ui;
    bool field_is_valid = false;
};

#endif // KEYBINDDIALOG_H
