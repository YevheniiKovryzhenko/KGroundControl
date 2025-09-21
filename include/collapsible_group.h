#pragma once
#include <QWidget>
#include <QString>

class QToolButton;
class QWidget;
class QLayout;

class CollapsibleGroup : public QWidget {
    Q_OBJECT
public:
    explicit CollapsibleGroup(const QString& title = QString(), QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setContentLayout(QLayout* layout);
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return collapsed_; }

private slots:
    void toggle();

private:
    QToolButton* headerButton_ = nullptr;
    QWidget* contentWidget_ = nullptr;
    bool collapsed_ = true;
    void updateArrow();
};
