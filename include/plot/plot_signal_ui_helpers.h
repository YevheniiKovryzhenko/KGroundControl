#pragma once

#include <QCheckBox>
#include <QSet>

namespace plot_signal_ui_helpers {

// Shared dynamic properties used for plot checkbox binding.
inline constexpr const char* kPlotIdProperty = "plotId";
inline constexpr const char* kPlotLabelProperty = "plotLabel";

QCheckBox* createPlotCheckBox(QWidget* parent, const QString& id, const QString& label);
void bindPlotCheckBox(QCheckBox* cb, const QString& id, const QString& label);

void syncPlotCheckBoxes(QWidget* root, const QSet<QString>& taggedIds);
void syncPlotCheckBoxes(QWidget* root);

} // namespace plot_signal_ui_helpers
