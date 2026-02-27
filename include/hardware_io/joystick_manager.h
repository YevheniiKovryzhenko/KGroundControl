/****************************************************************************
 *
 *    Copyright (C) 2026  Yevhenii Kovryzhenko. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Affero General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License Version 3 for more details.
 *
 *    You should have received a copy of the
 *    GNU Affero General Public License Version 3
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions, and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. No ownership or credit shall be claimed by anyone not mentioned in
 *       the above copyright statement.
 *    4. Any redistribution or public use of this software, in whole or in part,
 *       whether standalone or as part of a different project, must remain
 *       under the terms of the GNU Affero General Public License Version 3,
 *       and all distributions in binary form must be accompanied by a copy of
 *       the source code, as stated in the GNU Affero General Public License.
 *
 ****************************************************************************/

#ifndef JOYSTICK_MANAGER_H
#define JOYSTICK_MANAGER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QButtonGroup>
#include <QPushButton>
#include <QHash>
// struct for relay configuration attached to each joystick mapping
#include <QProgressBar>
#include <QCheckBox>
#include <QPointer>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidgetItem>

#include "hardware_io/joystick.h"
#include "mavlink_communication/remote_control_manager.h"

// simple widget to display a POV hat direction


// simple widget to display a POV hat direction
class POVIndicator : public QWidget
{
public:
    explicit POVIndicator(QWidget *parent, int joystick, int pov);
    void setAngle(int deg);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    int angle = -1; // degrees; -1 means undefined
    int js;
    int povIndex;
};

class JoystickAxisBar : public QProgressBar
{
    Q_OBJECT
public:
    explicit JoystickAxisBar(QWidget* parent, int joystick, int axis);
    remote_control::channel::axis::joystick::manager joystick;
    int joystickId;
    int axisId;
};

class JoystickButton : public QCheckBox
{
    Q_OBJECT
public:
    explicit JoystickButton(QWidget* parent, int joystick, int button);
    remote_control::channel::button::joystick::manager button;
    int joystickId;
    int buttonId;
};

namespace Ui {
class Joystick_manager;
}

// Top‑level widget for joystick configuration.
//
// Builds dynamic controls for all axes, buttons and POVs of the currently
// selected device. Calibration values and role mappings are kept in sync
// with the QJoysticks backend and persisted automatically.  Unique role
// enforcement operates both within a single joystick and across all
// connected joysticks, eliminating assignment conflicts.
class Joystick_manager : public QWidget
{
    Q_OBJECT

public:
    explicit Joystick_manager(QWidget *parent = nullptr);
    ~Joystick_manager();

public slots:
    void update_roles_list();

signals:
    void calibration_mode_toggled(bool enabled);
    void reset_calibration(void);

    void unset_role(int role);

    // requests to other managers for available items (return results via emit)
    QVector<QString> get_port_names(void);
    QVector<uint8_t> get_sysids(void);
    QVector<mavlink_enums::mavlink_component_id> get_compids(uint8_t sysid);

private slots:
    void update_joystick_list(void);

    void on_comboBox_joysticopt_currentTextChanged(const QString &arg1);

    bool add_axis(int axis_id);
    bool add_button(int button_id);

protected:
    void showEvent(QShowEvent *event) override;
    void clear_axes(void);
    void clear_buttons(void);
    void clear_povs(void);

    // remove a mapping role from every joystick except the currently active one
    void clearRoleFromOtherDevices(int role);

    void on_pushButton_reset_clicked();
    void on_pushButton_export_clicked();
    void on_pushButton_import_clicked();
    void on_pushButton_cal_axes_toggled(bool checked);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    int current_joystick_id = 0;

    Ui::Joystick_manager *ui;

    QJoysticks *joysticks = nullptr;
    QPointer<QWidget> axes_container;
    QPointer<QWidget> buttons_container;
    QPointer<QWidget> povs_container;
    QVBoxLayout *axes_layout = nullptr;
    QGridLayout *buttons_layout = nullptr;
    QVBoxLayout *povs_layout = nullptr;


    // write current manager settings (calibration ranges, output scaling, roles,
    // inversion) back to the QJoystickDevice struct so that it will be saved by
    // QJoysticks.  This centralises persistence logic and avoids scattered
    // device writes from the UI layer.
    void commitRolesToDevice();

    // track widgets for each field so value labels can be updated live
    struct FieldWidget {
        QTreeWidgetItem* item = nullptr;
        QLabel* valueLabel = nullptr;
        QLineEdit* valueEdit = nullptr;
        QString fieldName;
    };
    QVector<QVector<FieldWidget>> fieldWidgets; // indexed by relay entry

    // map from role enum value to button widget in left panel
    QHash<int, QPushButton*> roleButtons;

    // global update when any joystick input changes
    void on_any_joystick_changed(int joystick, int index, qreal value);

    // determine which role (if any) corresponds to the given joystick element
    int findRole(int joystick, int index) const;

    // rebuild the left‑column roles UI from scratch
    void refresh_roles_ui();

    // update a single role button's displayed value
    void update_role_value(int role, qreal mappedVal);

    // role values are obtained from sharedRoleValues (mapping lives in managers)

private slots:
    // notified whenever any axis/button manager reports its mapped value changed
    void joystick_value_updated(int role, qreal value);

public:
    // data and helpers for right‑hand relay/message panel
    struct RelayEntry {
        QString name;
        QVector<QString> ports;
        JoystickRelaySettings settings;
        QVector<int> field_roles; // index corresponds to field order in message
        QVector<QString> field_values; // stored/editable value for each field
    };

private:
    QList<RelayEntry> relayEntries;
    int selected_relay_index = -1;
    QButtonGroup *relay_btn_group = nullptr;

    // background relay threads corresponding to entries (nullptr when inactive)
    QVector<remote_control::JoystickRelayThread*> relayThreads;

    // lists maintained for combobox population
    QStringList avail_ports;
    QVector<uint8_t> avail_sysids;
    QMap<uint8_t, QVector<mavlink_enums::mavlink_component_id>> avail_compids;
    QStringList avail_frameids;

    void on_relay_button_clicked(QAbstractButton *btn);

    void update_relays_list();
    void sync_relay_threads();

    // guard against recursive UI rebuilds triggered by thread signals
    bool updating_relays = false;

    // persistence helpers for relay entries
    static QList<RelayEntry> load_relay_entries();
    static void save_relay_entries(const QList<RelayEntry>& entries);

public slots:
    // slots to refresh available choices (to be connected from outside)
    void update_relay_port_list(const QVector<QString>& new_ports);
    void update_relay_sysid_list(const QVector<uint8_t>& new_sysids);
    void update_relay_compids(uint8_t sysid, const QVector<mavlink_enums::mavlink_component_id>& compids);
    void update_relay_frame_ids(const QVector<int>& frame_ids);

private slots:
    void on_button_add_clicked();
    void on_button_remove_clicked();
    void on_button_edit_clicked();
    // slot now receives the clicked button pointer
    // void on_relay_button_clicked(int id);
};

#endif // JOYSTICK_MANAGER_H
