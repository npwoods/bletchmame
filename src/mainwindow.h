﻿/***************************************************************************

	mainwindow.h

	Main BletchMAME window

***************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// bletchmame headers
#include "auditcursor.h"
#include "auditqueue.h"
#include "devstatusdisplay.h"
#include "imagemenu.h"
#include "info.h"
#include "mainpanel.h"
#include "mameversion.h"
#include "liveinstancetracker.h"
#include "prefs.h"
#include "sessionbehavior.h"
#include "softwarelist.h"
#include "status.h"
#include "tableviewmanager.h"
#include "taskdispatcher.h"
#include "throughputtracker.h"
#include "dialogs/console.h"
#include "dialogs/stopwarning.h"

// Qt headers
#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>

// standard headers
#include <memory.h>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QLineEdit;
class QTableWidgetItem;
class QAbstractItemModel;
class QTableView;
class QWindowStateChangeEvent;
QT_END_NAMESPACE

class MainPanel;
class VersionResultEvent;
class ListXmlProgressEvent;
class ListXmlResultEvent;
class AuditDialog;
class LoadingDialog;
class RunMachineCompletedEvent;
class StatusUpdateEvent;
class ChatterEvent;


// ======================> MainWindow

class MainWindow : public QMainWindow, private IConsoleDialogHost, private IMainPanelHost, private IImageMenuHost
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

	virtual bool event(QEvent *event) override;

private slots:
	void on_actionStop_triggered();
	void on_actionPause_triggered();
	void on_actionImages_triggered();
	void on_actionQuickLoadState_triggered();
	void on_actionQuickSaveState_triggered();
	void on_actionLoadState_triggered();
	void on_actionSaveState_triggered();
	void on_actionSaveScreenshot_triggered();
	void on_actionToggleRecordMovie_triggered();
	void on_actionAuditingDisabled_triggered();
	void on_actionAuditingAutomatic_triggered();
	void on_actionAuditingManual_triggered();
	void on_actionAuditThis_triggered();
	void on_actionResetAuditingStatuses_triggered();	
	void on_actionDebugger_triggered();
	void on_actionSoftReset_triggered();
	void on_actionHardReset_triggered();
	void on_actionExit_triggered();
	void on_actionIncreaseSpeed_triggered();
	void on_actionDecreaseSpeed_triggered();
	void on_actionWarpMode_triggered();
	void on_actionFullScreen_triggered();
	void on_actionToggleSound_triggered();
	void on_actionCheats_triggered();
	void on_actionConsole_triggered();
	void on_actionJoysticksAndControllers_triggered();
	void on_actionKeyboard_triggered();
	void on_actionMiscellaneousInput_triggered();
	void on_actionConfiguration_triggered();
	void on_actionDipSwitches_triggered();
	void on_actionPaths_triggered();
	void on_actionResetToDefault_triggered();
	void on_actionImportMameIni_triggered();
	void on_actionAbout_triggered();
	void on_actionRefreshMachineInfo_triggered();
	void on_actionBletchMameWebSite_triggered();

	void on_menuAuditing_aboutToShow();

protected:
	virtual void closeEvent(QCloseEvent *event) override;
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void changeEvent(QEvent *event) override;

private:
	class Pauser;
	class ConfigurableDevicesDialogHost;
	class InputsHost;
	class SwitchesHost;
	class CheatsHost;
	class SnapshotViewEventFilter;

	class Aspect
	{
	public:
		typedef std::unique_ptr<Aspect> ptr;

		virtual ~Aspect() { }
		virtual void start() = 0;
		virtual void stop() = 0;
	};

	template<typename TStartAction, typename TStopAction> class ActionAspect;
	template<typename TObj, typename TGetValueType, typename TSetValueType, typename TSubscribable, typename TGetValue> class PropertySyncAspect;
	class StatusBarAspect;
	class MouseCaptureAspect;
	class ToggleMovieTextAspect;
	class QuickLoadSaveAspect;
	class EmulationPanelAttributesAspect;
	class Dummy;

	// statics
	static const float					s_throttle_rates[];
	static const QString				s_wc_saved_state;
	static const QString				s_wc_save_snapshot;
	static const QString				s_wc_record_movie;

	// variables configured at startup
	std::unique_ptr<Ui::MainWindow>		m_ui;
	MainPanel *							m_mainPanel;
	Preferences							m_prefs;
	TaskDispatcher						m_taskDispatcher;
	RunMachineTask::ptr					m_currentRunMachineTask;
	std::vector<Aspect::ptr>			m_aspects;

	// information retrieved by -version
	bool								m_promptIfMameNotFound;
	std::optional<MameVersion>			m_mameVersion;

	// information retrieved by -listxml
	info::database						m_info_db;

	// software lists
	std::optional<software_list_collection>	m_runningSoftwareListCollection;

	// status of running emulation
	std::unique_ptr<SessionBehavior>	m_sessionBehavior;
	std::optional<status::state>		m_state;

	// auditing
	AuditQueue							m_auditQueue;
	software_list_collection			m_auditSoftwareListCollection;
	QTimer *							m_auditTimer;
	unsigned int						m_maximumConcurrentAuditTasks;
	AuditCursor							m_auditCursor;
#if USE_PROFILER
	ThroughputTracker					m_auditThroughputTracker;
#endif // USE_PROFILER

	// other
	observable::value<bool>				m_captureMouseIfAppropriate;
	bool								m_pinging;
	DevicesStatusDisplay *				m_devicesStatusDisplay;
	const Pauser *						m_current_pauser;
	LiveInstanceTracker<LoadingDialog>	m_currentLoadingDialog;
	LiveInstanceTracker<AuditDialog>	m_currentAuditDialog;
	observable::value<QString>			m_current_recording_movie_filename;
	observable::unique_subscription		m_watch_subscription;
	std::function<void(const ChatterEvent &)>	m_on_chatter;
	observable::value<QString>			m_currentQuickState;

	// task notifications
	bool onFinalizeTask(const FinalizeTaskEvent &event);
	bool onVersionCompleted(VersionResultEvent &event);
	bool onListXmlProgress(const ListXmlProgressEvent &event);
	bool onListXmlCompleted(const ListXmlResultEvent &event);
	bool onRunMachineCompleted(const RunMachineCompletedEvent &event);
	bool onStatusUpdate(StatusUpdateEvent &event);
	bool onAuditResult(const AuditResultEvent &event);
	bool onAuditProgress(const AuditProgressEvent &event);
	bool onChatter(const ChatterEvent &event);

	// other events
	void onWindowStateChange(QWindowStateChangeEvent &event);

	// templated property/action binding
	template<typename TStartAction, typename TStopAction>				void setupActionAspect(TStartAction &&startAction, TStopAction &&stopAction);
	template<typename TObj, typename TValueType, typename TSubscribable = Dummy>						void setupPropSyncAspect(TObj &obj, TValueType(TObj::*getFunc)() const, void (TObj::*setFunc)(TValueType),			observable::value<TSubscribable>&(status::state::*getSubscribableFunc)(), TValueType value);
	template<typename TObj, typename TValueType, typename TSubscribable = Dummy, typename TGetValue>	void setupPropSyncAspect(TObj &obj, TValueType(TObj::*getFunc)() const, void (TObj::*setFunc)(TValueType),			observable::value<TSubscribable>&(status::state::*getSubscribableFunc)(), TGetValue &&func);
	template<typename TObj, typename TValueType, typename TSubscribable = Dummy>						void setupPropSyncAspect(TObj &obj, TValueType(TObj::*getFunc)() const, void (TObj::*setFunc)(const TValueType &),	observable::value<TSubscribable>&(status::state::*getSubscribableFunc)(), TValueType value);
	template<typename TObj, typename TValueType, typename TSubscribable = Dummy, typename TGetValue>	void setupPropSyncAspect(TObj &obj, TValueType(TObj::*getFunc)() const, void (TObj::*setFunc)(const TValueType &),	observable::value<TSubscribable>&(status::state::*getSubscribableFunc)(), TGetValue &&func);

	// methods
	bool IsMameExecutablePresent() const;
	void onGlobalPathEmuExecutableChanged(const QString& newPath);
	void launchVersionCheck(bool promptIfMameNotFound);
	bool loadInfoDb();
	bool promptForMameExecutable();
	bool refreshMameInfoDatabase();
	bool showStopEmulationWarning(StopWarningDialog::WarningType warningType);
	QMessageBox::StandardButton messageBox(const QString &message, QMessageBox::StandardButtons buttons = QMessageBox::Ok);
	void showInputsDialog(status::input::input_class input_class);
	void showSwitchesDialog(status::input::input_class input_class);
	bool isMameVersionAtLeast(const SimpleMameVersion &version) const;
	virtual void setChatterListener(std::function<void(const ChatterEvent &chatter)> &&func) override final;
	void watchForImageMount(const QString &tag);
	bool attachToMainWindow() const;
	QString attachWidgetId() const;
	virtual TaskDispatcher &taskDispatcher() override final { return m_taskDispatcher; }
	virtual void run(const info::machine &machine, std::unique_ptr<SessionBehavior> &&sessionBehavior) override final;
	virtual info::machine getRunningMachine() const override final;
	virtual Preferences &getPreferences() override final;
	virtual const software_list_collection &getRunningSoftwareListCollection() const override final;
	virtual status::state &getRunningState() override final;
	virtual void createImage(const QString &tag, QString &&path) override final;
	virtual void loadImage(const QString &tag, QString &&path) override final;
	virtual void unloadImage(const QString &tag) override final;
	QString preflightCheck() const;
	QString execFileDialogWithCommand(QFileDialog &dialog, std::vector<QString> &&commands);
	QString getTitleBarText();
	QString browseForMameIni();
	bool importMameIni(const QString &fileName, bool prompt);
	void issue(const std::vector<QString> &args);
	void issue(const std::initializer_list<std::string> &args);
	void waitForStatusUpdate();
	void invokePing();
	void invokeExit();
	void changePaused(bool paused);
	void changeThrottled(bool throttled);
	void changeThrottleRate(float throttle_rate);
	void changeThrottleRate(int adjustment);
	void changeSound(bool sound_enabled);
	bool onCheckForFocusSkew();
	static std::optional<WId> winGetFocus();
	static void winSetFocus(WId wid);
	QString widgetDesc(const QWidget *widget) const;
	QString widgetDesc(WId wid) const;
	void updateWindowBarsShown();
	void updateStatusBar();
	static QString runningStateText(const status::state &state);
	virtual void auditIfAppropriate(const info::machine &machine) override;
	virtual void auditIfAppropriate(const software_list::software &software) override;
	bool canAutomaticallyAudit() const;
	virtual void updateAuditTimer() override final;
	virtual void auditDialogStarted(AuditDialog &auditDialog, std::shared_ptr<AuditTask> &&auditTask) override final;
	virtual software_list_collection &getAuditSoftwareListCollection() override final;
	void auditTimerProc();
	void dispatchAuditTasks();
	void reportAuditResults(const std::vector<AuditResult> &results);
	bool reportAuditResult(const AuditResult &result);
	const QString *auditIdentifierString(const Identifier &identifier) const;
	static QString auditStatusString(AuditStatus status);
	void addLowPriorityAudits();
};

#endif // MAINWINDOW_H
