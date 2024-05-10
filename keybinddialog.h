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

private slots:
    void reset(void);

    bool eventFilter(QObject *object, QEvent *event);
    void keyPressEvent__(QKeyEvent* event);

private:
    Ui::KeyBindDialog *ui;
};

#endif // KEYBINDDIALOG_H
