/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2014 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "Notification.h"
#include "Application.h"
#include "PlatformIntegration.h"

namespace Otter
{

Notification::Notification(const QString &message, NotificationLevel level, QObject *parent) : QObject(parent),
	m_message(message),
	m_level(level)
{
}

void Notification::markClicked()
{
	emit clicked();

	deleteLater();
}

void Notification::markIgnored()
{
	emit ignored();

	deleteLater();
}

QString Notification::getMessage() const
{
	return m_message;
}

NotificationLevel Notification::getLevel() const
{
	return m_level;
}

Notification* Notification::createNotification(const QString &message, NotificationLevel level)
{
	Notification *notification = new Notification(message, level, Application::getInstance());
	PlatformIntegration *integration = Application::getInstance()->getPlatformIntegration();

	if (integration && integration->canShowNotifications())
	{
		integration->showNotification(notification);
	}
	else
	{
///TODO
	}

	return notification;
}

}

