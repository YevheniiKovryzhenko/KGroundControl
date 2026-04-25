#include "plot/plot_signal_ui_helpers.h"

#include <QSignalBlocker>

#include "plot/plot_signal_registry.h"

namespace plot_signal_ui_helpers {

static constexpr const char* kPlotBindingConnected = "_plotBindingConnected";

void bindPlotCheckBox(QCheckBox* cb, const QString& id, const QString& label)
{
    if (!cb) return;

    cb->setText(QString());
    cb->setProperty(kPlotIdProperty, id);
    cb->setProperty(kPlotLabelProperty, label);
    cb->setEnabled(!id.isEmpty());

    if (!cb->property(kPlotBindingConnected).toBool()) {
        QObject::connect(cb, &QCheckBox::toggled, cb, [cb](bool on){
            const QString plotId = cb->property(kPlotIdProperty).toString();
            if (plotId.isEmpty()) return;
            const QString plotLabel = cb->property(kPlotLabelProperty).toString();
            PlotSignalRegistry::instance().setTagged(PlotSignalDef{ plotId, plotLabel }, on);
        });
        cb->setProperty(kPlotBindingConnected, true);
    }

    QSignalBlocker blocker(cb);
    cb->setChecked(PlotSignalRegistry::instance().isTagged(id));
}

QCheckBox* createPlotCheckBox(QWidget* parent, const QString& id, const QString& label)
{
    auto* cb = new QCheckBox(parent);
    bindPlotCheckBox(cb, id, label);
    return cb;
}

void syncPlotCheckBoxes(QWidget* root, const QSet<QString>& taggedIds)
{
    if (!root) return;

    const auto checkboxes = root->findChildren<QCheckBox*>();
    for (QCheckBox* cb : checkboxes) {
        if (!cb || !cb->property(kPlotIdProperty).isValid()) continue;
        const QString id = cb->property(kPlotIdProperty).toString();
        const bool shouldBeChecked = taggedIds.contains(id);
        if (cb->isChecked() != shouldBeChecked) {
            QSignalBlocker blocker(cb);
            cb->setChecked(shouldBeChecked);
        }
    }
}

void syncPlotCheckBoxes(QWidget* root)
{
    syncPlotCheckBoxes(root, PlotSignalRegistry::instance().taggedIds());
}

} // namespace plot_signal_ui_helpers
