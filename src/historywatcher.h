/***************************************************************************

	historywatcher.h

	Watches a history file and loads it when appropriate

***************************************************************************/

#ifndef HISTORYWATCHER_H
#define HISTORYWATCHER_H

// bletchmame headers
#include "history.h"

// Qt headers
#include <QFileSystemWatcher>
#include <QObject>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class Preferences;
class TaskDispatcher;

// ======================> HistoryWatcher

class HistoryWatcher : public QObject
{
	Q_OBJECT
public:
	// ctor
	HistoryWatcher(Preferences &prefs, TaskDispatcher &taskDispatcher, QObject *parent = nullptr);

	// accessors
	const HistoryDatabase &db() const { return m_db; }

signals:
	void historyFileChanged();

private:
	class LoadHistoryTask;

	Preferences &		m_prefs;
	TaskDispatcher &	m_taskDispatcher;
	QFileSystemWatcher	m_fsw;
	HistoryDatabase		m_db;

	// private methods
	void startLoadHistoryTask(const QString &path, bool retainIfFail);
	void loadHistoryComplete(HistoryDatabase &&newDb, bool retainIfFail, bool success);
};


#endif // HISTORYWATCHER_H