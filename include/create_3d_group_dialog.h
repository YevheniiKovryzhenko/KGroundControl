#pragma once
#include <QDialog>
#include <QVector>
#include <QString>
#include <QPushButton>

namespace Ui { class Create3DGroupDialog; }

class Create3DGroupDialog : public QDialog {
    Q_OBJECT

public:
    explicit Create3DGroupDialog(const QVector<QString>& availableSignals, QWidget* parent = nullptr);
    ~Create3DGroupDialog();

    QString getGroupName() const;
    QString getXSignal() const;
    QString getYSignal() const;
    QString getZSignal() const;

private slots:
    void validateAndAccept();

private:
    Ui::Create3DGroupDialog* ui;
    QVector<QString> availableSignals_;
};