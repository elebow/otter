#include "WindowsManager.h"
#include "SettingsManager.h"
#include "../ui/TabBarWidget.h"
#include "../ui/Window.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtWidgets/QMdiSubWindow>

namespace Otter
{

WindowsManager::WindowsManager(QMdiArea *area, TabBarWidget *tabBar, bool privateSession) : QObject(area),
	m_area(area),
	m_tabBar(tabBar),
	m_currentWindow(-1),
	m_printedWindow(-1),
	m_privateSession(privateSession)
{
	QTimer::singleShot(250, this, SLOT(open()));

	connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(setCurrentWindow(int)));
	connect(m_tabBar, SIGNAL(requestedPin(int,bool)), this, SLOT(pinWindow(int,bool)));
	connect(m_tabBar, SIGNAL(requestedClose(int)), this, SLOT(closeWindow(int)));
	connect(m_tabBar, SIGNAL(requestedCloseOther(int)), this, SLOT(closeOther(int)));
}

void WindowsManager::open(const QUrl &url, bool privateWindow)
{
	Window *window = NULL;

	if (!url.isEmpty())
	{
		window = getWindow(getCurrentWindow());

		if (window && window->isEmpty())
		{
			window->setUrl(url);
			window->setPrivate(m_privateSession || privateWindow);

			return;
		}
	}

	window = new Window(m_area);
	window->setPrivate(m_privateSession || privateWindow);

	QMdiSubWindow *mdiWindow = m_area->addSubWindow(window, Qt::CustomizeWindowHint);
	mdiWindow->showMaximized();

	const int index = m_tabBar->count();

	m_tabBar->insertTab(index, window->getTitle());
	m_tabBar->setTabData(index, QVariant::fromValue(window));
	m_tabBar->setCurrentIndex(index);

	connect(window, SIGNAL(titleChanged(QString)), this, SLOT(setTitle(QString)));
	connect(window, SIGNAL(iconChanged(QIcon)), m_tabBar, SLOT(updateTabs()));
	connect(window, SIGNAL(loadingChanged(bool)), m_tabBar, SLOT(updateTabs()));
	connect(m_tabBar->tabButton(index, QTabBar::LeftSide), SIGNAL(destroyed()), window, SLOT(deleteLater()));

	window->setUrl(url);

	emit windowAdded(index);
}

void WindowsManager::close(int index)
{
	if (index < 0)
	{
		index = getCurrentWindow();
	}

	closeWindow(index);
}

void WindowsManager::closeOther(int index)
{
	if (index < 0)
	{
		index = getCurrentWindow();
	}

	if (index >= m_tabBar->count())
	{
		return;
	}

	for (int i = (m_tabBar->count() - 1); i > index; --i)
	{
		closeWindow(i);
	}

	for (int i = 0; i < index; ++i)
	{
		closeWindow(0);
	}
}

void WindowsManager::print(int index)
{
	Window *window = getWindow(index);

	if (!window)
	{
		return;
	}

	QPrinter printer;
	QPrintDialog printDialog(&printer, m_area);
	printDialog.setWindowTitle(tr("Print Project"));

	if (printDialog.exec() != QDialog::Accepted)
	{
		return;
	}

	window->print(&printer);
}

void WindowsManager::printPreview(int index)
{
	if (index < 0)
	{
		index = getCurrentWindow();
	}

	if (index < 0 || index >= m_tabBar->count())
	{
		return;
	}

	m_printedWindow = index;

	QPrintPreviewDialog prinPreviewtDialog(m_area);
	prinPreviewtDialog.setWindowTitle(tr("Print Preview"));

	connect(&prinPreviewtDialog, SIGNAL(paintRequested(QPrinter*)), this, SLOT(printPreview(QPrinter*)));

	prinPreviewtDialog.exec();

	m_printedWindow = -1;
}

void WindowsManager::printPreview(QPrinter *printer)
{
	Window *window = getWindow(m_printedWindow);

	if (window)
	{
		window->print(printer);
	}
}

void WindowsManager::pinWindow(int index, bool pin)
{
	int offset = 0;

	for (int i = 0; i < m_tabBar->count(); ++i)
	{
		if (!m_tabBar->getTabProperty(i, "isPinned", false).toBool())
		{
			break;
		}

		++offset;
	}

	if (!pin)
	{
		m_tabBar->setTabProperty(index, "isPinned", false);
		m_tabBar->setTabText(index, m_tabBar->tabToolTip(index));
		m_tabBar->moveTab(index, offset);
		m_tabBar->updateTabs();

		return;
	}

	m_tabBar->setTabProperty(index, "isPinned", true);
	m_tabBar->setTabText(index, QString());
	m_tabBar->moveTab(index, offset);
	m_tabBar->updateTabs();
}

void WindowsManager::closeWindow(int index)
{
	if (index < 0 || index >= m_tabBar->count() || m_tabBar->getTabProperty(index, "isPinned", false).toBool())
	{
		return;
	}

	if (m_tabBar->count() == 1)
	{
		Window *window = getWindow(0);

		if (window)
		{
			window->setUrl(QUrl("about:blank"));

			return;
		}
	}

	if (index < m_currentWindow)
	{
		--m_currentWindow;
	}

	m_tabBar->removeTab(index);

	emit windowRemoved(index);
}

void WindowsManager::reload()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->reload();
	}
}

void WindowsManager::stop()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->stop();
	}
}

void WindowsManager::goBack()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->goBack();
	}
}

void WindowsManager::goForward()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->goForward();
	}
}

void WindowsManager::undo()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->undo();
	}
}

void WindowsManager::redo()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->redo();
	}
}

void WindowsManager::cut()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->cut();
	}
}

void WindowsManager::copy()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->copy();
	}
}

void WindowsManager::paste()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->paste();
	}
}

void WindowsManager::remove()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->remove();
	}
}

void WindowsManager::selectAll()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->selectAll();
	}
}

void WindowsManager::zoomIn()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->zoomIn();
	}
}

void WindowsManager::zoomOut()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->zoomOut();
	}
}

void WindowsManager::zoomOriginal()
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->zoomOriginal();
	}
}

void WindowsManager::setZoom(int zoom)
{
	Window *window = getWindow(getCurrentWindow());

	if (window)
	{
		window->setZoom(zoom);
	}
}

void WindowsManager::setCurrentWindow(int index)
{
	if (index < 0 || index >= m_tabBar->count())
	{
		index = 0;
	}

	Window *window = getWindow(m_currentWindow);

	if (window)
	{
		disconnect(window->getUndoStack(), SIGNAL(undoTextChanged(QString)), this, SIGNAL(undoTextChanged(QString)));
		disconnect(window->getUndoStack(), SIGNAL(redoTextChanged(QString)), this, SIGNAL(redoTextChanged(QString)));
		disconnect(window->getUndoStack(), SIGNAL(canUndoChanged(bool)), this, SIGNAL(canUndoChanged(bool)));
		disconnect(window->getUndoStack(), SIGNAL(canRedoChanged(bool)), this, SIGNAL(canRedoChanged(bool)));
	}

	m_currentWindow = index;

	window = getWindow(m_currentWindow);

	if (window)
	{
		QList<QMdiSubWindow*> windows = m_area->subWindowList();

		for (int i = 0; i < windows.count(); ++i)
		{
			if (window == windows.at(i)->widget())
			{
				m_area->setActiveSubWindow(windows.at(i));

				break;
			}
		}

		emit windowTitleChanged(QString("%1 - Otter").arg(window->getTitle()));
		emit undoTextChanged(window->getUndoStack()->undoText());
		emit redoTextChanged(window->getUndoStack()->redoText());
		emit canUndoChanged(window->getUndoStack()->canUndo());
		emit canRedoChanged(window->getUndoStack()->canRedo());

		connect(window->getUndoStack(), SIGNAL(undoTextChanged(QString)), this, SIGNAL(undoTextChanged(QString)));
		connect(window->getUndoStack(), SIGNAL(redoTextChanged(QString)), this, SIGNAL(redoTextChanged(QString)));
		connect(window->getUndoStack(), SIGNAL(canUndoChanged(bool)), this, SIGNAL(canUndoChanged(bool)));
		connect(window->getUndoStack(), SIGNAL(canRedoChanged(bool)), this, SIGNAL(canRedoChanged(bool)));
	}

	emit currentWindowChanged(index);
}

void WindowsManager::setTitle(const QString &title)
{
	const QString text = (title.isEmpty() ? tr("Empty") : title);
	const int index = getWindowIndex(qobject_cast<Window*>(sender()));

	if (!m_tabBar->getTabProperty(index, "isPinned", false).toBool())
	{
		m_tabBar->setTabText(index, text);
	}

	m_tabBar->setTabToolTip(index, text);

	if (index == getCurrentWindow())
	{
		emit windowTitleChanged(QString("%1 - Otter").arg(text));
	}
}

Window* WindowsManager::getWindow(int index) const
{
	if (index < 0)
	{
		index = getCurrentWindow();
	}

	if (index < 0 || index >= m_tabBar->count())
	{
		return NULL;
	}

	return qvariant_cast<Window*>(m_tabBar->tabData(index));
}

QString WindowsManager::getTitle() const
{
	Window *window = getWindow(getCurrentWindow());

	return QString("%1 - Otter").arg(window ? window->getTitle() : tr("Empty"));
}

int WindowsManager::getCurrentWindow() const
{
	return m_tabBar->currentIndex();
}

int WindowsManager::getZoom() const
{
	Window *window = getWindow(getCurrentWindow());

	return (window ? window->getZoom() : 100);
}

int WindowsManager::getWindowIndex(Window *window) const
{
	for (int i = 0; i < m_tabBar->count(); ++i)
	{
		if (window == qvariant_cast<Window*>(m_tabBar->tabData(i)))
		{
			return i;
		}
	}

	return -1;
}

bool WindowsManager::canUndo() const
{
	Window *window = getWindow(getCurrentWindow());

	return (window && window->getUndoStack()->canUndo());
}

bool WindowsManager::canRedo() const
{
	Window *window = getWindow(getCurrentWindow());

	return (window && window->getUndoStack()->canRedo());
}

}
