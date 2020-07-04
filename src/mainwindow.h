/***************************************************************************

	mainwindow.h

	Main BletchMAME window

***************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <memory.h>

#include "prefs.h"
#include "client.h"
#include "info.h"
#include "softwarelist.h"
#include "status.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QLineEdit;
class QTableWidgetItem;
QT_END_NAMESPACE

class CollectionViewModel;
class MameVersion;
class SoftwareListViewModel;
class VersionResultEvent;
class ListXmlResultEvent;
class RunMachineCompletedEvent;
class StatusUpdateEvent;
class ChatterEvent;


// ======================> MainWindow

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	virtual bool event(QEvent *event);

private slots:
	void on_actionStop_triggered();
	void on_actionPause_triggered();
	void on_actionDebugger_triggered();
	void on_actionSoftReset_triggered();
	void on_actionHardReset_triggered();
	void on_actionExit_triggered();
	void on_actionIncreaseSpeed_triggered();
	void on_actionDecreaseSpeed_triggered();
	void on_actionWarpMode_triggered();
	void on_actionToggleSound_triggered();
	void on_actionPaths_triggered();
	void on_actionAbout_triggered();
	void on_actionRefreshMachineInfo_triggered();
	void on_actionBletchMameWebSite_triggered();
	void on_machinesTableView_activated(const QModelIndex &index);
	void on_tabWidget_currentChanged(int index);

protected:
	virtual void closeEvent(QCloseEvent *event) override;

private:
	// status of MAME version checks
	enum class check_mame_info_status
	{
		SUCCESS,			// we've loaded an info DB that matches the expected MAME version
		MAME_NOT_FOUND,		// we can't find the MAME executable
		DB_NEEDS_REBUILD	// we've found MAME, but we must rebuild the database
	};

	class Pauser;

	// statics
	static const float						s_throttle_rates[];

	// variables configured at startup
	std::unique_ptr<Ui::MainWindow>			m_ui;
	Preferences								m_prefs;
	MameClient								m_client;
	CollectionViewModel *					m_machinesViewModel;
	SoftwareListViewModel *					m_softwareListViewModel;
	QTimer *								m_pingTimer;
	std::vector<std::function<void()>>		m_updateMenuBarItemActions;

	// information retrieved by -version
	QString									m_mame_version;

	// information retrieved by -listxml
	info::database							m_info_db;

	// other
	software_list_collection				m_software_list_collection;
	QString									m_software_list_collection_machine_name;
	std::optional<status::state>			m_state;
	observable::value<bool>					m_menu_bar_shown;
	observable::value<bool>					m_capture_mouse;
	bool									m_pinging;
	const Pauser *							m_current_pauser;

	// task notifications
	bool onVersionCompleted(VersionResultEvent &event);
	bool onListXmlCompleted(const ListXmlResultEvent &event);
	bool onRunMachineCompleted(const RunMachineCompletedEvent &event);
	bool onStatusUpdate(StatusUpdateEvent &event);
	bool onChatter(const ChatterEvent &event);

	// methods
	bool IsMameExecutablePresent() const;
	void InitialCheckMameInfoDatabase();
	check_mame_info_status CheckMameInfoDatabase();
	bool PromptForMameExecutable();
	bool refreshMameInfoDatabase();
	QMessageBox::StandardButton messageBox(const QString &message, QMessageBox::StandardButtons buttons = QMessageBox::Ok);
	bool shouldPromptOnStop() const;
	bool isMameVersionAtLeast(const MameVersion &version) const;
	void setupSearchBox(QLineEdit &lineEdit, const char *collection_view_desc_name, CollectionViewModel &collectionViewModel);
	void updateSoftwareList();
	bool AttachToRootPanel() const;
	void Run(const info::machine &machine, const software_list::software *software = nullptr, void *profile = nullptr);
	QString preflightCheck() const;
	info::machine GetMachineFromIndex(long item) const;
	const QString &GetMachineListItemText(info::machine machine, long column) const;
	void updateEmulationSession();
	void updateTitleBar();
	void updateMenuBar();
	void updateMenuBarItems();
	void updateEmulationMenuItemAction(QAction &action, std::optional<bool> checked = { }, bool enabled = true);
	void updateStatusBar();
	void Issue(const std::vector<QString> &args);
	void Issue(const std::initializer_list<std::string> &args);
	void Issue(const char *command);
	void InvokePing();
	void InvokeExit();
	void ChangePaused(bool paused);
	void ChangeThrottled(bool throttled);
	void ChangeThrottleRate(float throttle_rate);
	void ChangeThrottleRate(int adjustment);
	void ChangeSound(bool sound_enabled);
	bool IsSoundEnabled() const;
};

#endif // MAINWINDOW_H
