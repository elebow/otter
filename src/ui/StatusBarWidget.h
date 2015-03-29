/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#ifndef OTTER_STATUSBARWIDGET_H
#define OTTER_STATUSBARWIDGET_H

#include <QtWidgets/QStatusBar>

namespace Otter
{

class MainWindow;
class ToolBarWidget;

class StatusBarWidget : public QStatusBar
{
	Q_OBJECT

public:
	explicit StatusBarWidget(MainWindow *parent);

protected:
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *event);

protected slots:
	void toolBarModified(int identifier);
	void updateSize();

private:
	ToolBarWidget *m_toolBar;
};

}

#endif