/****************************************************************************
 *
 *    Copyright (C) 2025  Yevhenii Kovryzhenko. All rights reserved.
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
 ****************************************************************************/

#include "collapsible_group.h"
#include <QToolButton>
#include <QVBoxLayout>
#include <QFrame>
#include <QPropertyAnimation>
#include <QPalette>
#include <QColor>
#include <QApplication>

CollapsibleGroup::CollapsibleGroup(QWidget* parent)
    : QWidget(parent)
{
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(2);
    mainLayout->setContentsMargins(0, 5, 0, 5);

    // Header button (clickable title) - use standard button styling from app
    headerButton_ = new QToolButton(this);
    
    // Use application font for consistency with the rest of the app
    QFont headerFont = QApplication::font();
    headerFont.setBold(true);
    headerButton_->setFont(headerFont);
    
    // Don't override stylesheet - let the button use standard app styling
    // This makes it look exactly like other buttons in the application
    headerButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    headerButton_->setArrowType(Qt::RightArrow);
    headerButton_->setText("");
    // Don't use setCheckable - we want the button to remain visually unchanged when clicked
    headerButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerButton_->setMinimumHeight(35);
    headerButton_->setCursor(Qt::PointingHandCursor);
    
    // Content widget (initially hidden) with padding
    contentWidget_ = new QWidget(this);
    contentWidget_->setVisible(false);
    contentWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    contentWidget_->setContentsMargins(10, 5, 10, 5);
    
    // Add widgets to layout
    mainLayout->addWidget(headerButton_);
    mainLayout->addWidget(contentWidget_);
    
    // Initialize arrow to show collapsed state
    updateArrow();
    
    // Connect toggle
    connect(headerButton_, &QToolButton::clicked, this, &CollapsibleGroup::toggle);
}

void CollapsibleGroup::setTitle(const QString& title)
{
    if (headerButton_) {
        headerButton_->setText(title);
    }
}

void CollapsibleGroup::setContentLayout(QLayout* layout)
{
    if (contentWidget_ && layout) {
        // Remove old layout if exists
        if (contentWidget_->layout()) {
            delete contentWidget_->layout();
        }
        contentWidget_->setLayout(layout);
    }
}

void CollapsibleGroup::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    contentWidget_->setVisible(!collapsed_);
    updateArrow();
}

void CollapsibleGroup::toggle()
{
    setCollapsed(!collapsed_);
}

void CollapsibleGroup::updateArrow()
{
    headerButton_->setArrowType(collapsed_ ? Qt::RightArrow : Qt::DownArrow);
}
