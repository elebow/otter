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

#include "ConsoleWidget.h"
#include "../core/Utils.h"

#include "ui_ConsoleWidget.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

namespace Otter
{

ConsoleWidget::ConsoleWidget(QWidget *parent) : QWidget(parent),
	m_model(NULL),
	m_ui(new Ui::ConsoleWidget)
{
	m_ui->setupUi(this);

	connect(m_ui->scopeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(filterCategories()));
	connect(m_ui->networkButton, SIGNAL(clicked()), this, SLOT(filterCategories()));
	connect(m_ui->securityButton, SIGNAL(clicked()), this, SLOT(filterCategories()));
	connect(m_ui->javaScriptButton, SIGNAL(clicked()), this, SLOT(filterCategories()));
	connect(m_ui->otherButton, SIGNAL(clicked()), this, SLOT(filterCategories()));
	connect(m_ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
	connect(m_ui->filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterMessages(QString)));
	connect(m_ui->consoleView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
}

ConsoleWidget::~ConsoleWidget()
{
	delete m_ui;
}

void ConsoleWidget::showEvent(QShowEvent *event)
{
	if (!m_model)
	{
		m_model = new QStandardItemModel(this);
		m_model->setSortRole(Qt::UserRole);

		const QList<ConsoleMessage*> messages = Console::getMessages();

		for (int i = 0; i < messages.count(); ++i)
		{
			addMessage(messages.at(i));
		}

		m_ui->consoleView->setModel(m_model);

		connect(Console::getInstance(), SIGNAL(messageAdded(ConsoleMessage*)), this, SLOT(addMessage(ConsoleMessage*)));
	}

	QWidget::showEvent(event);
}

void ConsoleWidget::addMessage(ConsoleMessage *message)
{
	if (!m_model || !message)
	{
		return;
	}

	QIcon icon;
	QString category;

	switch (message->level)
	{
		case ErrorMessageLevel:
			icon = Utils::getIcon(QLatin1String("dialog-error"));

			break;
		case WarningMessageLevel:
			icon = Utils::getIcon(QLatin1String("dialog-warning"));

			break;
		default:
			icon = Utils::getIcon(QLatin1String("dialog-information"));

			break;
	}

	switch (message->category)
	{
		case NetworkMessageCategory:
			category = tr("Network");

			break;
		case SecurityMessageCategory:
			category = tr("Security");

			break;
		case JavaScriptMessageCategory:
			category = tr("JS");

			break;
		default:
			category = tr("Other");

			break;
	}

	const QString source = message->source + ((message->line > 0) ? QStringLiteral(":%1").arg(message->line) : QString());
	QString entry = QStringLiteral("[%1] %2").arg(message->time.toString()).arg(category);

	if (!message->source.isEmpty())
	{
		entry.append(QStringLiteral(" - %1").arg(source));
	}

	QStandardItem *parentItem = new QStandardItem(icon, entry);
	parentItem->setData(message->time.toTime_t(), Qt::UserRole);
	parentItem->setData(message->category, (Qt::UserRole + 1));
	parentItem->setData(source, (Qt::UserRole + 2));
	parentItem->setData(message->window, (Qt::UserRole + 3));

	if (!message->note.isEmpty())
	{
		parentItem->appendRow(new QStandardItem(message->note));
	}

	m_model->appendRow(parentItem);
	m_model->sort(0, Qt::DescendingOrder);
}

void ConsoleWidget::clear()
{
	if (m_model)
	{
		m_model->clear();
	}
}

void ConsoleWidget::copyText()
{
	QApplication::clipboard()->setText(m_ui->consoleView->currentIndex().data(Qt::DisplayRole).toString(), QClipboard::Selection);
}

void ConsoleWidget::filterCategories()
{
	QList<MessageCategory> categories;

	if (m_ui->networkButton->isChecked())
	{
		categories.append(NetworkMessageCategory);
	}

	if (m_ui->securityButton->isChecked())
	{
		categories.append(SecurityMessageCategory);
	}

	if (m_ui->javaScriptButton->isChecked())
	{
		categories.append(JavaScriptMessageCategory);
	}

	if (m_ui->otherButton->isChecked())
	{
		categories.append(OtherMessageCategory);
	}

	for (int i = 0; i < m_model->rowCount(); ++i)
	{
		QStandardItem *item = m_model->item(i, 0);

		if (item)
		{
			if (categories.contains(static_cast<MessageCategory>(item->data(Qt::UserRole + 1).toInt())))
			{
				item->setFlags(item->flags() | Qt::ItemIsEnabled);
			}
			else
			{
				item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

				m_ui->consoleView->setRowHidden(i, m_ui->consoleView->rootIndex(), true);
			}
		}
	}

	filterMessages(m_ui->filterLineEdit->text());
}

void ConsoleWidget::filterMessages(const QString &filter)
{
	if (!m_model)
	{
		return;
	}

	for (int i = 0; i < m_model->rowCount(); ++i)
	{
		QStandardItem *item = m_model->item(i, 0);

		if (item)
		{
			m_ui->consoleView->setRowHidden(i, m_ui->consoleView->rootIndex(), (!(item->flags() & Qt::ItemIsEnabled) || (!filter.isEmpty() && !(item->data(Qt::UserRole + 2).toString().contains(filter, Qt::CaseInsensitive) || (item->child(0, 0) && item->child(0, 0)->text().contains(filter, Qt::CaseInsensitive))))));
		}
	}
}

void ConsoleWidget::showContextMenu(const QPoint position)
{
	QMenu menu(m_ui->consoleView);
	menu.addAction(Utils::getIcon(QLatin1String("edit-copy")), tr("Copy"), this, SLOT(copyText()));
	menu.exec(m_ui->consoleView->mapToGlobal(position));
}

}
