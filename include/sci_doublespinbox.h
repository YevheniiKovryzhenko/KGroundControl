#pragma once
#include <QDoubleSpinBox>

class SciDoubleSpinBox : public QDoubleSpinBox {
public:
    explicit SciDoubleSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {}

    void setScientific(bool on) { if (scientific_ != on) { scientific_ = on; updateDisplay(); } }
    bool scientific() const { return scientific_; }

protected:
    QString textFromValue(double value) const override {
        if (scientific_) {
            return QString::number(value, 'E', 3);
        }
        // Default: fixed with current decimals
        return QDoubleSpinBox::textFromValue(value);
    }

    double valueFromText(const QString& text) const override {
        bool ok = false;
        double v = text.toDouble(&ok);
        if (ok) return v;
        return QDoubleSpinBox::valueFromText(text);
    }

private:
    void updateDisplay() {
        // Force repaint of text by re-setting the value
        const double v = value();
        blockSignals(true);
        setValue(v);
        blockSignals(false);
        update();
    }

    bool scientific_ = false;
};