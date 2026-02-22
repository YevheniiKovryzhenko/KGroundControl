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

#ifndef FAKE_MOCAP_DIALOG_H
#define FAKE_MOCAP_DIALOG_H

#include <QDialog>
#include "settings.h"

namespace Ui { class FakeMocapDialog; }

class FakeMocapDialog : public QDialog {
    Q_OBJECT
public:
    explicit FakeMocapDialog(QWidget* parent = nullptr);
    ~FakeMocapDialog();

    void setSettings(const fake_mocap_settings& s);

signals:
    void settingsChanged(const fake_mocap_settings& s);

private slots:
    void on_btn_toggle_clicked();
    void on_txt_period_textChanged(const QString& text);
    void on_txt_radius_textChanged(const QString& text);
    void on_txt_frame_id_textChanged(const QString& text);
    void on_btn_reset_clicked();
    void on_btn_save_clicked();
    void on_btn_exit_clicked();

private:
    void refreshUi();
    fake_mocap_settings settings_;
    Ui::FakeMocapDialog* ui;
};

#endif // FAKE_MOCAP_DIALOG_H
