﻿/***************************************************************************

	mainwindow.cpp

	Main BletchMAME window

***************************************************************************/

// bletchmame headers
#include "mainwindow.h"
#include "mainpanel.h"
#include "mameversion.h"
#include "perfprofiler.h"
#include "ui_mainwindow.h"
#include "audittask.h"
#include "filedlgs.h"
#include "focuswatchinghook.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "versiontask.h"
#include "utility.h"
#include "dialogs/about.h"
#include "dialogs/audit.h"
#include "dialogs/cheats.h"
#include "dialogs/confdev.h"
#include "dialogs/console.h"
#include "dialogs/importmameini.h"
#include "dialogs/inputs.h"
#include "dialogs/loading.h"
#include "dialogs/paths.h"
#include "dialogs/resetprefs.h"
#include "dialogs/stopwarning.h"
#include "dialogs/switches.h"

// Qt headers
#include <QThread>
#include <QMessageBox>
#include <QTimer>
#include <QStringListModel>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QCloseEvent>
#include <QLabel>
#include <QFileDialog>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTextStream>
#include <QWindowStateChangeEvent>

// standard headers
#include <chrono>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#ifdef min
#undef min
#endif // min

#ifdef max
#undef max
#endif // max

#define LOG_FOCUSSKEW	0


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> Pauser

class MainWindow::Pauser
{
public:
	Pauser(MainWindow &host, bool actually_pause = true)
		: m_host(host)
		, m_last_pauser(host.m_current_pauser)
	{
		// if we're running and not pause, pause while the message box is up
		m_is_running = actually_pause && m_host.m_state && !m_host.m_state->paused().get();
		if (m_is_running)
			m_host.changePaused(true);

		// track the chain of pausers
		m_host.m_current_pauser = this;
	}

	~Pauser()
	{
		if (m_is_running)
			m_host.changePaused(false);
		m_host.m_current_pauser = m_last_pauser;
	}

private:
	MainWindow &	m_host;
	const Pauser *	m_last_pauser;
	bool			m_is_running;
};


// ======================> ConfigurableDevicesDialogHost

class MainWindow::ConfigurableDevicesDialogHost : public IConfigurableDevicesDialogHost
{
public:
	ConfigurableDevicesDialogHost(MainWindow &host)
		: m_host(host)
	{
	}

	virtual IImageMenuHost &imageMenuHost()
	{
		return m_host;
	}

	virtual bool startedWithHashPaths() const
	{
		return m_host.m_currentRunMachineTask->startedWithHashPaths();
	}

	virtual void changeSlots(std::map<QString, QString> &&changes)
	{
		// prepare an argument list
		std::vector<QString> args;
		args.reserve(changes.size() * 2 + 1);
		args.push_back("change_slots");

		// move the change map into the arguments
		for (auto &pair : changes)
		{
			args.push_back(std::move(pair.first));
			args.push_back(std::move(pair.second));
		}

		// and issue the command
		m_host.issue(args);
	}

private:
	MainWindow &m_host;

	const QString &getMachineName() const
	{
		return m_host.getRunningMachine().name();
	}
};


// ======================> InputsHost

class MainWindow::InputsHost : public IInputsHost
{
public:
	InputsHost(MainWindow &host) : m_host(host) { }

	virtual observable::value<std::vector<status::input>> &GetInputs() override
	{
		return m_host.m_state->inputs();
	}

	virtual const std::vector<status::input_class> &GetInputClasses() override
	{
		return m_host.m_state->input_classes().get();
	}

	virtual observable::value<bool> &GetPollingSeqChanged() override
	{
		return m_host.m_state->polling_input_seq();
	}

	virtual void SetInputSeqs(std::vector<SetInputSeqRequest> &&seqs) override
	{
		// sanity check
		assert(!seqs.empty());

		// set up arguments
		std::vector<QString> args;
		args.reserve(seqs.size() * 3 + 1);
		args.emplace_back("seq_set");

		// append sequences
		for (SetInputSeqRequest &seq : seqs)
		{
			args.emplace_back(std::move(seq.m_port_tag));
			args.emplace_back(QString::number(seq.m_mask));
			args.emplace_back(SeqTypeString(seq.m_seq_type));
			args.emplace_back(seq.m_tokens);
		}

		// invoke!
		m_host.issue(args);
	}

	virtual void StartPolling(const QString &port_tag, ioport_value mask, status::input_seq::type seq_type, const QString &start_seq_tokens) override
	{
		m_host.issue({ "seq_poll_start", port_tag, QString::number(mask), SeqTypeString(seq_type), start_seq_tokens });
	}

	virtual void StopPolling() override
	{
		m_host.issue({ "seq_poll_stop" });
	}

private:
	MainWindow &m_host;

	static const QString &SeqTypeString(status::input_seq::type seq_type)
	{
		static const QString s_standard = "standard";
		static const QString s_increment = "increment";
		static const QString s_decrement = "decrement";

		switch (seq_type)
		{
		case status::input_seq::type::STANDARD:		return s_standard;
		case status::input_seq::type::INCREMENT:	return s_increment;
		case status::input_seq::type::DECREMENT:	return s_decrement;
		default:
			throw false;
		}
	}
};


// ======================> SwitchesHost

class MainWindow::SwitchesHost : public ISwitchesHost
{
public:
	SwitchesHost(MainWindow &host)
		: m_host(host)
	{
	}

	virtual const std::vector<status::input> &GetInputs() override
	{
		return m_host.m_state->inputs().get();
	}

	virtual void SetInputValue(const QString &port_tag, ioport_value mask, ioport_value value) override
	{
		m_host.issue({ "set_input_value", port_tag, QString::number(mask), QString::number(value) });
	}

private:
	MainWindow &m_host;
};


// ======================> CheatsHost

class MainWindow::CheatsHost : public ICheatsHost
{
public:
	CheatsHost(MainWindow &host)
		: m_host(host)
	{
	}

	virtual observable::value<std::vector<status::cheat>> &getCheats() override
	{
		return m_host.m_state->cheats();
	}

	virtual void setCheatState(const QString &id, bool enabled, std::optional<std::uint64_t> parameter) override
	{
		if (parameter)
			m_host.issue({ "set_cheat_state", id, enabled ? "1" : "0", QString::number(*parameter) });
		else
			m_host.issue({ "set_cheat_state", id, enabled ? "1" : "0" });
	}

private:
	MainWindow &m_host;
};


// ======================> ActionAspect

template<typename TStartAction, typename TStopAction>
class MainWindow::ActionAspect : public Aspect
{
public:
	ActionAspect(TStartAction &&startAction, TStopAction &&stopAction)
		: m_startAction(startAction)
		, m_stopAction(stopAction)
	{
	}

	virtual void start()
	{
		m_startAction();
	}

	virtual void stop()
	{
		m_stopAction();
	}

private:
	TStartAction	m_startAction;
	TStopAction		m_stopAction;
};


// ======================> PropertySyncAspect

template<typename TObj, typename TGetValueType, typename TSetValueType, typename TSubscribable, typename TGetValueFunc>
class MainWindow::PropertySyncAspect : public Aspect
{
public:
	PropertySyncAspect(MainWindow &host, TObj &obj, TGetValueType(TObj::*getFunc)() const, void (TObj::*setFunc)(TSetValueType), observable::value<TSubscribable> &(status::state:: *getSubscribableFunc)(), TGetValueFunc &&getValueFunc)
		: m_host(host)
		, m_obj(obj)
		, m_setFunc(setFunc)
		, m_getSubscribableFunc(getSubscribableFunc)
		, m_getValueFunc(std::move(getValueFunc))
	{
		// determine the original value (to be restored when the emulation completes)
		m_originalValue = ((m_obj).*(getFunc))();
	}

	virtual void start()
	{
		// perform the initial refresh
		refresh();

		// and subscribe (if necessary)
		if (m_getSubscribableFunc && sizeof(TSubscribable) > 0)
		{
			auto &subscribable = ((*m_host.m_state).*(m_getSubscribableFunc))();
			subscribable.subscribe([this]()
			{
				refresh();
			});
		}
	}

	virtual void stop()
	{
		setValue(m_originalValue);
	}

private:
	MainWindow &		m_host;
	TObj &				m_obj;
	void (TObj:: *m_setFunc)(TSetValueType);
	observable::value<TSubscribable> &(status::state:: *m_getSubscribableFunc)();
	TGetValueFunc		m_getValueFunc;
	TGetValueType		m_originalValue;

	void refresh()
	{
		TGetValueType value = m_getValueFunc();
		setValue(value);
	}

	void setValue(TSetValueType value)
	{
		((m_obj).*(m_setFunc))(value);
	}
};


// ======================> StatusBarAspect

class MainWindow::StatusBarAspect : public Aspect
{
public:
	StatusBarAspect(MainWindow &host)
		: m_host(host)
	{
	}

	virtual void start()
	{
		m_host.m_devicesStatusDisplay->subscribe(*m_host.m_state);

		m_host.m_state->phase().subscribe(						[this]() { update(); });
		m_host.m_state->speed_percent().subscribe(				[this]() { update(); });
		m_host.m_state->effective_frameskip().subscribe(		[this]() { update(); });
		m_host.m_state->startup_text().subscribe(				[this]() { update(); });
		m_host.m_state->images().subscribe(						[this]() { update(); });
		m_host.m_state->has_input_using_mouse().subscribe(		[this]() { update(); });
		m_host.m_state->has_mouse_enabled_problem().subscribe(	[this]() { update(); });
		update();
	}

	virtual void stop()
	{
		update();
	}

private:
	MainWindow &m_host;

	void update()
	{
		m_host.updateStatusBar();
	}
};


// ======================> MouseCaptureAspect

class MainWindow::MouseCaptureAspect : public Aspect
{
public:
	MouseCaptureAspect(MainWindow &host)
		: m_host(host)
	{
	}

	virtual void start()
	{
		// mouse capturing is a bit more involved
		m_mouseCaptured = observable::observe(m_host.m_state->has_input_using_mouse() && m_host.m_captureMouseIfAppropriate);
		m_mouseCaptured.subscribe([this]()
		{
			m_host.issue({ "SET_MOUSE_ENABLED", m_mouseCaptured ? "true" : "false" });
			m_host.setCursor(m_mouseCaptured.get() ? Qt::BlankCursor : Qt::ArrowCursor);
		});
	}

	virtual void stop()
	{
	}

private:
	MainWindow &			m_host;
	observable::value<bool>	m_mouseCaptured;
};


// ======================> ToggleMovieTextAspect

class MainWindow::ToggleMovieTextAspect : public Aspect
{
public:
	ToggleMovieTextAspect(observable::value<QString> &currentRecordingMovieFilename, QAction &actionToggleRecordMovie)
		: m_currentRecordingMovieFilename(currentRecordingMovieFilename)
		, m_actionToggleRecordMovie(actionToggleRecordMovie)
	{
		m_currentRecordingMovieFilename.subscribe([this] { update(); });
	}

	virtual void start()
	{
		m_currentRecordingMovieFilename = "";
	}

	virtual void stop()
	{
		m_currentRecordingMovieFilename = "";
	}

private:
	observable::value<QString> &	m_currentRecordingMovieFilename;
	QAction &						m_actionToggleRecordMovie;

	void update()
	{
		QString text = m_currentRecordingMovieFilename.get().isEmpty()
			? QString("Record Movie...")
			: QString("Stop Recording Movie \"%1\"").arg(QDir::toNativeSeparators(m_currentRecordingMovieFilename.get()));
		m_actionToggleRecordMovie.setText(std::move(text));
	}
};


// ======================> QuickLoadSaveAspect

class MainWindow::QuickLoadSaveAspect : public Aspect
{
public:
	QuickLoadSaveAspect(observable::value<QString> &currentQuickState, QAction &quickLoadState, QAction &quickSaveState)
		: m_currentQuickState(currentQuickState)
		, m_quickLoadState(quickLoadState)
		, m_quickSaveState(quickSaveState)
	{
		m_currentQuickState.subscribe([this] { update(); });
	}

	virtual void start()
	{
		m_currentQuickState = "";
	}

	virtual void stop()
	{
		m_currentQuickState = "";
	}

private:
	observable::value<QString> &	m_currentQuickState;
	QAction &						m_quickLoadState;
	QAction &						m_quickSaveState;

	void update()
	{
		QString quickStateName;
		if (!m_currentQuickState.get().isEmpty())
		{
			// we want to only get the complete base name (note that if we triggered a state save,
			// the file might not be saved yet)
			quickStateName = QFileInfo(m_currentQuickState.get()).completeBaseName();
		}

		bool isEnabled = !quickStateName.isEmpty();
		QString quickLoadText = quickStateName.isEmpty()
			? QString("Quick Load State")
			: QString("Quick Load \"%1\"").arg(quickStateName);
		QString quickSaveText = quickStateName.isEmpty()
			? QString("Quick Save State")
			: QString("Quick Save \"%1\"").arg(quickStateName);

		m_quickLoadState.setEnabled(isEnabled);
		m_quickLoadState.setText(quickLoadText);
		m_quickSaveState.setEnabled(isEnabled);
		m_quickSaveState.setText(quickSaveText);
	}
};


// ======================> EmulationPanelAttributesAspect

class MainWindow::EmulationPanelAttributesAspect : public Aspect
{
public:
	EmulationPanelAttributesAspect(MainWindow &host)
		: m_host(host)
	{
		update();
	}

	virtual void start()
	{
		m_host.m_state->paused().subscribe([this] { update(); });
	}

	virtual void stop()
	{
		update();
	}

private:
	MainWindow &	m_host;

	void update()
	{
		// do we have a running emulation and are we not paused?
		bool attrValue = m_host.m_state && !m_host.m_state->paused().get();

		// set these attributes accordingly - this cuts down on flicker
		m_host.m_ui->emulationPanel->setAttribute(Qt::WA_OpaquePaintEvent, attrValue);
		m_host.m_ui->emulationPanel->setAttribute(Qt::WA_NoSystemBackground, attrValue);
	}
};


// ======================> Dummy

class MainWindow::Dummy
{
	// completely bogus dummy class
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

const float MainWindow::s_throttle_rates[] = { 10.0f, 5.0f, 2.0f, 1.0f, 0.5f, 0.2f, 0.1f };

const QString MainWindow::s_wc_saved_state = "MAME Saved State Files (*.sta);;All Files (*.*)";
const QString MainWindow::s_wc_save_snapshot = "PNG Files (*.png);;All Files (*.*)";
const QString MainWindow::s_wc_record_movie = "AVI Files (*.avi);;MNG Files (*.mng);;All Files (*.*)";

static const int SOUND_ATTENUATION_OFF = -32;
static const int SOUND_ATTENUATION_ON = 0;

static QEvent::Type s_checkForFocusSkewEvent = (QEvent::Type)QEvent::registerEventType();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_mainPanel(nullptr)
	, m_prefs(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)))
	, m_taskDispatcher(*this, m_prefs)
	, m_auditQueue(m_prefs, m_info_db, m_auditSoftwareListCollection, 20)
	, m_auditTimer(nullptr)
	, m_maximumConcurrentAuditTasks(std::thread::hardware_concurrency() * 3 + 8)
	, m_auditCursor(m_prefs)
#if USE_PROFILER
	, m_auditThroughputTracker(QCoreApplication::applicationDirPath() + "/auditthroughput.txt")
#endif // USE_PROFILER
	, m_pinging(false)
	, m_current_pauser(nullptr)
{
	using namespace std::chrono_literals;

	ProfilerScope prof(CURRENT_FUNCTION);

	// set up Qt form
	m_ui = std::make_unique<Ui::MainWindow>();
	m_ui->setupUi(this);

	// set the title
	setWindowTitle(getTitleBarText());

	// initial preferences read
	m_prefs.load();

	// set up the MainPanel - the UX code that is active outside the emulation
	m_mainPanel = new MainPanel(
		m_info_db,
		m_prefs,
		*this,
		m_ui->rootWidget);
	m_ui->stackedLayout->addWidget(m_mainPanel);
	m_ui->stackedLayout->setCurrentWidget(m_mainPanel);

	// set up status bar widgets owned by MainPanel
	for (QWidget &widget : m_mainPanel->statusWidgets())
		m_ui->statusBar->addPermanentWidget(&widget);

	// listen to status updates from MainPanel
	connect(m_mainPanel, &MainPanel::statusMessageChanged, this, [this](const QString &newStatus)
	{
		updateStatusBar();
	});

	// set up the ping timer
	QTimer &pingTimer = *new QTimer(this);
	connect(&pingTimer, &QTimer::timeout, this, &MainWindow::invokePing);
	setupActionAspect([&pingTimer]() { pingTimer.start(500); }, [&pingTimer]() { pingTimer.stop(); });

	// set up profiler timer
	if (PerformanceProfiler::current())
	{
		QTimer &perfProfilerTimer = *new QTimer(this);
		connect(&perfProfilerTimer, &QTimer::timeout, this, []()
		{
			PerformanceProfiler::current()->ping();
		});
		perfProfilerTimer.start(1500);
	}

	// set the proper window state from preferences
	switch (m_prefs.getWindowState())
	{
	case Preferences::WindowState::Normal:
		if (m_prefs.getSize())
			resize(*m_prefs.getSize());
		break;

	case Preferences::WindowState::Maximized:
		showMaximized();
		break;

	case Preferences::WindowState::FullScreen:
		showFullScreen();
		break;

	default:
		throw false;
	}

	// set up the audit timer
	m_auditTimer = new QTimer(this);
	m_auditTimer->setInterval(500ms);
	connect(m_auditTimer, &QTimer::timeout, this, &MainWindow::auditTimerProc);

	// workaround for silly MSVC2019 issue
	typedef observable::value<std::vector<status::image>> &(status::state:: *StatusStateImagesFunc)();
	StatusStateImagesFunc status_state_images = &status::state::images;

	// setup properties that pertain to runtime behavior
	setupPropSyncAspect(*m_ui->stackedLayout,					&QStackedLayout::currentWidget,	&QStackedLayout::setCurrentWidget,	{ },								[this]() { return attachToMainWindow() ? nullptr : m_ui->emulationPanel; });
	setupPropSyncAspect((QWidget &) *this,						&QWidget::windowTitle,			&QWidget::setWindowTitle,			&status::state::paused,				[this]() { return getTitleBarText(); });

	// actions
	setupPropSyncAspect(*m_ui->actionStop,						&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionPause,						&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionPause,						&QAction::isChecked,			&QAction::setChecked,				&status::state::paused,				[this]() { return m_state->paused().get(); });
	setupPropSyncAspect(*m_ui->actionImages,					&QAction::isEnabled,			&QAction::setEnabled,				status_state_images,				[this]() { return m_state->images().get().size() > 0;});
	setupPropSyncAspect(*m_ui->actionLoadState,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionSaveState,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionSaveScreenshot,			&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionToggleRecordMovie,			&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionAuditingDisabled,			&QAction::isEnabled,			&QAction::setEnabled,				{ },								false);
	setupPropSyncAspect(*m_ui->actionAuditingAutomatic,			&QAction::isEnabled,			&QAction::setEnabled,				{ },								false);
	setupPropSyncAspect(*m_ui->actionAuditingManual,			&QAction::isEnabled,			&QAction::setEnabled,				{ },								false);
	setupPropSyncAspect(*m_ui->actionAuditThis,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								false);
	setupPropSyncAspect(*m_ui->actionDebugger,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionSoftReset,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionHardReset,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionIncreaseSpeed,				&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionDecreaseSpeed,				&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionWarpMode,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionToggleSound,				&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionToggleSound,				&QAction::isChecked,			&QAction::setChecked,				&status::state::sound_attenuation,	[this]() { return m_state->sound_attenuation().get() != SOUND_ATTENUATION_OFF; });
	setupPropSyncAspect(*m_ui->actionCheats,					&QAction::isEnabled,			&QAction::setEnabled,				&status::state::cheats,				[this]() { return m_state->cheats().get().size() > 0; });
	setupPropSyncAspect(*m_ui->actionConsole,					&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionJoysticksAndControllers,	&QAction::isEnabled,			&QAction::setEnabled,				&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::CONTROLLER); });
	setupPropSyncAspect(*m_ui->actionKeyboard,					&QAction::isEnabled,			&QAction::setEnabled,				&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::KEYBOARD); });
	setupPropSyncAspect(*m_ui->actionMiscellaneousInput,		&QAction::isEnabled,			&QAction::setEnabled,				&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::MISC); });
	setupPropSyncAspect(*m_ui->actionConfiguration,				&QAction::isEnabled,			&QAction::setEnabled,				&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::CONFIG); });
	setupPropSyncAspect(*m_ui->actionDipSwitches,				&QAction::isEnabled,			&QAction::setEnabled,				&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::DIPSWITCH); });
	setupPropSyncAspect(*m_ui->actionRefreshMachineInfo,		&QAction::isEnabled,			&QAction::setEnabled,				{ },								false);

	// special setup for throttle dynamic menu
	QAction &throttleSeparator = *m_ui->menuThrottle->actions()[0];
	for (size_t i = 0; i < std::size(s_throttle_rates); i++)
	{
		float throttle_rate = s_throttle_rates[i];
		QString text = QString::number((int)(throttle_rate * 100)) + "%";
		QAction &action = *new QAction(text, m_ui->menuThrottle);
		m_ui->menuThrottle->insertAction(&throttleSeparator, &action);
		action.setEnabled(false);
		action.setCheckable(true);

		connect(&action, &QAction::triggered, this, [this, throttle_rate]() { changeThrottleRate(throttle_rate); });
		setupPropSyncAspect(action, &QAction::isEnabled, &QAction::setEnabled, { },								true);
		setupPropSyncAspect(action, &QAction::isChecked, &QAction::setChecked, &status::state::throttle_rate,	[this, throttle_rate]() { return m_state->throttle_rate().get() == throttle_rate; });
	}

	// special setup for frameskip dynamic menu
	for (int i = -1; i <= 10; i++)
	{
		QString value = i == -1 ? "auto" : QString::number(i);
		QString text = i == -1 ? "Auto" : QString::number(i);
		QAction &action = *m_ui->menuFrameSkip->addAction(text);
		action.setEnabled(false);
		action.setCheckable(true);

		connect(&action, &QAction::triggered, this, [this, value{ value.toStdString()}]() { issue({ "frameskip", value }); });
		setupPropSyncAspect(action, &QAction::isEnabled, &QAction::setEnabled, { },							true);
		setupPropSyncAspect(action, &QAction::isChecked, &QAction::setChecked, &status::state::frameskip,	[this, value{std::move(value)}]() { return m_state->frameskip().get() == value; });
	}

	// set up other miscellaneous aspects
	m_aspects.push_back(std::make_unique<StatusBarAspect>(*this));
	m_aspects.push_back(std::make_unique<MouseCaptureAspect>(*this));
	m_aspects.push_back(std::make_unique<ToggleMovieTextAspect>(m_current_recording_movie_filename, *m_ui->actionToggleRecordMovie));
	m_aspects.push_back(std::make_unique<QuickLoadSaveAspect>(m_currentQuickState, *m_ui->actionQuickLoadState, *m_ui->actionQuickSaveState));
	m_aspects.push_back(std::make_unique<EmulationPanelAttributesAspect>(*this));

	// connect the signals on the devices status display
	m_devicesStatusDisplay = new DevicesStatusDisplay(*this, this);
	connect(m_devicesStatusDisplay, &DevicesStatusDisplay::addWidget, this, [this](QWidget &widget)
	{
		m_ui->statusBar->addPermanentWidget(&widget);
	});

	// prepare the main tab
	m_info_db.addOnChangedHandler([this]()
	{
		m_mainPanel->updateTabContents();
		m_auditQueue.bumpCookie();
	});

	// monitor general state
	connect(&m_prefs, &Preferences::selectedTabChanged, this, [this](Preferences::list_view_type newSelectedTab)
	{
		AuditableListItemModel *newModel = m_mainPanel->currentAuditableListItemModel();
		m_auditCursor.setListItemModel(newModel);
		updateAuditTimer();
	});
	connect(&m_prefs, &Preferences::windowBarsShownChanged, this, [this](bool windowBarsShown)
	{
		updateWindowBarsShown();
	});
	updateWindowBarsShown();
	connect(&m_prefs, &Preferences::auditingStateChanged, this, [this]()
	{
		// audit statuses may have changed
		m_mainPanel->machineAuditStatusesChanged();
		m_mainPanel->softwareAuditStatusesChanged();

		// we may need to kick the timer
		updateAuditTimer();
	});

	// monitor prefs changes
	connect(&m_prefs, &Preferences::globalPathEmuExecutableChanged, this, [this](const QString &newPath)
	{
		onGlobalPathEmuExecutableChanged(newPath);
	});
	connect(&m_prefs, &Preferences::globalPathRomsChanged, this, [this](const QString &newPath)
	{
		// reset machines and software
		m_prefs.bulkDropMachineAuditStatuses([this](const QString &machineName)
		{
			std::optional<info::machine> machine = m_info_db.find_machine(machineName);
			return machine && (!machine->roms().empty() || !machine->disks().empty());
		});
		m_prefs.bulkDropSoftwareAuditStatuses();
	});
	connect(&m_prefs, &Preferences::globalPathSamplesChanged, this, [this](const QString &newPath)
	{
		// reset machines
		m_prefs.bulkDropMachineAuditStatuses([this](const QString &machineName)
		{
			std::optional<info::machine> machine = m_info_db.find_machine(machineName);
			return machine && !machine->samples().empty();
		});
	});

	// monitor bulk audit changes
	connect(&m_prefs, &Preferences::bulkDroppedMachineAuditStatuses, this, [this]()
	{
		m_mainPanel->machineAuditStatusesChanged();
		updateAuditTimer();
	});
	connect(&m_prefs, &Preferences::bulkDroppedSoftwareAuditStatuses, this, [this]()
	{
		m_mainPanel->softwareAuditStatusesChanged();
		updateAuditTimer();
	});

	// prep the audit cursor
	AuditableListItemModel *newModel = m_mainPanel->currentAuditableListItemModel();
	m_auditCursor.setListItemModel(newModel);

#ifdef Q_OS_WINDOWS
	// windows-specific steps to ensure that running emulations don't lose focus
	{
		WinFocusWatchingHook &hook = *new WinFocusWatchingHook(this);
		connect(&hook, &WinFocusWatchingHook::winFocusChanged, this, [this](WId newWindow, WId oldWindow)
		{
			if (LOG_FOCUSSKEW)
				qDebug() << QString("winFocusChanged: newWindow=%1 oldWindow=%2").arg(widgetDesc(newWindow), widgetDesc(oldWindow));

			if (newWindow == winId())
			{
				auto event = std::make_unique<QEvent>(s_checkForFocusSkewEvent);
				QCoreApplication::postEvent(this, event.release());
			}
		});
	}
#endif // Q_OS_WINDOWS

	// load the info DB
	loadInfoDb();

	// and launch a version check - most of the time this should just confirm that nothing has changed
	launchVersionCheck(true);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MainWindow::~MainWindow()
{
	m_prefs.save();
}


//-------------------------------------------------
//  setupActionAspect
//-------------------------------------------------

template<typename TStartAction, typename TStopAction>			
void MainWindow::setupActionAspect(TStartAction &&startAction, TStopAction &&stopAction)
{
	Aspect::ptr action = std::make_unique<ActionAspect<TStartAction, TStopAction>>(
		std::move(startAction),
		std::move(stopAction));
	m_aspects.push_back(std::move(action));
}


//-------------------------------------------------
//  setupPropSyncAspect
//-------------------------------------------------

template<typename TObj, typename TValueType, typename TSubscribable>
void MainWindow::setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj:: *setFunc)(TValueType), observable::value<TSubscribable> &(status::state:: *getSubscribableFunc)(), TValueType value)
{
	setupPropSyncAspect(obj, getFunc, setFunc, getSubscribableFunc, [value]() { return value; });
}


template<typename TObj, typename TValueType, typename TSubscribable, typename TGetValue>
void MainWindow::setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj:: *setFunc)(TValueType), observable::value<TSubscribable> &(status::state:: *getSubscribableFunc)(), TGetValue &&func)
{
	Aspect::ptr aspect = std::make_unique<PropertySyncAspect<TObj, TValueType, TValueType, TSubscribable, TGetValue>>(
		*this,
		obj,
		getFunc,
		setFunc,
		getSubscribableFunc,
		std::move(func));

	// and add it to the list
	m_aspects.push_back(std::move(aspect));
}


template<typename TObj, typename TValueType, typename TSubscribable>	
void MainWindow::setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj:: *setFunc)(const TValueType &), observable::value<TSubscribable> &(status::state:: *getSubscribableFunc)(), TValueType value)
{
	setupPropSyncAspect(obj, getFunc, setFunc, getSubscribableFunc, [value]() { return value; });
}


template<typename TObj, typename TValueType, typename TSubscribable, typename TGetValue>
void MainWindow::setupPropSyncAspect(TObj &obj, TValueType(TObj:: *getFunc)() const, void (TObj:: *setFunc)(const TValueType &), observable::value<TSubscribable> &(status::state:: *getSubscribableFunc)(), TGetValue &&func)
{
	Aspect::ptr aspect = std::make_unique<PropertySyncAspect<TObj, TValueType, const TValueType &, TSubscribable, TGetValue>>(
		*this,
		obj,
		getFunc,
		setFunc,
		getSubscribableFunc,
		std::move(func));

	// and add it to the list
	m_aspects.push_back(std::move(aspect));
}


//-------------------------------------------------
//  on_actionStop_triggered
//-------------------------------------------------

void MainWindow::on_actionStop_triggered()
{
	if (!showStopEmulationWarning(StopWarningDialog::WarningType::Stop))
		return;

	invokeExit();
}


//-------------------------------------------------
//  on_actionPause_triggered
//-------------------------------------------------

void MainWindow::on_actionPause_triggered()
{
	changePaused(!m_state->paused().get());
}


//-------------------------------------------------
//  on_actionImages_triggered
//-------------------------------------------------

void MainWindow::on_actionImages_triggered()
{
	Pauser pauser(*this);
	ConfigurableDevicesDialogHost images_host(*this);
	ConfigurableDevicesDialog dialog(*this, images_host, false);
	dialog.exec();
}


//-------------------------------------------------
//  on_actionQuickLoadState_triggered
//-------------------------------------------------

void MainWindow::on_actionQuickLoadState_triggered()
{
	if (QFileInfo(m_currentQuickState.get()).exists())
		issue({ "state_load", m_currentQuickState.get() });
}


//-------------------------------------------------
//  on_actionQuickSaveState_triggered
//-------------------------------------------------

void MainWindow::on_actionQuickSaveState_triggered()
{
	issue({ "state_save", m_currentQuickState.get() });
}


//-------------------------------------------------
//  on_actionLoadState_triggered
//-------------------------------------------------

void MainWindow::on_actionLoadState_triggered()
{
	// prepare the dialog
	QFileDialog dialog(this, "Load State", QString(), s_wc_saved_state);
	associateFileDialogWithMachinePrefs(dialog, m_prefs, getRunningMachine(), Preferences::machine_path_type::LAST_SAVE_STATE);
	dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
	dialog.setFileMode(QFileDialog::FileMode::ExistingFile);

	// execute the dialog
	QString path = execFileDialogWithCommand(dialog, { "state_load" });
	if (!path.isEmpty())
		m_currentQuickState = std::move(path);
}


//-------------------------------------------------
//  on_actionSaveState_triggered
//-------------------------------------------------

void MainWindow::on_actionSaveState_triggered()
{
	// prepare the dialog
	QFileDialog dialog(this, "Save State", QString(), s_wc_saved_state);
	associateFileDialogWithMachinePrefs(dialog, m_prefs, getRunningMachine(), Preferences::machine_path_type::LAST_SAVE_STATE);
	dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
	dialog.setFileMode(QFileDialog::FileMode::AnyFile);

	// execute the dialog
	QString path = execFileDialogWithCommand(dialog, { "state_save" });
	if (!path.isEmpty())
		m_currentQuickState = std::move(path);
}


//-------------------------------------------------
//  on_actionSaveScreenshot_triggered
//-------------------------------------------------

void MainWindow::on_actionSaveScreenshot_triggered()
{
	// find a snapshot dir
	QStringList snapshotPaths = m_prefs.getSplitPaths(Preferences::global_path_type::SNAPSHOTS);
	QString snapDir;
	for (QString &x : snapshotPaths)
	{
		if (QFileInfo(x).isDir())
		{
			snapDir = std::move(x);
			break;
		}
	}

	// prepare the dialog
	QFileDialog dialog(this, "Save Snapshot", snapDir, s_wc_save_snapshot);
	dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
	dialog.setFileMode(QFileDialog::FileMode::AnyFile);
	if (snapDir.isEmpty())
		associateFileDialogWithMachinePrefs(dialog, m_prefs, getRunningMachine(), Preferences::machine_path_type::WORKING_DIRECTORY);

	// prep a unique name for this screenshot
	QFileInfo fi = getUniqueFileName(
		dialog.directory().absolutePath(),
		getRunningMachine().name(),
		"png");
	dialog.selectFile(fi.fileName());

	// execute the dialog
	execFileDialogWithCommand(dialog, { "save_snapshot", "0" });
}


//-------------------------------------------------
//  on_actionToggleRecordMovie_triggered
//-------------------------------------------------

void MainWindow::on_actionToggleRecordMovie_triggered()
{
	// Are we the required version? 
	if (!isMameVersionAtLeast(MameVersion::Capabilities::HAS_TOGGLE_MOVIE))
	{
		QString message = QString("Recording movies requires %1 or newer to function")
			.arg(MameVersion::Capabilities::HAS_TOGGLE_MOVIE.toPrettyString());
		messageBox(message);
		return;
	}

	// We're toggling the movie status... so are we recording?
	if (m_state->is_recording())
	{
		// If so, stop the recording
		issue({ "end_recording" });
		m_current_recording_movie_filename = "";
	}
	else
	{
		enum class recording_type
		{
			AVI,
			MNG
		};

		// if not, prepare a file dialog...
		Pauser pauser(*this);
		QFileDialog dialog(this, "Record Movie", QString(), s_wc_record_movie);
		associateFileDialogWithMachinePrefs(dialog, m_prefs, getRunningMachine(), Preferences::machine_path_type::WORKING_DIRECTORY);
		dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
		dialog.setFileMode(QFileDialog::FileMode::AnyFile);

		// and run the dialog
		dialog.exec();
		if (dialog.result() == QDialog::DialogCode::Accepted)
		{
			// determine the recording file type
			QString path = dialog.selectedFiles().first();
			QString extension = QFileInfo(path).suffix().toUpper();
			recording_type recordingType = extension == "MNG"
				? recording_type::MNG
				: recording_type::AVI;

			// convert to a string
			QString recordingTypeString;
			switch (recordingType)
			{
			case recording_type::AVI:
				recordingTypeString = "avi";
				break;
			case recording_type::MNG:
				recordingTypeString = "mng";
				break;
			default:
				throw false;
			}

			// and issue the command
			QString nativePath = QDir::toNativeSeparators(path);
			issue({ "begin_recording", std::move(nativePath), recordingTypeString });
			m_current_recording_movie_filename = std::move(path);
		}
	}
}


//-------------------------------------------------
//  on_menuAuditing_aboutToShow
//-------------------------------------------------

void MainWindow::on_menuAuditing_aboutToShow()
{
	// set the checkmark for the current AuditingState
	Preferences::AuditingState auditingState = m_prefs.getAuditingState();
	m_ui->actionAuditingDisabled->setChecked(auditingState == Preferences::AuditingState::Disabled);
	m_ui->actionAuditingAutomatic->setChecked(auditingState == Preferences::AuditingState::Automatic);
	m_ui->actionAuditingManual->setChecked(auditingState == Preferences::AuditingState::Manual);

	// identify the currently selected auditable
	std::optional<info::machine> selectedMachine;
	const software_list::software *selectedSoftware;
	const QString *auditTargetText = nullptr;
	switch (m_prefs.getSelectedTab())
	{
	case Preferences::list_view_type::MACHINE:
		selectedMachine = m_mainPanel->currentlySelectedMachine();
		if (selectedMachine)
			auditTargetText = &selectedMachine->description();
		break;

	case Preferences::list_view_type::SOFTWARELIST:
		selectedSoftware = m_mainPanel->currentlySelectedSoftware();
		if (selectedSoftware)
			auditTargetText = &selectedSoftware->description();
		break;

	case Preferences::list_view_type::PROFILE:
		// do nothing
		break;
	}

	// update the "Audit This" item
	QString actionAuditThisText = MainPanel::auditThisActionText(auditTargetText ? QString(*auditTargetText) : QString());
	m_ui->actionAuditThis->setEnabled(auditTargetText);
	m_ui->actionAuditThis->setText(actionAuditThisText);
}


//-------------------------------------------------
//  on_actionAuditingDisabled_triggered
//-------------------------------------------------

void MainWindow::on_actionAuditingDisabled_triggered()
{
	m_prefs.setAuditingState(Preferences::AuditingState::Disabled);
}


//-------------------------------------------------
//  on_actionAuditingAutomatic_triggered
//-------------------------------------------------

void MainWindow::on_actionAuditingAutomatic_triggered()
{
	m_prefs.setAuditingState(Preferences::AuditingState::Automatic);
}


//-------------------------------------------------
//  on_actionAuditingManual_triggered
//-------------------------------------------------

void MainWindow::on_actionAuditingManual_triggered()
{
	m_prefs.setAuditingState(Preferences::AuditingState::Manual);
}


//-------------------------------------------------
//  on_actionAuditThis_triggered
//-------------------------------------------------

void MainWindow::on_actionAuditThis_triggered()
{
	std::optional<info::machine> selectedMachine;
	const software_list::software *selectedSoftware;

	switch (m_prefs.getSelectedTab())
	{
	case Preferences::list_view_type::MACHINE:
		selectedMachine = m_mainPanel->currentlySelectedMachine();
		if (selectedMachine)
			m_mainPanel->manualAudit(*selectedMachine);
		break;

	case Preferences::list_view_type::SOFTWARELIST:
		selectedSoftware = m_mainPanel->currentlySelectedSoftware();
		if (selectedSoftware)
			m_mainPanel->manualAudit(*selectedSoftware);
		break;

	case Preferences::list_view_type::PROFILE:
		// should not get here; the menu should be disabled
		break;
	}
}


//-------------------------------------------------
//  on_actionResetAuditingStatuses_triggered
//-------------------------------------------------

void MainWindow::on_actionResetAuditingStatuses_triggered()
{
	// reset machines and software
	m_prefs.bulkDropMachineAuditStatuses();
	m_prefs.bulkDropSoftwareAuditStatuses();
}


//-------------------------------------------------
//  on_actionDebugger_triggered
//-------------------------------------------------

void MainWindow::on_actionDebugger_triggered()
{
	issue({ "debugger" });
}


//-------------------------------------------------
//  on_actionSoftReset_triggered
//-------------------------------------------------

void MainWindow::on_actionSoftReset_triggered()
{
	issue({ "soft_reset" });
}


//-------------------------------------------------
//  on_actionHard_Reset_triggered
//-------------------------------------------------

void MainWindow::on_actionHardReset_triggered()
{
	issue({ "hard_reset" });
}


//-------------------------------------------------
//  on_actionExit_triggered
//-------------------------------------------------

void MainWindow::on_actionExit_triggered()
{
	close();
}


//-------------------------------------------------
//  on_actionIncreaseSpeed_triggered
//-------------------------------------------------

void MainWindow::on_actionIncreaseSpeed_triggered()
{
	changeThrottleRate(-1);
}


//-------------------------------------------------
//  on_actionDecreaseSpeed_triggered
//-------------------------------------------------

void MainWindow::on_actionDecreaseSpeed_triggered()
{
	changeThrottleRate(+1);
}


//-------------------------------------------------
//  on_actionWarpMode_triggered
//-------------------------------------------------

void MainWindow::on_actionWarpMode_triggered()
{
	changeThrottled(!m_state->throttled());
}


//-------------------------------------------------
//  on_actionFullScreen_triggered
//-------------------------------------------------

void MainWindow::on_actionFullScreen_triggered()
{
	// figure out the new value
	bool currentFullScreen = (windowState() & Qt::WindowFullScreen) != 0;
	bool newFullScreen = !currentFullScreen;

	// change the window state
	if (newFullScreen)
		showFullScreen();
	else
		showNormal();

	// and update the menu item
	m_ui->actionFullScreen->setChecked(newFullScreen);
}


//-------------------------------------------------
//  on_actionToggleSound_triggered
//-------------------------------------------------

void MainWindow::on_actionToggleSound_triggered()
{
	bool isEnabled = m_ui->actionToggleSound->isEnabled();
	changeSound(!isEnabled);
}


//-------------------------------------------------
//  on_actionCheats_triggered
//-------------------------------------------------

void MainWindow::on_actionCheats_triggered()
{
	Pauser pauser(*this);
	CheatsHost host(*this);
	CheatsDialog dialog(this, host);
	dialog.exec();
}


//-------------------------------------------------
//  on_actionConsole_triggered
//-------------------------------------------------

void MainWindow::on_actionConsole_triggered()
{
	ConsoleDialog dialog(this, RunMachineTask::ptr(m_currentRunMachineTask), *this);
	dialog.exec();
}


//-------------------------------------------------
//  on_actionJoysticksAndControllers_triggered
//-------------------------------------------------

void MainWindow::on_actionJoysticksAndControllers_triggered()
{
	showInputsDialog(status::input::input_class::CONTROLLER);
}


//-------------------------------------------------
//  on_actionKeyboard_triggered
//-------------------------------------------------

void MainWindow::on_actionKeyboard_triggered()
{
	showInputsDialog(status::input::input_class::KEYBOARD);
}


//-------------------------------------------------
//  on_actionMiscellaneousInput_triggered
//-------------------------------------------------

void MainWindow::on_actionMiscellaneousInput_triggered()
{
	showInputsDialog(status::input::input_class::MISC);
}


//-------------------------------------------------
//  on_actionConfiguration_triggered
//-------------------------------------------------

void MainWindow::on_actionConfiguration_triggered()
{
	showSwitchesDialog(status::input::input_class::CONFIG);
}


//-------------------------------------------------
//  on_actionDipSwitches_triggered
//-------------------------------------------------

void MainWindow::on_actionDipSwitches_triggered()
{
	showSwitchesDialog(status::input::input_class::DIPSWITCH);
}


//-------------------------------------------------
//  on_actionPaths_triggered
//-------------------------------------------------

void MainWindow::on_actionPaths_triggered()
{
	Pauser pauser(*this);
	PathsDialog dialog(*this, m_prefs);
	dialog.exec();
	if (dialog.result() == QDialog::DialogCode::Accepted)
	{
		dialog.persist();
		m_prefs.save();
	}
}


//-------------------------------------------------
//  on_actionResetToDefault_triggered
//-------------------------------------------------

void MainWindow::on_actionResetToDefault_triggered()
{
	Pauser pauser(*this);
	ResetPreferencesDialog dialog(*this);
	dialog.exec();
	if (dialog.result() == QDialog::DialogCode::Accepted)
	{
		m_prefs.resetToDefaults(
			dialog.isResetUiChecked(),
			dialog.isResetPathsChecked(),
			dialog.isResetFoldersChecked());
	}
}


//-------------------------------------------------
//  on_actionImportMameIni_triggered
//-------------------------------------------------

void MainWindow::on_actionImportMameIni_triggered()
{
	Pauser pauser(*this);

	// browse for the MAME INI
	QString fileName = browseForMameIni();

	// if we got something, present the import UI
	if (!fileName.isEmpty())
		importMameIni(fileName, false);
}


//-------------------------------------------------
//  on_actionAbout_triggered
//-------------------------------------------------

void MainWindow::on_actionAbout_triggered()
{
	AboutDialog dlg(this, m_mameVersion);
	dlg.exec();
}


//-------------------------------------------------
//  on_actionRefreshMachineInfo_triggered
//-------------------------------------------------

void MainWindow::on_actionRefreshMachineInfo_triggered()
{
	refreshMameInfoDatabase();
}


//-------------------------------------------------
//  on_actionBletchMameWebSite_triggered
//-------------------------------------------------

void MainWindow::on_actionBletchMameWebSite_triggered()
{
	QDesktopServices::openUrl(QUrl("https://www.bletchmame.org/"));
}


//-------------------------------------------------
//  event
//-------------------------------------------------

bool MainWindow::event(QEvent *event)
{
	std::optional<bool> result = { };
	if (event->type() == FinalizeTaskEvent::eventId())
	{
		result = onFinalizeTask(static_cast<FinalizeTaskEvent &>(*event));
	}
	else if (event->type() == VersionResultEvent::eventId())
	{
		result = onVersionCompleted(static_cast<VersionResultEvent &>(*event));
	}
	else if (event->type() == ListXmlProgressEvent::eventId())
	{
		result = onListXmlProgress(static_cast<ListXmlProgressEvent &>(*event));
	}
	else if (event->type() == ListXmlResultEvent::eventId())
	{
		result = onListXmlCompleted(static_cast<ListXmlResultEvent &>(*event));
	}
	else if (event->type() == RunMachineCompletedEvent::eventId())
	{
		result = onRunMachineCompleted(static_cast<RunMachineCompletedEvent &>(*event));
	}
	else if (event->type() == StatusUpdateEvent::eventId())
	{
		result = onStatusUpdate(static_cast<StatusUpdateEvent &>(*event));
	}
	else if (event->type() == AuditResultEvent::eventId())
	{
		result = onAuditResult(static_cast<AuditResultEvent &>(*event));
	}
	else if (event->type() == AuditProgressEvent::eventId())
	{
		result = onAuditProgress(static_cast<AuditProgressEvent &>(*event));
	}
	else if (event->type() == ChatterEvent::eventId())
	{
		result = onChatter(static_cast<ChatterEvent &>(*event));
	}
	else if (event->type() == s_checkForFocusSkewEvent)
	{
		result = onCheckForFocusSkew();
	}

	// if we have a result, we've handled the event; otherwise we have to pass it on
	// to QMainWindow::event()
	return result
		? *result
		: QMainWindow::event(event);
}


//-------------------------------------------------
//  IsMameExecutablePresent
//-------------------------------------------------

bool MainWindow::IsMameExecutablePresent() const
{
	const QString &path = m_prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
	return !path.isEmpty() && QFileInfo(path).exists();
}


//-------------------------------------------------
//  onGlobalPathEmuExecutableChanged
//-------------------------------------------------

void MainWindow::onGlobalPathEmuExecutableChanged(const QString &newPath)
{
	// is there a MAME.ini file in that directory?  if so, give the user an opportunity to import it
	QDir newMameDir = QFileInfo(newPath).absoluteDir();
	QFileInfo candidateMameIni(newMameDir.filePath("mame.ini"));
	if (candidateMameIni.exists())
		importMameIni(candidateMameIni.filePath(), true);

	// start a version check
	launchVersionCheck(false);
}


//-------------------------------------------------
//  launchVersionCheck - launches a task to check
//	MAME's version
//-------------------------------------------------

void MainWindow::launchVersionCheck(bool promptIfMameNotFound)
{
	// record if we want to prompt if MAME is not found; this is the case on initial
	// startup but it is not the case if the user consciously messed with the paths
	m_promptIfMameNotFound = promptIfMameNotFound;

	// launch the actual task
	Task::ptr task = createVersionTask();
	m_taskDispatcher.launch(std::move(task));
}


//-------------------------------------------------
//  loadInfoDb
//-------------------------------------------------

bool MainWindow::loadInfoDb()
{
	// sanity check; bail if we're running
	if (m_state)
		return false;

	// load the info DB
	QString dbPath = m_prefs.getMameXmlDatabasePath();
	bool success = m_info_db.load(dbPath, m_mameVersion? m_mameVersion->toString() : "");

	// special case for when we're starting up - if we successfully load Info DB but we have
	// not determined the MAME version yet, lets assume that Info DB is correct
	if (success && !m_mameVersion)
		m_mameVersion = MameVersion(m_info_db.version());

	return success;
}


//-------------------------------------------------
//  promptForMameExecutable
//-------------------------------------------------

bool MainWindow::promptForMameExecutable()
{
	// prompt the user
	QString path = PathsDialog::browseForPathDialog(*this, Preferences::global_path_type::EMU_EXECUTABLE, m_prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE));
	if (path.isEmpty())
		return false;

	// set the path
	m_prefs.setGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE, std::move(path));
	return true;
}


//-------------------------------------------------
//  refreshMameInfoDatabase
//-------------------------------------------------

bool MainWindow::refreshMameInfoDatabase()
{
	// sanity check; bail if we can't find the executable
	if (!IsMameExecutablePresent())
		return false;

	// list XML
	QString dbPath = m_prefs.getMameXmlDatabasePath();
	Task::ptr task = std::make_shared<ListXmlTask>(std::move(dbPath));
	m_taskDispatcher.launch(task);

	// callback to request interruptions when an emulation is running
	auto callback = [this, &task]()
	{
		if (m_state)
			task->requestInterruption();
	};

	// and show the dialog
	{
		LoadingDialog dlg(*this, callback);
		m_currentLoadingDialog.track(dlg);

		dlg.exec();
		if (dlg.result() != QDialog::DialogCode::Accepted)
		{
			task->requestInterruption();
			return false;
		}
	}

	// we've succeeded; load the DB
	return loadInfoDb();
}


//-------------------------------------------------
//  getRunningMachine
//-------------------------------------------------

info::machine MainWindow::getRunningMachine() const
{
	// this call is only valid if we have a running machine
	assert(m_currentRunMachineTask);

	// return the machine
	return m_currentRunMachineTask->getMachine();
}


//-------------------------------------------------
//  attachToMainWindow
//-------------------------------------------------

bool MainWindow::attachToMainWindow() const
{
	return !MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW
		|| !isMameVersionAtLeast(*MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW);
}


//-------------------------------------------------
//  attachWidgetId
//-------------------------------------------------

QString MainWindow::attachWidgetId() const
{
	QString result;

	if (MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW
		&& isMameVersionAtLeast(*MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW))
	{
		QWidget *attachWidget = attachToMainWindow()
			? (QWidget *)this
			: m_ui->emulationPanel;

		// the documentation for QWidget::WId() says that this value can change any
		// time; this is probably not true on Windows (where this returns the HWND)
		result = QString::number(attachWidget->winId());
	}
	return result;
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void MainWindow::run(const info::machine &machine, std::unique_ptr<SessionBehavior> &&sessionBehavior)
{
	// sanity check - this should never happen
	assert(m_mameVersion);

	// set the session behavior
	m_sessionBehavior = std::move(sessionBehavior);

	// run a "preflight check" on MAME, to catch obvious problems that might not be caught or reported well
	QString preflight_errors = preflightCheck();
	if (!preflight_errors.isEmpty())
	{
		messageBox(preflight_errors);
		return;
	}

	// identify the software name; we either used what was passed in, or we use what is in a profile
	// for which no images are mounted (suggesting a fresh launch)
	QString software_name = m_sessionBehavior->getInitialSoftware();

	// we need to have full information to support the emulation session; retrieve
	// fake a pauser to forestall "PAUSED" from appearing in the menu bar
	Pauser fake_pauser(*this, false);

	// run the emulation
	RunMachineTask::ptr task = std::make_shared<RunMachineTask>(
		machine,
		std::move(software_name),
		m_sessionBehavior->getOptions(),
		attachWidgetId());
	m_taskDispatcher.launch(task);
	m_currentRunMachineTask = std::move(task);

	// set up running state and subscribe to events
	m_state.emplace();

	// load the software list collection
	m_runningSoftwareListCollection.emplace();
	m_runningSoftwareListCollection->load(m_prefs, machine);

	// execute the start handler for all aspects
	for (const auto &aspect : m_aspects)
		aspect->start();

	// set the focus to the main window
	setFocus();

	// wait for first ping
	waitForStatusUpdate();

	// if we exited, bail
	if (!m_sessionBehavior)
		return;

	// load images associated with the behavior
	std::map<QString, QString> behaviorImages = m_sessionBehavior->getImages();
	if (behaviorImages.size() > 0)
	{
		std::vector<QString> args;
		args.push_back("load");
		for (auto &pair : behaviorImages)
		{
			args.push_back(std::move(pair.first));
			args.push_back(QDir::toNativeSeparators(pair.second));
		}
		issue(args);
	}

	// load a saved state associated with the behavior
	QString behaviorSavedStateFileName = m_sessionBehavior->getSavedState();
	if (!behaviorSavedStateFileName.isEmpty() && QFileInfo(behaviorSavedStateFileName).exists())
	{
		// no need to wait for a status update here
		issue({ "state_load", QDir::toNativeSeparators(behaviorSavedStateFileName) });
	}

	// do we have any images that require images?
	auto iter = std::ranges::find_if(m_state->images().get(), [&behaviorImages](const status::image &image)
	{
		return image.m_details
			&& image.m_details->m_must_be_loaded							// is this an image that must be loaded?
			&& image.m_file_name.isEmpty()									// and the filename is empty?
			&& behaviorImages.find(image.m_tag) == behaviorImages.end();	// and it wasn't specified as a "behavior image" (in which case we need to wait)?
	});
	if (iter != m_state->images().get().cend())
	{
		// if so, show the dialog
		ConfigurableDevicesDialogHost images_host(*this);
		ConfigurableDevicesDialog dialog(*this, images_host, true);
		dialog.exec();
		if (dialog.result() != QDialog::DialogCode::Accepted)
		{
			issue({ "exit" });
			return;
		}
	}

	// unpause
	changePaused(false);

	// set the focus
	if (!attachToMainWindow())
		m_ui->emulationPanel->setFocus(Qt::OtherFocusReason);
}


//-------------------------------------------------
//  getPreferences
//-------------------------------------------------

Preferences &MainWindow::getPreferences()
{
	return m_prefs;
}


//-------------------------------------------------
//  getRunningSoftwareListCollection
//-------------------------------------------------

const software_list_collection &MainWindow::getRunningSoftwareListCollection() const
{
	return *m_runningSoftwareListCollection;
}


//-------------------------------------------------
//  getRunningState
//-------------------------------------------------

status::state &MainWindow::getRunningState()
{
	return *m_state;
}


//-------------------------------------------------
//  createImage
//-------------------------------------------------

void MainWindow::createImage(const QString &tag, QString &&path)
{
	watchForImageMount(tag);
	issue({ "create", tag, QDir::toNativeSeparators(path) });
}


//-------------------------------------------------
//  loadImage
//-------------------------------------------------

void MainWindow::loadImage(const QString &tag, QString &&path)
{
	watchForImageMount(tag);
	issue({ "load", tag, QDir::toNativeSeparators(path) });
}


//-------------------------------------------------
//  unloadImage
//-------------------------------------------------

void MainWindow::unloadImage(const QString &tag)
{
	issue({ "unload", tag });
}


//-------------------------------------------------
//  preflightCheck - run checks on MAME to catch
//	obvious problems when they are easier to
//	diagnose (MAME's error reporting is hard for
//	BletchMAME to decipher)
//-------------------------------------------------

QString MainWindow::preflightCheck() const
{
	// get a list of the plugin paths, checking for the obvious problem where there are no paths
	QStringList paths = m_prefs.getSplitPaths(Preferences::global_path_type::PLUGINS);
	if (paths.empty())
		return QString("No plug-in paths are specified.  Under these circumstances, the required \"%1\" plug-in cannot be loaded.").arg(WORKER_UI_PLUGIN_NAME);

	// apply substitutions and normalize the paths
	for (QString &path : paths)
	{
		// if there is no trailing '/', append one
		if (!path.endsWith('/'))
			path += '/';
	}

	// local function to check for plug in files
	auto checkForPluginFiles = [&paths](const std::initializer_list<QString> &files)
	{
		bool success = util::find_if_ptr(paths, [&files](const QString &path)
		{
			for (const QString &file : files)
			{		
				QFileInfo fi(path + file);
				if (fi.exists() && fi.isFile())
					return true;
			}
			return false;
		});
		return success;
	};

	// local function to get all paths as a string (for error reporting)
	auto getAllPaths = [&paths]()
	{
		QString result;
		for (const QString &path : paths)
		{
			result += QDir::toNativeSeparators(path);
			result += "\n";
		}
		return result;
	};

	// check to see if worker_ui exists
	if (!checkForPluginFiles({ QString(WORKER_UI_PLUGIN_NAME "/init.lua"), QString(WORKER_UI_PLUGIN_NAME "/plugin.json") }))
	{
		auto message = QString("Could not find the %1 plug-in in the following directories:\n\n%2");
		return message.arg(WORKER_UI_PLUGIN_NAME, getAllPaths());
	}

	// check to see if boot.lua exists
	if (!checkForPluginFiles({ QString("boot.lua") }))
	{
		auto message = QString("Could not find boot.lua in the following directories:\n\n%1");
		return message.arg(getAllPaths());
	}

	// success!
	return QString();
}


//-------------------------------------------------
//  showStopEmulationWarning
//-------------------------------------------------

bool MainWindow::showStopEmulationWarning(StopWarningDialog::WarningType warningType)
{
	bool result;
	if (m_sessionBehavior->shouldPromptOnStop() && m_prefs.getShowStopEmulationWarning())
	{
		// prepare the dialog
		StopWarningDialog dialog(this, warningType);
		dialog.setShowThisChecked(m_prefs.getShowStopEmulationWarning());

		// show it
		result = dialog.exec() == QDialog::Accepted;

		// and update the preference
		m_prefs.setShowStopEmulationWarning(dialog.showThisChecked());
	}
	else
	{
		// the dialog was bypassed
		result = true;
	}
	return result;
}


//-------------------------------------------------
//  messageBox
//-------------------------------------------------

QMessageBox::StandardButton MainWindow::messageBox(const QString &message, QMessageBox::StandardButtons buttons)
{
	Pauser pauser(*this);

	QMessageBox msgBox(this);
	msgBox.setText(message);
	msgBox.setWindowTitle("BletchMAME");
	msgBox.setStandardButtons(buttons);
	return (QMessageBox::StandardButton) msgBox.exec();
}


//-------------------------------------------------
//  closeEvent
//-------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (m_state)
	{
		// prompt the user, if appropriate
		if (!showStopEmulationWarning(StopWarningDialog::WarningType::Exit))
		{
			event->ignore();
			return;
		}

		// issue exit command so we can shut down the emulation session gracefully
		invokeExit();
		while (m_state)
		{
			QCoreApplication::processEvents();
			QThread::yieldCurrentThread();
		}
	}

	// yup, we're closing
	event->accept();
}


//-------------------------------------------------
//  resizeEvent
//-------------------------------------------------

void MainWindow::resizeEvent(QResizeEvent *event)
{
	m_prefs.setSize(size());
	QWidget::resizeEvent(event);
}


//-------------------------------------------------
//  keyPressEvent
//-------------------------------------------------

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	// in an ode to MAME's normal UI, ScrollLock toggles the window bars (menu bar and status bar)
	if (event->key() == Qt::Key::Key_ScrollLock)
		m_prefs.setWindowBarsShown(!m_prefs.getWindowBarsShown());

	// pressing ALT to bring up menus is not friendly when running the emulation
	if (m_state && (event->modifiers() & Qt::AltModifier))
		event->ignore();
}


//-------------------------------------------------
//  changeEvent
//-------------------------------------------------

void MainWindow::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::WindowStateChange)
	{
		onWindowStateChange(*static_cast<QWindowStateChangeEvent *>(event));
	}
	QWidget::changeEvent(event);
}


//-------------------------------------------------
//  onWindowStateChange
//-------------------------------------------------

void MainWindow::onWindowStateChange(QWindowStateChangeEvent &event)
{
	auto qtWindowState = windowState();

	Preferences::WindowState prefsWindowState;
	if (qtWindowState & Qt::WindowFullScreen)
		prefsWindowState = Preferences::WindowState::FullScreen;
	else if (qtWindowState & Qt::WindowMaximized)
		prefsWindowState = Preferences::WindowState::Maximized;
	else
		prefsWindowState = Preferences::WindowState::Normal;
	m_prefs.setWindowState(prefsWindowState);
}


//-------------------------------------------------
//  showInputsDialog
//-------------------------------------------------

void MainWindow::showInputsDialog(status::input::input_class input_class)
{
	Pauser pauser(*this);
	InputsHost host(*this);
	InputsDialog dialog(this, host, input_class);
	dialog.exec();
}


//-------------------------------------------------
//  showSwitchesDialog
//-------------------------------------------------

void MainWindow::showSwitchesDialog(status::input::input_class input_class)
{
	Pauser pauser(*this);
	SwitchesHost host(*this);
	SwitchesDialog dialog(this, host, input_class, getRunningMachine());
	dialog.exec();
}


//-------------------------------------------------
//  isMameVersionAtLeast
//-------------------------------------------------

bool MainWindow::isMameVersionAtLeast(const SimpleMameVersion &version) const
{
	return m_mameVersion && m_mameVersion->isAtLeast(version);
}


//-------------------------------------------------
//  onFinalizeTask
//-------------------------------------------------m

bool MainWindow::onFinalizeTask(const FinalizeTaskEvent &event)
{
	m_taskDispatcher.finalize(event.task());
	return true;
}


//-------------------------------------------------
//  onVersionCompleted
//-------------------------------------------------m

bool MainWindow::onVersionCompleted(VersionResultEvent &event)
{
	// get the MAME version out of the event
	if (!event.m_version.isEmpty())
		m_mameVersion = MameVersion(event.m_version);
	else
		m_mameVersion.reset();

	// if the current MAME version is problematic, generate a warning
	QString message;
	if (m_mameVersion && !isMameVersionAtLeast(MameVersion::Capabilities::MINIMUM_SUPPORTED))
	{
		message = QString("This version of MAME doesn't seem to be supported; BletchMAME requires %1 or newer to function correctly").arg(
			MameVersion::Capabilities::MINIMUM_SUPPORTED.toPrettyString());
	}
	else if (!MameVersion::Capabilities::HAS_ATTACH_WINDOW)
	{
		message = "MAME on this platform does not support -attach_window, and the MAME window will not properly be embedded within BletchMAME";
	}
	else if (m_mameVersion && !isMameVersionAtLeast(*MameVersion::Capabilities::HAS_ATTACH_WINDOW))
	{
		message = QString("%1 is needed on this platform for -attach_window, and consequently the MAME window will not properly be embedded within BletchMAME").arg(
			MameVersion::Capabilities::HAS_ATTACH_WINDOW->toPrettyString());
	}

	// ...and display the warning if appropriate
	if (!message.isEmpty())
		messageBox(message);

	// the version check completed and we've dispensed with warnings; does this new version inform anything?
	if (!m_mameVersion)
	{
		// no MAME found - reset the database
		m_info_db.reset();

		// and prompt if that is in the plan
		if (m_promptIfMameNotFound)
			promptForMameExecutable();
	}
	else if (m_mameVersion->toString() != m_info_db.version())		
	{
		// we have MAME but need to load the database
		if (!loadInfoDb())
		{
			// time to refresh the database
			refreshMameInfoDatabase();
		}
	}

	// we're done!
	return true;
}


//-------------------------------------------------
//  onListXmlProgress
//-------------------------------------------------

bool MainWindow::onListXmlProgress(const ListXmlProgressEvent &event)
{
	if (m_currentLoadingDialog)
		m_currentLoadingDialog->progress(event.machineName(), event.machineDescription());
	return true;
}


//-------------------------------------------------
//  onListXmlCompleted
//-------------------------------------------------

bool MainWindow::onListXmlCompleted(const ListXmlResultEvent &event)
{
	int dialogResult;

	// check the status
	switch (event.status())
	{
	case ListXmlResultEvent::Status::SUCCESS:
		// if it succeeded, try to load the DB
		loadInfoDb();
		dialogResult = QDialog::Accepted;
		break;

	case ListXmlResultEvent::Status::ABORTED:
		// if we aborted, do nothing
		dialogResult = QDialog::Rejected;
		break;

	case ListXmlResultEvent::Status::ERROR:
		// present an error message
		messageBox(!event.errorMessage().isEmpty()
			? event.errorMessage()
			: "Error building MAME info database");
		dialogResult = QDialog::Rejected;
		break;

	default:
		throw false;
	}

	// end the dialog
	if (m_currentLoadingDialog)
		m_currentLoadingDialog->done(dialogResult);

	return true;
}


//-------------------------------------------------
//  onRunMachineCompleted
//-------------------------------------------------

bool MainWindow::onRunMachineCompleted(const RunMachineCompletedEvent &event)
{
	// this updates the profile, if present
	m_sessionBehavior->persistState(m_state->devslots().get(), m_state->images().get());

	// clear out all of the state
	m_currentRunMachineTask.reset();
	m_state.reset();
	m_runningSoftwareListCollection.reset();
	m_sessionBehavior.reset();

	// execute the stop handler for all aspects
	for (const auto &aspect : m_aspects)
		aspect->stop();

	// update the window - the emulation may have trashed it
	update();

	// report any errors
	if (!event.errorMessage().isEmpty())
	{
		messageBox(event.errorMessage());
	}
	return true;
}


//-------------------------------------------------
//  onStatusUpdate
//-------------------------------------------------

bool MainWindow::onStatusUpdate(StatusUpdateEvent &event)
{
	m_state->update(event.detachStatus());
	m_pinging = false;
	return true;
}


//-------------------------------------------------
//  watchForImageMount
//-------------------------------------------------

void MainWindow::watchForImageMount(const QString &tag)
{
	// find the device type
	info::machine machine = getRunningMachine();
	auto deviceIter = std::ranges::find_if(
		machine.devices(),
		[&tag](const info::device device)
		{
			return device.tag() == tag;
		});
	if (deviceIter == machine.devices().end())
		return;
	const QString &deviceType = deviceIter->type();

	// find the current value; we want to monitor for this value changing
	QString currentValue;
	const status::image *image = m_state->find_image(tag);
	if (image)
		currentValue = image->m_file_name;

	// start watching
	const QString &machineName = machine.name();
	m_watch_subscription.unsubscribe();
	m_watch_subscription = m_state->images().subscribe([this, currentValue{std::move(currentValue)}, tag, &machineName, &deviceType]
	{
		// did the value change?
		const status::image *image = m_state->find_image(tag);
		if (image && image->m_file_name != currentValue)
		{
			// it did!  place the new file in recent files
			QString newImageFileName = QDir::fromNativeSeparators(image->m_file_name);
			m_prefs.placeInRecentDeviceFiles(machineName, deviceType, std::move(newImageFileName));

			// and stop subscribing
			m_watch_subscription.unsubscribe();
			m_watch_subscription = observable::unique_subscription();
		}
	});	
}


//-------------------------------------------------
//  setChatterListener
//-------------------------------------------------

void MainWindow::setChatterListener(std::function<void(const ChatterEvent &chatter)> &&func)
{
	m_on_chatter = std::move(func);
}


//-------------------------------------------------
//  onChatter
//-------------------------------------------------

bool MainWindow::onChatter(const ChatterEvent &event)
{
	if (m_on_chatter)
		m_on_chatter(event);
	return true;
}


//-------------------------------------------------
//  execFileDialogWithCommand
//-------------------------------------------------

QString MainWindow::execFileDialogWithCommand(QFileDialog &dialog, std::vector<QString> &&commands)
{
	QString result;

	// exec the dialog
	dialog.exec();
	if (dialog.result() == QDialog::DialogCode::Accepted)
	{
		// get the result
		result = dialog.selectedFiles().first();

		// issue the commands
		commands.push_back(QDir::toNativeSeparators(result));
		issue(commands);
	}
	return result;
}


//-------------------------------------------------
//  getTitleBarText
//-------------------------------------------------

QString MainWindow::getTitleBarText()
{
	// assemble the "root" title
	QString applicationName = QCoreApplication::applicationName();
	QString applicationVersion = QCoreApplication::applicationVersion();
	QString nameWithVersion = !applicationVersion.isEmpty()
		? QString("%1 %2").arg(applicationName, applicationVersion)
		: applicationName;

	// are we running?
	QString result;
	if (m_state)
	{
		// get the machine description
		const QString &machineDesc = m_currentRunMachineTask->getMachine().description();

		// assemble a running description (e.g. - 'Pac-Man (MAME 0.234)')
		QString runningDesc = m_mameVersion
			? QString("%1 (%2)").arg(machineDesc, m_mameVersion->toPrettyString())
			: machineDesc;

		// we want to append "PAUSED" if and only if the user paused, not as a consequence of a menu
		QString titleTextFormat = m_state->paused().get() && !m_current_pauser
			? "%1: %2 PAUSED"
			: "%1: %2";

		// and apply the format
		result = titleTextFormat.arg(nameWithVersion, runningDesc);
	}
	else
	{
		// not running - just use the name/version
		result = nameWithVersion;
	}
	return result;
}


//-------------------------------------------------
//  onCheckForFocusSkew
//-------------------------------------------------

bool MainWindow::onCheckForFocusSkew()
{
	if (LOG_FOCUSSKEW)
	{
		qDebug() << QString("MainWindow::onCheckForFocusSkew(): winGetFocus=%1 qApp->focusWidget()=%2").arg(
			widgetDesc(winGetFocus().value_or(0)),
			widgetDesc(qApp->focusWidget()));
	}

	// this mechanism is only needed when there is a live and unpaused emulation session
	if (m_currentRunMachineTask
		&& winId() == winGetFocus()
		&& (!qApp->focusWidget() || (qApp->focusWidget() == m_ui->emulationPanel)))
	{
		if (LOG_FOCUSSKEW)
			qDebug() << "MainWindow::onCheckForFocusSkew(): Calling winSetFocus()";

		// Qt thinks that the emulationPanel but Windows thinks the main window has focus; we
		// need to tell Windows to change the focus
		winSetFocus(m_ui->emulationPanel->winId());
	}
	return true;
}


//-------------------------------------------------
//  winGetFocus - wrapper for Win32 ::GetFocus()
//  function
//-------------------------------------------------

std::optional<WId> MainWindow::winGetFocus()
{
	// this compiles out when not on Windows
#ifdef Q_OS_WINDOWS
	return (WId) ::GetFocus();
#else // !Q_OS_WINDOWS
	return { };
#endif // Q_OS_WINDOWS
}


//-------------------------------------------------
//  winSetFocus - wrapper for Win32 ::SetFocus()
//  function
//-------------------------------------------------

void MainWindow::winSetFocus(WId wid)
{
	// this compiles out when not on Windows
#ifdef Q_OS_WINDOWS
	::SetFocus((HWND)wid);
#endif // Q_OS_WINDOWS
}


//-------------------------------------------------
//  widgetDesc - diagnostic function to label a WId
//-------------------------------------------------

QString MainWindow::widgetDesc(const QWidget *widget) const
{
	return widgetDesc(widget ? widget->winId() : (WId)0);
}


//-------------------------------------------------
//  widgetDesc - diagnostic function to label a WId
//-------------------------------------------------

QString MainWindow::widgetDesc(WId wid) const
{
	// try to identify the WId
	QString desc;
	if (wid == winId())
		desc = "MainWindow";
	else if (wid == m_ui->emulationPanel->winId())
		desc = "emulationPanel";

	// and format accordingly
	const char *format = !desc.isEmpty()
		? "[0x%1 (%2)]"
		: "[0x%1]";
	return QString(format).arg(QString::number(wid, 16), desc);
}


//-------------------------------------------------
//  browseForMameIni
//-------------------------------------------------

QString MainWindow::browseForMameIni()
{
	// get the default directory for finding the MAME.ini
	QString defaultDirPath;
	QString fileToSelect;
	const QString &emuExecutablePath = m_prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
	if (!emuExecutablePath.isEmpty())
	{
		// we have an executable path; lets default to that directory
		QDir defaultDir = QFileInfo(emuExecutablePath).absoluteDir();
		defaultDirPath = defaultDir.absolutePath();

		// is there already a mame.ini there?
		QString candidateFileToSelect = defaultDir.absoluteFilePath("mame.ini");
		if (QFileInfo(candidateFileToSelect).isFile())
			fileToSelect = std::move(candidateFileToSelect);
	}

	// try to open a file
	QFileDialog fileDialog(this, "Find MAME.ini", defaultDirPath, "MAME INI files (*.ini)");
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	if (!fileToSelect.isEmpty())
		fileDialog.selectFile(fileToSelect);

	// run the dialog
	QString result;
	if (fileDialog.exec())
	{
		QStringList selectedFiles = fileDialog.selectedFiles();
		if (selectedFiles.size() > 0)
			result = std::move(selectedFiles[0]);
	}
	return result;
}


//-------------------------------------------------
//  importMameIni
//-------------------------------------------------

bool MainWindow::importMameIni(const QString &fileName, bool prompt)
{
	// prepare the import dialog
	ImportMameIniDialog dialog(m_prefs, this);
	if (!dialog.loadMameIni(fileName))
		return false;

	// is this an "opportunistic" MAME.ini load?  if so we should prompt the user to see if
	// this is desirable first
	if (prompt)
	{
		// if there is nothing to import, showing the dialog is pointless
		if (!dialog.hasImportables())
			return false;

		// show the prompt
		QString message = QString("%1 has found a MAME.ini file containing settings that can be imported.  Do you wish to import some or all of these settings?")
			.arg(QCoreApplication::applicationName());
		if (messageBox(message, QMessageBox::Yes | QMessageBox::No) != QMessageBox::StandardButton::Yes)
			return false;

	}

	// show the dialog
	if (dialog.exec() != QDialog::Accepted)
		return false;

	// apply!
	dialog.apply();
	return true;
}


//**************************************************************************
//  RUNTIME CONTROL
//
//	Actions that affect MAME at runtime go here.  The naming convention is
//	that "invocation actions" take the form InvokeXyz(), whereas methods
//	that change something take the form ChangeXyz()
//**************************************************************************

//-------------------------------------------------
//  issue
//-------------------------------------------------

void MainWindow::issue(const std::vector<QString> &args)
{
	if (m_currentRunMachineTask)
		m_currentRunMachineTask->issue(args);
}


void MainWindow::issue(const std::initializer_list<std::string> &args)
{
	std::vector<QString> qargs;
	qargs.reserve(args.size());

	for (const auto &arg : args)
	{
		QString qarg = QString::fromStdString(arg);
		qargs.push_back(std::move(qarg));
	}

	issue(qargs);
}


//-------------------------------------------------
//  waitForStatusUpdate
//-------------------------------------------------

void MainWindow::waitForStatusUpdate()
{
	m_pinging = true;
	while (m_pinging)
	{
		if (!m_state)
			return;
		QCoreApplication::processEvents();
		QThread::yieldCurrentThread();
	}
}


//-------------------------------------------------
//  invokePing
//-------------------------------------------------

void MainWindow::invokePing()
{
	// only issue a ping if there is an active session, and there is no ping in flight
	if (!m_pinging && m_state)
	{
		m_pinging = true;
		issue({ "ping" });
	}
}


//-------------------------------------------------
//  invokeExit
//-------------------------------------------------

void MainWindow::invokeExit()
{
	QString savedStatePath = m_sessionBehavior->getSavedState();
	if (!savedStatePath.isEmpty())
	{
		issue({ "state_save_and_exit", QDir::toNativeSeparators(savedStatePath) });
	}
	else
	{
		issue({ "exit" });
	}
}


//-------------------------------------------------
//  changePaused
//-------------------------------------------------

void MainWindow::changePaused(bool paused)
{
	issue({ paused ? "pause" : "resume" });
}


//-------------------------------------------------
//  changeThrottled
//-------------------------------------------------

void MainWindow::changeThrottled(bool throttled)
{
	issue({ "throttled", std::to_string(throttled ? 1 : 0) });
}


//-------------------------------------------------
//  changeThrottleRate
//-------------------------------------------------

void MainWindow::changeThrottleRate(float throttle_rate)
{
	issue({ "throttle_rate", std::to_string(throttle_rate) });
}


//-------------------------------------------------
//  changeThrottleRate
//-------------------------------------------------

void MainWindow::changeThrottleRate(int adjustment)
{
	// find where we are in the array
	int index;
	for (index = 0; index < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); index++)
	{
		if (m_state->throttle_rate().get() >= s_throttle_rates[index])
			break;
	}

	// apply the adjustment
	index += adjustment;
	index = std::max(index, 0);
	index = std::min(index, (int)(sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]) - 1));

	// and change the throttle rate
	changeThrottleRate(s_throttle_rates[index]);
}


//-------------------------------------------------
//  changeSound
//-------------------------------------------------

void MainWindow::changeSound(bool sound_enabled)
{
	issue({ "set_attenuation", std::to_string(sound_enabled ? SOUND_ATTENUATION_ON : SOUND_ATTENUATION_OFF) });
}


//-------------------------------------------------
//  updateWindowBarsShown
//-------------------------------------------------

void MainWindow::updateWindowBarsShown()
{
	bool shown = m_prefs.getWindowBarsShown();

	// update the window bars' visibility
	m_ui->menubar->setVisible(shown);
	m_ui->statusBar->setVisible(shown);

	// mouse capture is another thing
	m_captureMouseIfAppropriate = !shown;
}


//-------------------------------------------------
//  updateStatusBar
//-------------------------------------------------

void MainWindow::updateStatusBar()
{
	// the status message is different depending on whether we're running
	QString statusMessage = m_state
		? runningStateText(*m_state)
		: m_mainPanel->statusMessage();

	// and show it
	m_ui->statusBar->showMessage(statusMessage);
}


//-------------------------------------------------
//  runningStateText
//-------------------------------------------------

QString MainWindow::runningStateText(const status::state &state)
{
	// prepare a vector with the status text
	QStringList statusText;

	// first entry depends on whether we are running
	if (state.phase().get() == status::machine_phase::RUNNING)
	{
		QString speedText;
		int speedPercent = (int)(state.speed_percent().get() * 100.0 + 0.5);
		if (state.effective_frameskip().get() == 0)
		{
			speedText = QString("%2%1").arg(
				"%",
				QString::number(speedPercent));
		}
		else
		{
			speedText = QString("%2%1 (frameskip %3/10)").arg(
				"%",
				QString::number(speedPercent),
				QString::number((int)state.effective_frameskip().get()));
		}
		statusText.push_back(std::move(speedText));
	}
	else
	{
		statusText.push_back(state.startup_text().get());
	}

	// and return it specify it
	return statusText.join(' ');
}


//-------------------------------------------------
//  auditIfAppropriate
//-------------------------------------------------

void MainWindow::auditIfAppropriate(const info::machine &machine)
{
	// if we can automatically audit, and this status is unknown...
	if (canAutomaticallyAudit()
		&& m_prefs.getMachineAuditStatus(machine.name()) == AuditStatus::Unknown)
	{
		// then add it to the queue
		MachineIdentifier identifier(machine.name());
		m_auditQueue.push(std::move(identifier), true);
		updateAuditTimer();
	}
}


//-------------------------------------------------
//  auditIfAppropriate
//-------------------------------------------------

void MainWindow::auditIfAppropriate(const software_list::software &software)
{
	// if we can automatically audit, and this status is unknown...
	if (canAutomaticallyAudit()
		&& m_prefs.getSoftwareAuditStatus(software.parent().name(), software.name()) == AuditStatus::Unknown)
	{
		// then add it to the queue
		SoftwareIdentifier identifier(software.parent().name(), software.name());
		m_auditQueue.push(std::move(identifier), true);
		updateAuditTimer();
	}
}


//-------------------------------------------------
//  canAutomaticallyAudit
//-------------------------------------------------

bool MainWindow::canAutomaticallyAudit() const
{
	// we have to be configured to automatically audit, and not be running an emulation
	return (m_prefs.getAuditingState() == Preferences::AuditingState::Automatic)
		&& !m_currentRunMachineTask;
}


//-------------------------------------------------
//  updateAuditTimer
//-------------------------------------------------

void MainWindow::updateAuditTimer()
{
	// the timer has the role of dispatching audit tasks and also keeping the queue populated with
	// low priority tasks, therefore this logic is somewhat knotty
	//
	// perhaps this could be driven by Qt signals?

	bool auditTimerActive;
	if (m_prefs.getAuditingState() != Preferences::AuditingState::Automatic || m_currentRunMachineTask)
	{
		// either we're running an emulation or auditing is off
		auditTimerActive = false;
	}
	else if (m_auditQueue.hasUndispatched())
	{
		// yes there are undispatched tasks - we need the timer
		auditTimerActive = true;
	}
	else
	{
		// no pending audits - we will still need the timer for low priority audits
		// from the audit cursor
		auditTimerActive = !m_auditCursor.isComplete();
	}

	if (auditTimerActive)
		m_auditTimer->start();
	else
		m_auditTimer->stop();
}


//-------------------------------------------------
//  auditDialogStarted
//-------------------------------------------------

void MainWindow::auditDialogStarted(AuditDialog &auditDialog, std::shared_ptr<AuditTask> &&auditTask)
{
	// track the dialog
	m_currentAuditDialog.track(auditDialog);

	// and dispatch the task
	m_taskDispatcher.launch(std::move(auditTask));
}


//-------------------------------------------------
//  getAuditSoftwareListCollection
//-------------------------------------------------

software_list_collection &MainWindow::getAuditSoftwareListCollection()
{
	return m_auditSoftwareListCollection;
}


//-------------------------------------------------
//  auditTimerProc
//-------------------------------------------------

void MainWindow::auditTimerProc()
{
	addLowPriorityAudits();
	dispatchAuditTasks();
}


//-------------------------------------------------
//  dispatchAuditTasks
//-------------------------------------------------

void MainWindow::dispatchAuditTasks()
{
	// find out how many active tasks are present
	std::size_t activeTaskCount = m_taskDispatcher.countActiveTasksByType<AuditTask>();

	// spin up tasks
	AuditTask::ptr auditTask;
	while (activeTaskCount < m_maximumConcurrentAuditTasks && (auditTask = m_auditQueue.tryCreateAuditTask()) != nullptr)
	{
		m_taskDispatcher.launch(std::move(auditTask));
		activeTaskCount++;
	}

	updateAuditTimer();
}


//-------------------------------------------------
//  reportAuditResults
//-------------------------------------------------

void MainWindow::reportAuditResults(const std::vector<AuditResult> &results)
{
	// we only want to report one of them (its silly to pop up multiple messages), but
	// hypothetically reportAuditResult() can fail
	for (const AuditResult &result : results)
	{
		if (reportAuditResult(result))
		{
			// we've reported something - break out
			break;
		}
	}
}


//-------------------------------------------------
//  reportAuditResult
//-------------------------------------------------

bool MainWindow::reportAuditResult(const AuditResult &result)
{
	using namespace std::chrono_literals;

	// is this audit result present?  while its ok in principle to audit invisible items, we
	// really don't want to do so because the user is trying to keep such items away
	const AuditableListItemModel *model = m_mainPanel->currentAuditableListItemModel();
	if (!model || !model->isAuditIdentifierPresent(result.identifier()))
		return false;

	// audit identifier string
	const QString *identifierString = auditIdentifierString(result.identifier());
	if (!identifierString)
		return false;

	// build the message
	QString message = QString("Audited \"%1\": %2").arg(
		*identifierString,
		auditStatusString(result.status()));

	// and show it
	auto messageDuration = 5s;
	m_ui->statusBar->showMessage(
		message,
		std::chrono::duration_cast<std::chrono::milliseconds>(messageDuration).count());
	return true;
}


//-------------------------------------------------
//  auditIdentifierString
//-------------------------------------------------

const QString *MainWindow::auditIdentifierString(const Identifier &identifier) const
{
	const QString *result = nullptr;

	std::visit(util::overloaded
	{
		[this, &result] (const MachineIdentifier &identifier)
		{
			std::optional<info::machine> machine = m_info_db.find_machine(identifier.machineName());
			if (machine)
				result = &machine->description();
		},
		[this, &result](const SoftwareIdentifier &identifier)
		{
			QString softwareListQString = util::toQString(identifier.softwareList());
			QString softwareQString = util::toQString(identifier.software());
			const software_list::software *software = m_auditSoftwareListCollection.find_software_by_list_and_name(
				softwareListQString,
				softwareQString);
			if (software)
				result = &software->description();
		}
	}, identifier);

	return result;
}


//-------------------------------------------------
//  auditStatusString
//-------------------------------------------------

QString MainWindow::auditStatusString(AuditStatus status)
{
	QString result;
	switch (status)
	{
	case AuditStatus::Unknown:
		result = "Unknown";
		break;
	case AuditStatus::Found:
		result = "All Media Found";
		break;
	case AuditStatus::MissingOptional:
		result = "Optional Media Missing";
		break;
	case AuditStatus::Missing:
		result = "Media Missing";
		break;
	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  addLowPriorityAudits
//-------------------------------------------------

void MainWindow::addLowPriorityAudits()
{
	if (!m_auditCursor.isComplete())
	{
		// get the current position so that we don't wrap around
		int basePosition = m_auditCursor.currentPosition();

		// keep on getting identifiers
		std::optional<Identifier> identifier;
		while (m_auditQueue.isCloseToEmpty() && bool(identifier = m_auditCursor.next(basePosition)))
		{
			m_auditQueue.push(std::move(*identifier), false);
		}
	}
}


//-------------------------------------------------
//  onAuditResult
//-------------------------------------------------

bool MainWindow::onAuditResult(const AuditResultEvent &event)
{
	// if we have a positive cookie, only use it if it matches
	if (event.cookie() < 0 || event.cookie() == m_auditQueue.currentCookie())
	{
		// they do in fact match; update the statuses
		m_mainPanel->setAuditStatuses(event.results());

		// and report the results in the status bar
		reportAuditResults(event.results());

		// and measure throughput (if enabled)
#if USE_PROFILER
		m_auditThroughputTracker.mark(event.results().size());
#endif // USE_PROFILER
	}
	return true;
}


//-------------------------------------------------
//  onAuditProgress
//-------------------------------------------------

bool MainWindow::onAuditProgress(const AuditProgressEvent &event)
{
	if (m_currentAuditDialog)
		m_currentAuditDialog->auditProgress(event.entryIndex(), event.bytesProcessed(), event.totalBytes(), event.verdict());
	return true;
}
