/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2015 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "TransfersContentsWidget.h"
#include "ProgressBarDelegate.h"
#include "../../../core/ActionsManager.h"
#include "../../../core/Transfer.h"
#include "../../../core/Utils.h"
#include "../../../ui/ItemDelegate.h"

#include "ui_TransfersContentsWidget.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QtMath>
#include <QtCore/QQueue>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

namespace Otter
{

TransfersContentsWidget::TransfersContentsWidget(Window *window) : ContentsWidget(window),
	m_model(new QStandardItemModel(this)),
	m_isLoading(false),
	m_ui(new Ui::TransfersContentsWidget)
{
	m_ui->setupUi(this);

	QStringList labels;
	labels << QString() << tr("Filename") << tr("Size") << tr("Progress") << tr("Time") << tr("Speed") << tr("Started") << tr("Finished");

	m_model->setHorizontalHeaderLabels(labels);

	m_ui->transfersView->setModel(m_model);
	m_ui->transfersView->horizontalHeader()->setTextElideMode(Qt::ElideRight);
	m_ui->transfersView->horizontalHeader()->resizeSection(0, 30);
	m_ui->transfersView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	m_ui->transfersView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_ui->transfersView->setItemDelegate(new ItemDelegate(this));
	m_ui->transfersView->setItemDelegateForColumn(3, new ProgressBarDelegate(this));
	m_ui->transfersView->installEventFilter(this);
	m_ui->stopResumeButton->setIcon(Utils::getIcon(QLatin1String("task-ongoing")));
	m_ui->redownloadButton->setIcon(Utils::getIcon(QLatin1String("view-refresh")));

	// In order for the sizeHint method to be called, the ResizeToContents needs to be set up
	m_ui->transfersView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	const QList<Transfer*> transfers = TransfersManager::getTransfers();

	for (int i = 0; i < transfers.count(); ++i)
	{
		addTransfer(transfers.at(i));
	}

	if (!window)
	{
		m_ui->detailsWidget->hide();
	}

	connect(TransfersManager::getInstance(), SIGNAL(transferStarted(Transfer*)), this, SLOT(addTransfer(Transfer*)));
	connect(TransfersManager::getInstance(), SIGNAL(transferRemoved(Transfer*)), this, SLOT(removeTransfer(Transfer*)));
	connect(TransfersManager::getInstance(), SIGNAL(transferChanged(Transfer*)), this, SLOT(updateTransfer(Transfer*)));
	connect(m_model, SIGNAL(modelReset()), this, SLOT(updateActions()));
	connect(m_ui->transfersView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(updateActions()));
	connect(m_ui->transfersView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openTransfer(QModelIndex)));
	connect(m_ui->transfersView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
	connect(m_ui->downloadLineEdit, SIGNAL(returnPressed()), this, SLOT(startQuickTransfer()));
	connect(m_ui->stopResumeButton, SIGNAL(clicked()), this, SLOT(stopResumeTransfer()));
	connect(m_ui->redownloadButton, SIGNAL(clicked()), this, SLOT(redownloadTransfer()));
}

TransfersContentsWidget::~TransfersContentsWidget()
{
	delete m_ui;
}

void TransfersContentsWidget::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);

	switch (event->type())
	{
		case QEvent::LanguageChange:
			m_ui->retranslateUi(this);

			break;
		default:
			break;
	}
}

void TransfersContentsWidget::addTransfer(Transfer *transfer)
{
	QList<QStandardItem*> items;
	QStandardItem *item = new QStandardItem();
	item->setData(qVariantFromValue((void*) transfer), Qt::UserRole);

	items.append(item);

	item = new QStandardItem(QFileInfo(transfer->getTarget()).fileName());

	items.append(item);

	for (int i = 2; i < m_model->columnCount(); ++i)
	{
		items.append(new QStandardItem());
	}

	m_model->appendRow(items);

	if (transfer->getState() == Transfer::RunningState)
	{
		m_speeds[transfer] = QQueue<qint64>();
	}

	updateTransfer(transfer);
}

void TransfersContentsWidget::removeTransfer(Transfer *transfer)
{
	const int row = findTransfer(transfer);

	if (row >= 0)
	{
		m_model->removeRow(row);
	}

	m_speeds.remove(transfer);
}

void TransfersContentsWidget::removeTransfer()
{
	Transfer *transfer = getTransfer(m_ui->transfersView->currentIndex());

	if (transfer)
	{
		if (transfer->getState() == Transfer::RunningState && QMessageBox::warning(this, tr("Warning"), tr("This transfer is still running.\nDo you really want to remove it?"), (QMessageBox::Yes | QMessageBox::Cancel)) == QMessageBox::Cancel)
		{
			return;
		}

		m_speeds.remove(transfer);

		m_model->removeRow(m_ui->transfersView->currentIndex().row());

		TransfersManager::removeTransfer(transfer);
	}
}

void TransfersContentsWidget::updateTransfer(Transfer *transfer)
{
	const int row = findTransfer(transfer);

	if (row < 0)
	{
		return;
	}

	QString remainingTime;

	if (transfer->getState() == Transfer::RunningState)
	{
		if (!m_speeds.contains(transfer))
		{
			m_speeds[transfer] = QQueue<qint64>();
		}

		m_speeds[transfer].enqueue(transfer->getSpeed());

		if (m_speeds[transfer].count() > 10)
		{
			m_speeds[transfer].dequeue();
		}

		if (transfer->getBytesTotal() > 0)
		{
			qint64 speedSum = 0;
			const QList<qint64> speeds = m_speeds[transfer];

			for (int i = 0; i < speeds.count(); ++i)
			{
				speedSum += speeds.at(i);
			}

			speedSum /= (speeds.count());

			remainingTime = Utils::formatTime(qreal(transfer->getBytesTotal() - transfer->getBytesReceived()) / speedSum);
		}
	}
	else
	{
		m_speeds.remove(transfer);
	}

	QIcon icon;

	switch (transfer->getState())
	{
		case Transfer::RunningState:
			icon = Utils::getIcon(QLatin1String("task-ongoing"));

			break;
		case Transfer::FinishedState:
			icon = Utils::getIcon(QLatin1String("task-complete"));

			break;
		case Transfer::ErrorState:
			icon = Utils::getIcon(QLatin1String("task-reject"));

			break;
		default:
			break;
	}

	m_model->item(row, 0)->setIcon(icon);
	m_model->item(row, 1)->setText(QFileInfo(transfer->getTarget()).fileName());
	m_model->item(row, 2)->setText(Utils::formatUnit(transfer->getBytesTotal(), false, 1));
	m_model->item(row, 3)->setText((transfer->getBytesTotal() > 0) ? QString::number(qFloor(((qreal) transfer->getBytesReceived() / transfer->getBytesTotal()) * 100), 'f', 0) : QString());
	m_model->item(row, 4)->setText(remainingTime);
	m_model->item(row, 5)->setText((transfer->getState() == Transfer::RunningState) ? Utils::formatUnit(transfer->getSpeed(), true, 1) : QString());
	m_model->item(row, 6)->setText(transfer->getTimeStarted().toString(QLatin1String("yyyy-MM-dd HH:mm:ss")));
	m_model->item(row, 7)->setText(transfer->getTimeFinished().toString(QLatin1String("yyyy-MM-dd HH:mm:ss")));

	const QString tooltip = tr("<div style=\"white-space:pre;\">Source: %1\nTarget: %2\nSize: %3\nDownloaded: %4\nProgress: %5</div>").arg(transfer->getSource().toString().toHtmlEscaped()).arg(transfer->getTarget().toHtmlEscaped()).arg((transfer->getBytesTotal() > 0) ? tr("%1 (%n B)", "", transfer->getBytesTotal()).arg(Utils::formatUnit(transfer->getBytesTotal())) : QString('?')).arg(tr("%1 (%n B)", "", transfer->getBytesReceived()).arg(Utils::formatUnit(transfer->getBytesReceived()))).arg(QStringLiteral("%1%").arg(((transfer->getBytesTotal() > 0) ? (((qreal) transfer->getBytesReceived() / transfer->getBytesTotal()) * 100) : 0.0), 0, 'f', 1));

	for (int i = 0; i < m_model->columnCount(); ++i)
	{
		m_model->item(row, i)->setToolTip(tooltip);
	}

	if (m_ui->transfersView->selectionModel()->hasSelection())
	{
		updateActions();
	}

	const bool isRunning = (transfer && transfer->getState() == Transfer::RunningState);

	if (isRunning != m_isLoading)
	{
		if (isRunning)
		{
			m_isLoading = true;

			emit loadingChanged(true);
		}
		else
		{
			const QList<Transfer*> transfers = TransfersManager::getTransfers();
			bool hasRunning = false;

			for (int i = 0; i < transfers.count(); ++i)
			{
				if (transfers.at(i) && transfers.at(i)->getState() == Transfer::RunningState)
				{
					hasRunning = true;

					break;
				}
			}

			if (!hasRunning)
			{
				m_isLoading = false;

				emit loadingChanged(false);
			}
		}
	}
}

void TransfersContentsWidget::openTransfer(const QModelIndex &index)
{
	Transfer *transfer = getTransfer(index.isValid() ? index : m_ui->transfersView->currentIndex());

	if (transfer)
	{
		transfer->openTarget();
	}
}

void TransfersContentsWidget::openTransfer(QAction *action)
{
	Transfer *transfer = getTransfer(m_ui->transfersView->currentIndex());

	if (transfer && action && !action->data().isNull())
	{
		Utils::runApplication(action->data().toString(), transfer->getTarget());
	}
}

void TransfersContentsWidget::openTransferFolder(const QModelIndex &index)
{
	Transfer *transfer = getTransfer(index.isValid() ? index : m_ui->transfersView->currentIndex());

	if (transfer)
	{
		Utils::runApplication(QString(), QFileInfo(transfer->getTarget()).dir().canonicalPath());
	}
}

void TransfersContentsWidget::copyTransferInformation()
{
	QStandardItem *item = m_model->itemFromIndex(m_ui->transfersView->currentIndex());

	if (item)
	{
		QApplication::clipboard()->setText(item->toolTip().remove(QRegularExpression(QLatin1String("<[^>]*>"))), QClipboard::Selection);
	}
}

void TransfersContentsWidget::stopResumeTransfer()
{
	Transfer *transfer = getTransfer(m_ui->transfersView->selectionModel()->hasSelection() ? m_ui->transfersView->selectionModel()->currentIndex() : QModelIndex());

	if (transfer)
	{
		if (transfer->getState() == Transfer::RunningState)
		{
			transfer->stop();
		}
		else if (transfer->getState() == Transfer::ErrorState)
		{
			transfer->resume();
		}

		updateActions();
	}
}

void TransfersContentsWidget::redownloadTransfer()
{
	Transfer *transfer = getTransfer(m_ui->transfersView->selectionModel()->hasSelection() ? m_ui->transfersView->selectionModel()->currentIndex() : QModelIndex());

	if (transfer)
	{
		transfer->restart();
	}
}

void TransfersContentsWidget::startQuickTransfer()
{
	TransfersManager::startTransfer(m_ui->downloadLineEdit->text(), QString(), true, SessionsManager::isPrivate());

	m_ui->downloadLineEdit->clear();
}

void TransfersContentsWidget::clearFinishedTransfers()
{
	TransfersManager::clearTransfers();
}

void TransfersContentsWidget::showContextMenu(const QPoint &point)
{
	Transfer *transfer = getTransfer(m_ui->transfersView->indexAt(point));
	QMenu menu(this);

	if (transfer)
	{
		menu.addAction(tr("Open"), this, SLOT(openTransfer()));

		const QList<ApplicationInformation> applications = Utils::getApplicationsForMimeType(transfer->getMimeType());

		if (applications.count() > 1)
		{
			QMenu *applicationsMenu = menu.addMenu(tr("Open With"));

			for (int i = 0; i < applications.count(); ++i)
			{
				applicationsMenu->addAction(applications.at(i).icon, ((applications.at(i).name.isEmpty()) ? tr("Unknown") : applications.at(i).name))->setData(applications.at(i).command);

				if (i == 0)
				{
					applicationsMenu->addSeparator();
				}
			}

			connect(applicationsMenu, SIGNAL(triggered(QAction*)), this, SLOT(openTransfer(QAction*)));
		}

		menu.addAction(tr("Open Folder"), this, SLOT(openTransferFolder()));
		menu.addSeparator();
		menu.addAction(((transfer->getState() == Transfer::ErrorState) ? tr("Resume") : tr("Stop")), this, SLOT(stopResumeTransfer()))->setEnabled(transfer->getState() == Transfer::RunningState || transfer->getState() == Transfer::ErrorState);
		menu.addAction(tr("Redownload"), this, SLOT(redownloadTransfer()));
		menu.addSeparator();
		menu.addAction(tr("Copy Transfer Information"), this, SLOT(copyTransferInformation()));
		menu.addSeparator();
		menu.addAction(tr("Remove"), this, SLOT(removeTransfer()));
	}

	const QList<Transfer*> transfers = TransfersManager::getTransfers();
	int finishedTransfers = 0;

	for (int i = 0; i < transfers.count(); ++i)
	{
		if (transfers.at(i)->getState() == Transfer::FinishedState)
		{
			++finishedTransfers;
		}
	}

	menu.addAction(tr("Clear Finished Transfers"), this, SLOT(clearFinishedTransfers()))->setEnabled(finishedTransfers > 0);
	menu.exec(m_ui->transfersView->mapToGlobal(point));
}

void TransfersContentsWidget::updateActions()
{
	Transfer *transfer = getTransfer(m_ui->transfersView->selectionModel()->hasSelection() ? m_ui->transfersView->selectionModel()->currentIndex() : QModelIndex());

	if (transfer && transfer->getState() == Transfer::ErrorState)
	{
		m_ui->stopResumeButton->setText(tr("Resume"));
		m_ui->stopResumeButton->setIcon(Utils::getIcon(QLatin1String("task-ongoing")));
	}
	else
	{
		m_ui->stopResumeButton->setText(tr("Stop"));
		m_ui->stopResumeButton->setIcon(Utils::getIcon(QLatin1String("task-reject")));
	}

	m_ui->stopResumeButton->setEnabled(transfer && (transfer->getState() == Transfer::RunningState || transfer->getState() == Transfer::ErrorState));
	m_ui->redownloadButton->setEnabled(transfer);

	getAction(ActionsManager::CopyAction)->setEnabled(transfer);
	getAction(ActionsManager::DeleteAction)->setEnabled(transfer);

	if (transfer)
	{
		m_ui->sourceLabelWidget->setText(transfer->getSource().toString());
		m_ui->targetLabelWidget->setText(transfer->getTarget());
		m_ui->sizeLabelWidget->setText((transfer->getBytesTotal() > 0) ? tr("%1 (%n B)", "", transfer->getBytesTotal()).arg(Utils::formatUnit(transfer->getBytesTotal())) : QString('?'));
		m_ui->downloadedLabelWidget->setText(tr("%1 (%n B)", "", transfer->getBytesReceived()).arg(Utils::formatUnit(transfer->getBytesReceived())));
		m_ui->progressLabelWidget->setText(QStringLiteral("%1%").arg(((transfer->getBytesTotal() > 0) ? (((qreal) transfer->getBytesReceived() / transfer->getBytesTotal()) * 100) : 0.0), 0, 'f', 1));
	}
	else
	{
		m_ui->sourceLabelWidget->clear();
		m_ui->targetLabelWidget->clear();
		m_ui->sizeLabelWidget->clear();
		m_ui->downloadedLabelWidget->clear();
		m_ui->progressLabelWidget->clear();
	}
}

void TransfersContentsWidget::print(QPrinter *printer)
{
	m_ui->transfersView->render(printer);
}

void TransfersContentsWidget::triggerAction(int identifier, bool checked)
{
	Q_UNUSED(checked)

	switch (identifier)
	{
		case ActionsManager::CopyAction:
			if (m_ui->transfersView->hasFocus() && m_ui->transfersView->currentIndex().isValid())
			{
				copyTransferInformation();
			}
			else
			{
				QWidget *widget = focusWidget();

				if (widget->metaObject()->className() == QLatin1String("Otter::TextLabelWidget"))
				{
					TextLabelWidget *label = qobject_cast<TextLabelWidget*>(widget);

					if (label)
					{
						label->copy();
					}
				}
			}

			break;
		case ActionsManager::DeleteAction:
			removeTransfer();

			break;
		case ActionsManager::FindAction:
		case ActionsManager::QuickFindAction:
		case ActionsManager::ActivateAddressFieldAction:
			m_ui->downloadLineEdit->setFocus();
			m_ui->downloadLineEdit->selectAll();

			break;
		default:
			break;
	}
}

Transfer* TransfersContentsWidget::getTransfer(const QModelIndex &index)
{
	if (index.isValid() && m_model->item(index.row(), 0))
	{
		return static_cast<Transfer*>(m_model->item(index.row(), 0)->data(Qt::UserRole).value<void*>());
	}

	return NULL;
}

Action* TransfersContentsWidget::getAction(int identifier)
{
	if (m_actions.contains(identifier))
	{
		return m_actions[identifier];
	}

	if (identifier != ActionsManager::CopyAction && identifier != ActionsManager::DeleteAction)
	{
		return NULL;
	}

	Action *action = new Action(identifier, this);

	m_actions[identifier] = action;

	connect(action, SIGNAL(triggered()), this, SLOT(triggerAction()));

	return action;
}

QString TransfersContentsWidget::getTitle() const
{
	return tr("Transfers Manager");
}

QLatin1String TransfersContentsWidget::getType() const
{
	return QLatin1String("transfers");
}

QUrl TransfersContentsWidget::getUrl() const
{
	return QUrl(QLatin1String("about:transfers"));
}

QIcon TransfersContentsWidget::getIcon() const
{
	return Utils::getIcon(QLatin1String("transfers"), false);
}

int TransfersContentsWidget::findTransfer(Transfer *transfer) const
{
	if (!transfer)
	{
		return -1;
	}

	for (int i = 0; i < m_model->rowCount(); ++i)
	{
		if (transfer == static_cast<Transfer*>(m_model->item(i, 0)->data(Qt::UserRole).value<void*>()))
		{
			return i;
		}
	}

	return -1;
}

bool TransfersContentsWidget::isLoading() const
{
	return m_isLoading;
}

bool TransfersContentsWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->transfersView && event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

		if (keyEvent && (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return))
		{
			openTransfer();

			return true;
		}
	}

	return QWidget::eventFilter(object, event);
}

}
