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
#include "tableviewmanager.h"
#include "status.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QLineEdit;
class QTableWidgetItem;
class QAbstractItemModel;
class QTableView;
QT_END_NAMESPACE

class SoftwareListItemModel;
class ProfileListItemModel;
class MameVersion;
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
	void on_actionLoadState_triggered();
	void on_actionSaveState_triggered();
	void on_actionSaveScreenshot_triggered();
	void on_actionToggleRecordMovie_triggered();
	void on_actionDebugger_triggered();
	void on_actionSoftReset_triggered();
	void on_actionHardReset_triggered();
	void on_actionExit_triggered();
	void on_actionIncreaseSpeed_triggered();
	void on_actionDecreaseSpeed_triggered();
	void on_actionWarpMode_triggered();
	void on_actionFullScreen_triggered();
	void on_actionToggleSound_triggered();
	void on_actionPaths_triggered();
	void on_actionAbout_triggered();
	void on_actionRefreshMachineInfo_triggered();
	void on_actionBletchMameWebSite_triggered();
	void on_machinesTableView_activated(const QModelIndex &index);
	void on_softwareTableView_activated(const QModelIndex &index);
	void on_tabWidget_currentChanged(int index);

protected:
	virtual void closeEvent(QCloseEvent *event) override;
	virtual void keyPressEvent(QKeyEvent *event) override;

private:
	// status of MAME version checks
	enum class check_mame_info_status
	{
		SUCCESS,			// we've loaded an info DB that matches the expected MAME version
		MAME_NOT_FOUND,		// we can't find the MAME executable
		DB_NEEDS_REBUILD	// we've found MAME, but we must rebuild the database
	};

	enum class file_dialog_type
	{
		LOAD,
		SAVE
	};

	class Pauser;

	class Aspect
	{
	public:
		typedef std::unique_ptr<Aspect> ptr;

		virtual ~Aspect() { }
		virtual void start() = 0;
		virtual void stop() = 0;
	};

	template<typename TStartAction, typename TStopAction> class ActionAspect;
	template<typename TValueType, typename TObserve> class PropertySyncAspect;
	class StatusBarAspect;
	class MenuBarAspect;
	class ToggleMovieTextAspect;

	// statics
	static const float					s_throttle_rates[];
	static const QString				s_wc_saved_state;
	static const QString				s_wc_save_snapshot;
	static const QString				s_wc_record_movie;

	// variables configured at startup
	std::unique_ptr<Ui::MainWindow>		m_ui;
	Preferences							m_prefs;
	MameClient							m_client;
	SoftwareListItemModel *				m_softwareListItemModel;
	ProfileListItemModel *				m_profileListItemModel;
	std::vector<Aspect::ptr>			m_aspects;

	// information retrieved by -version
	QString								m_mame_version;

	// information retrieved by -listxml
	info::database						m_info_db;

	// other
	std::optional<status::state>		m_state;
	observable::value<bool>				m_menu_bar_shown;
	bool								m_pinging;
	const Pauser *						m_current_pauser;
	observable::value<QString>			m_current_recording_movie_filename;

	// task notifications
	bool onVersionCompleted(VersionResultEvent &event);
	bool onListXmlCompleted(const ListXmlResultEvent &event);
	bool onRunMachineCompleted(const RunMachineCompletedEvent &event);
	bool onStatusUpdate(StatusUpdateEvent &event);
	bool onChatter(const ChatterEvent &event);

	// templated property/action binding
	template<typename TStartAction, typename TStopAction>			void setupActionAspect(TStartAction &&startAction, TStopAction &&stopAction);
	template<typename TObj, typename TValueType>					void setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj::*setFunc)(TValueType), TValueType value);
	template<typename TObj, typename TValueType, typename TObserve>	void setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj::*setFunc)(TValueType), TObserve &&func);
	template<typename TObj, typename TValueType>					void setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj::*setFunc)(const TValueType &), TValueType value);
	template<typename TObj, typename TValueType, typename TObserve>	void setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj::*setFunc)(const TValueType &), TObserve &&func);

	// methods
	bool IsMameExecutablePresent() const;
	void InitialCheckMameInfoDatabase();
	check_mame_info_status CheckMameInfoDatabase();
	bool PromptForMameExecutable();
	bool refreshMameInfoDatabase();
	QMessageBox::StandardButton messageBox(const QString &message, QMessageBox::StandardButtons buttons = QMessageBox::Ok);
	bool shouldPromptOnStop() const;
	bool isMameVersionAtLeast(const MameVersion &version) const;
	void setupTableView(QTableView &tableView, QLineEdit *lineEdit, QAbstractItemModel &itemModel, const TableViewManager::Description &desc);
	void updateSoftwareList();
	info::machine GetRunningMachine() const;
	bool AttachToRootPanel() const;
	void Run(const info::machine &machine, const software_list::software *software = nullptr, void *profile = nullptr);
	QString preflightCheck() const;
	QString GetFileDialogFilename(Preferences::machine_path_type path_type, const QString &wildcard_string, file_dialog_type dlgtype);
	void FileDialogCommand(std::vector<QString> &&commands, Preferences::machine_path_type path_type, bool path_is_file, const QString &wildcard_string, file_dialog_type dlgtype);
	info::machine machineFromModelIndex(const QModelIndex &index) const;
	observable::value<QString> observeTitleBarText();
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
};

#endif // MAINWINDOW_H
