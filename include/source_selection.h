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

#ifndef SOURCE_SELECTION_H
#define SOURCE_SELECTION_H

#include <QComboBox>
#include <QSignalBlocker>
#include <QStringList>

// Refresh by identity, never by row. Unavailable configured targets stay unselected;
// browsing selectors can choose the first surviving item. No placeholder is an item.
namespace source_selection {
inline void sync(QComboBox *box, const QStringList &values, const QString &selected,
                 bool chooseFirst = true)
{
    QStringList clean;
    for (const auto &value : values)
        if (!value.isEmpty() && !clean.contains(value)) clean.append(value);
    const QSignalBlocker blocker(box);
    box->clear();
    for (const auto &value : clean) box->addItem(value, value);
    const int index = clean.indexOf(selected);
    box->setPlaceholderText(selected.isEmpty() ? QStringLiteral("No selection")
                                             : QStringLiteral("Unavailable: %1").arg(selected));
    box->setCurrentIndex(index >= 0 ? index : (chooseFirst && !clean.isEmpty() ? 0 : -1));
    box->setEnabled(!clean.isEmpty());
}
inline void sync(QComboBox *box, const QStringList &values)
{
    sync(box, values, box->currentText());
}
inline void sync_target(QComboBox *box, const QStringList &values)
{
    const QString selected = box->currentIndex() >= 0 ? box->currentText()
        : box->property("selectedSource").toString();
    sync(box, values, selected, selected.isEmpty());
    if (box->currentIndex() >= 0) box->setProperty("selectedSource", box->currentText());
    else if (!selected.isEmpty()) box->setProperty("selectedSource", selected);
}
}
#endif
