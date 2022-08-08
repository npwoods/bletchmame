/***************************************************************************

	historywatcher.h

	Watches a history file and loads it when appropriate

***************************************************************************/

// bletchmame headers
#include "historywatcher.h"
#include "prefs.h"
#include "taskdispatcher.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> LoadHistoryTask

class HistoryWatcher::LoadHistoryTask : public Task
{
public:
	typedef std::unique_ptr<LoadHistoryTask> ptr;

	LoadHistoryTask(HistoryWatcher &host, const QString &path, bool retainIfFail);
	~LoadHistoryTask();

protected:
	// virtuals
	virtual void run() final override;

private:
	HistoryWatcher &		m_host;
	QString					m_path;
	bool					m_retainIfFail;
	HistoryDatabase::ptr	m_newDb;
	std::optional<bool>		m_success;
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

HistoryWatcher::HistoryWatcher(Preferences &prefs, TaskDispatcher &taskDispatcher, QObject *parent)
	: QObject(parent)
	, m_prefs(prefs)
	, m_taskDispatcher(taskDispatcher)
{
	connect(&m_fsw, &QFileSystemWatcher::fileChanged, this, [this](const QString &path)
	{
		startLoadHistoryTask(path, false);
	});
	connect(&m_prefs, &Preferences::globalPathHistoryChanged, this, [this](const QString &newPath)
	{
		startLoadHistoryTask(newPath, true);
		m_fsw.removePaths(m_fsw.files());
		m_fsw.addPath(newPath);
	});

	// load the current history
	const QString &historyPath = m_prefs.getGlobalPath(Preferences::global_path_type::HISTORY);
	m_fsw.addPath(historyPath);
	startLoadHistoryTask(historyPath, false);
}


//-------------------------------------------------
//  startLoadHistoryTask
//-------------------------------------------------

void HistoryWatcher::startLoadHistoryTask(const QString &path, bool retainIfFail)
{
	LoadHistoryTask::ptr task = std::make_unique<LoadHistoryTask>(*this, path, retainIfFail);
	m_taskDispatcher.launch(std::move(task));
}


//-------------------------------------------------
//  loadHistoryComplete
//-------------------------------------------------

void HistoryWatcher::loadHistoryComplete(HistoryDatabase &&newDb, bool retainIfFail, bool success)
{
	// are we going to want to change our currently loaded history?
	if (success || !retainIfFail)
	{
		if (success)
			m_db = std::move(newDb);
		else
			m_db.clear();
		emit historyFileChanged();
	}
}


//-------------------------------------------------
//  LoadHistoryTask ctor
//-------------------------------------------------

HistoryWatcher::LoadHistoryTask::LoadHistoryTask(HistoryWatcher &host, const QString &path, bool retainIfFail)
	: m_host(host)
	, m_path(path)
	, m_retainIfFail(retainIfFail)
{
}


//-------------------------------------------------
//  LoadHistoryTask dtor
//-------------------------------------------------

HistoryWatcher::LoadHistoryTask::~LoadHistoryTask()
{
	// in our destructor, we pass our results back to our host
	if (m_newDb && m_success)
		m_host.loadHistoryComplete(std::move(*m_newDb), m_retainIfFail, *m_success);
}


//-------------------------------------------------
//  LoadHistoryTask::run
//-------------------------------------------------

void HistoryWatcher::LoadHistoryTask::run()
{
	// load the new history
	m_newDb = std::make_unique<HistoryDatabase>();
	m_success = m_newDb->load(m_path);
}