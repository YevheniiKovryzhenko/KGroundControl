#pragma once

#include <QCheckBox>
#include <QSet>
#include <QComboBox>
#include "plot/plot_signal_registry.h"

namespace plot_signal_ui_helpers {

// Shared dynamic properties used for plot checkbox binding.
inline constexpr const char* kPlotIdProperty = "plotId";
inline constexpr const char* kPlotLabelProperty = "plotLabel";

// Visible labels are independent of the stable IDs stored in item data.
void populateSignalCombo(QComboBox* combo, const QVector<PlotSignalDef>& definitions);

QCheckBox* createPlotCheckBox(QWidget* parent, const QString& id, const QString& label);
void bindPlotCheckBox(QCheckBox* cb, const QString& id, const QString& label);

void syncPlotCheckBoxes(QWidget* root, const QSet<QString>& taggedIds);
void syncPlotCheckBoxes(QWidget* root);

} // namespace plot_signal_ui_helpers
