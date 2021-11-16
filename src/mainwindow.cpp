/***************************************************************************

	mainwindow.cpp

	Main BletchMAME window

***************************************************************************/

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
#include <QTextStream>
#include <QWindowStateChangeEvent>

#include <chrono>

#include "mainwindow.h"
#include "mainpanel.h"
#include "mameversion.h"
#include "perfprofiler.h"
#include "ui_mainwindow.h"
#include "audittask.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "versiontask.h"
#include "utility.h"
#include "dialogs/about.h"
#include "dialogs/audit.h"
#include "dialogs/cheats.h"
#include "dialogs/confdev.h"
#include "dialogs/console.h"
#include "dialogs/inputs.h"
#include "dialogs/loading.h"
#include "dialogs/paths.h"
#include "dialogs/switches.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#ifdef min
#undef min
#endif // min

#ifdef max
#undef max
#endif // max


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
		m_is_running = actually_pause && m_host.m_state.has_value() && !m_host.m_state->paused().get();
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

	virtual info::machine getMachine()
	{
		return m_host.getRunningMachine();
	}

	virtual Preferences &getPreferences()
	{
		return m_host.m_prefs;
	}

	virtual observable::value<std::vector<status::image>> &getImages()
	{
		return m_host.m_state->images();
	}

	virtual observable::value<std::vector<status::slot>> &getSlots()
	{
		return m_host.m_state->devslots();
	}

	virtual void associateFileDialogWithMachinePrefs(QFileDialog &fileDialog)
	{
		m_host.associateFileDialogWithMachinePrefs(fileDialog, getMachineName(), Preferences::machine_path_type::WORKING_DIRECTORY, false);
	}

	virtual const std::vector<QString> &getRecentFiles(const QString &tag) const
	{
		info::machine machine = m_host.getRunningMachine();
		const QString &device_type = GetDeviceType(machine, tag);
		return m_host.m_prefs.getRecentDeviceFiles(machine.name(), device_type);
	}

	virtual std::vector<QString> getExtensions(const QString &tag) const
	{
		std::vector<QString> result;

		// first try getting the result from the image format
		const status::image *image = m_host.m_state->find_image(tag);
		if (image && image->m_formats.has_value())
		{
			// we did find it in the image formats
			for (const status::image_format &format : *image->m_formats)
			{
				for (const QString &ext : format.m_extensions)
				{
					if (std::find(result.begin(), result.end(), ext) == result.end())
						result.push_back(ext);
				}
			}
		}
		else
		{
			// find the device declaration
			auto devices = m_host.getRunningMachine().devices();

			auto iter = std::find_if(devices.begin(), devices.end(), [&tag](info::device dev)
			{
				return dev.tag() == tag;
			});
			assert(iter != devices.end());

			// and return it!
			result = util::string_split(iter->extensions(), [](auto ch) { return ch == ','; });
		}
		return result;
	}

	virtual void createImage(const QString &tag, QString &&path)
	{
		m_host.WatchForImageMount(tag);
		m_host.issue({ "create", tag, std::move(path) });
	}

	virtual void loadImage(const QString &tag, QString &&path)
	{
		m_host.WatchForImageMount(tag);
		m_host.issue({ "load", tag, std::move(path) });
	}

	virtual void unloadImage(const QString &tag)
	{
		m_host.issue({ "unload", tag });
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
			m_host.issue({ "set_cheat_state", id, enabled ? "1" : "0", QString::number(parameter.value()) });
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
		// prepare a vector with the status text
		QStringList statusText;

		// is there a running emulation?
		if (state().has_value())
		{
			// first entry depends on whether we are running
			if (state()->phase().get() == status::machine_phase::RUNNING)
			{
				QString speedText;
				int speedPercent = (int)(m_host.m_state->speed_percent().get() * 100.0 + 0.5);
				if (state()->effective_frameskip().get() == 0)
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
						QString::number((int)m_host.m_state->effective_frameskip().get()));
				}
				statusText.push_back(std::move(speedText));
			}
			else
			{
				statusText.push_back(state()->startup_text().get());
			}

			// next entries come from device displays
			for (auto iter = state()->images().get().cbegin(); iter < state()->images().get().cend(); iter++)
			{
				if (!iter->m_display.isEmpty())
					statusText.push_back(iter->m_display);
			}

			// do we have the mouse capture problem?
			if (state()->has_input_using_mouse().get() && state()->has_mouse_enabled_problem().get())
				statusText.push_back("This version of MAME does not support hot changes to mouse capture; the mouse may not be usable");
		}

		// and specify it
		QString statusTextString = statusText.join(' ');
		statusBar().showMessage(statusTextString);
	}


	std::optional<status::state> &state()
	{
		return m_host.m_state;
	}


	QStatusBar &statusBar()
	{
		return *m_host.m_ui->statusBar;
	}
};


// ======================> MenuBarAspect

class MainWindow::MenuBarAspect : public Aspect
{
public:
	MenuBarAspect(MainWindow &host)
		: m_host(host)
	{
		QMenuBar &menuBar = *m_host.m_ui->menubar;
		m_host.m_menu_bar_shown.subscribe([&menuBar](bool shown) { menuBar.setVisible(shown); });
		m_host.m_menu_bar_shown = true;
	}

	virtual void start()
	{
		// update the menu bar from the prefs
		m_host.m_menu_bar_shown = m_host.m_prefs.getMenuBarShown();

		// mouse capturing is a bit more involved
		m_mouseCaptured = observable::observe(m_host.m_state->has_input_using_mouse() && !m_host.m_menu_bar_shown);
		m_mouseCaptured.subscribe([this]()
		{
			m_host.issue({ "SET_MOUSE_ENABLED", m_mouseCaptured ? "true" : "false" });
			m_host.setCursor(m_mouseCaptured.get() ? Qt::BlankCursor : Qt::ArrowCursor);
		});
	}

	virtual void stop()
	{
		m_host.m_prefs.setMenuBarShown(m_host.m_menu_bar_shown.get());
		m_host.m_menu_bar_shown = true;
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


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_mainPanel(nullptr)
	, m_taskDispatcher(*this, m_prefs)
	, m_auditQueue(m_prefs, m_info_db, m_softwareListCollection)
	, m_auditTimer(nullptr)
	, m_maximumConcurrentAuditTasks(std::max(std::thread::hardware_concurrency(), (unsigned int)2))
	, m_auditCursor(m_prefs)
	, m_pinging(false)
	, m_current_pauser(nullptr)
{
	using namespace std::chrono_literals;

	// set up Qt form
	m_ui = std::make_unique<Ui::MainWindow>();
	m_ui->setupUi(this);

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

	// set up status labels
	for (auto i = 0; i < std::size(m_statusLabels); i++)
	{
		m_statusLabels[i] = new QLabel(this);
		m_statusLabels[i]->setFixedWidth(120);
		m_ui->statusBar->addPermanentWidget(m_statusLabels[i]);
	}

	// listen to status updates from MainPanel
	connect(m_mainPanel, &MainPanel::statusChanged, this, [this](const auto &newStatus)
	{
		m_ui->statusBar->showMessage(newStatus[0]);
		for (auto i = 0; i < std::min(std::size(m_statusLabels), std::size(newStatus) - 1); i++)
			m_statusLabels[i]->setText(newStatus[i + 1]);
	});

	// set up the ping timer
	QTimer &pingTimer = *new QTimer(this);
	connect(&pingTimer, &QTimer::timeout, this, &MainWindow::invokePing);
	setupActionAspect([&pingTimer]() { pingTimer.start(500); }, [&pingTimer]() { pingTimer.stop(); });

	// set up profiler timer
	if (PerformanceProfiler::instance().isReal())
	{
		QTimer &perfProfilerTimer = *new QTimer(this);
		connect(&perfProfilerTimer, &QTimer::timeout, this, []()
		{
			PerformanceProfiler::instance().dump();
		});
		perfProfilerTimer.start(1500);
	}

	// set the proper window state from preferences
	switch (m_prefs.getWindowState())
	{
	case Preferences::WindowState::Normal:
		if (m_prefs.getSize().has_value())
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

	// setup properties that pertain to runtime behavior
	setupPropSyncAspect(*m_ui->stackedLayout,					&QStackedLayout::currentWidget,	&QStackedLayout::setCurrentWidget,	{ },								[this]() { return attachToMainWindow() ? nullptr : m_ui->emulationPanel; });
	setupPropSyncAspect((QWidget &) *this,						&QWidget::windowTitle,			&QWidget::setWindowTitle,			&status::state::paused,				[this]() { return getTitleBarText(); });

	// actions
	setupPropSyncAspect(*m_ui->actionStop,						&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionPause,						&QAction::isEnabled,			&QAction::setEnabled,				{ },								true);
	setupPropSyncAspect(*m_ui->actionPause,						&QAction::isChecked,			&QAction::setChecked,				&status::state::paused,				[this]() { return m_state->paused().get(); });
	setupPropSyncAspect(*m_ui->actionImages,					&QAction::isEnabled,			&QAction::setEnabled,				&status::state::images,				[this]() { return m_state->images().get().size() > 0;});
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
	m_aspects.push_back(std::make_unique<MenuBarAspect>(*this));
	m_aspects.push_back(std::make_unique<ToggleMovieTextAspect>(m_current_recording_movie_filename, *m_ui->actionToggleRecordMovie));
	m_aspects.push_back(std::make_unique<QuickLoadSaveAspect>(m_currentQuickState, *m_ui->actionQuickLoadState, *m_ui->actionQuickSaveState));

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
		launchVersionCheck(false);
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
	if (m_sessionBehavior->shouldPromptOnStop())
	{
		QString message = "Do you really want to stop?\n"
			"\n"
			"All data in emulated RAM will be lost";

		if (messageBox(message, QMessageBox::Yes | QMessageBox::No) != QMessageBox::StandardButton::Yes)
			return;
	}

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
	QString path = fileDialogCommand(
		{ "state_load" },
		"Load State",
		Preferences::machine_path_type::LAST_SAVE_STATE,
		true,
		s_wc_saved_state,
		QFileDialog::AcceptMode::AcceptOpen);

	if (!path.isEmpty())
		m_currentQuickState = std::move(path);
}


//-------------------------------------------------
//  on_actionSaveState_triggered
//-------------------------------------------------

void MainWindow::on_actionSaveState_triggered()
{
	QString path = fileDialogCommand(
		{ "state_save" },
		"Save State",
		Preferences::machine_path_type::LAST_SAVE_STATE,
		true,
		s_wc_saved_state,
		QFileDialog::AcceptMode::AcceptSave);

	if (!path.isEmpty())
		m_currentQuickState = std::move(path);
}


//-------------------------------------------------
//  on_actionSaveScreenshot_triggered
//-------------------------------------------------

void MainWindow::on_actionSaveScreenshot_triggered()
{
	fileDialogCommand(
		{ "save_snapshot", "0" },
		"Save Snapshot",
		Preferences::machine_path_type::WORKING_DIRECTORY,
		false,
		s_wc_save_snapshot,
		QFileDialog::AcceptMode::AcceptSave);
}


//-------------------------------------------------
//  on_actionToggleRecordMovie_triggered
//-------------------------------------------------

void MainWindow::on_actionToggleRecordMovie_triggered()
{
	// Are we the required version? 
	if (!isMameVersionAtLeast(MameVersion::Capabilities::HAS_TOGGLE_MOVIE))
	{
		QString message = QString("Recording movies requires MAME %1 or newer to function")
			.arg(MameVersion::Capabilities::HAS_TOGGLE_MOVIE.toString());
		messageBox(message);
		return;
	}

	// We're toggling the movie status... so are we recording?
	if (m_state.value().is_recording())
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

		// If not, show a file dialog and start recording
		Pauser pauser(*this);
		QString path = getFileDialogFilename(
			"Record Movie",
			Preferences::machine_path_type::WORKING_DIRECTORY,
			s_wc_record_movie,
			QFileDialog::AcceptMode::AcceptSave,
			false);
		if (!path.isEmpty())
		{
			// determine the recording file type
			QFileInfo fi(path);
			QString extension = fi.suffix().toUpper();
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
	ensureProperFocus();

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
//  on_actionAbout_triggered
//-------------------------------------------------

void MainWindow::on_actionAbout_triggered()
{
	AboutDialog dlg(this, m_info_db.version());
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
	else if (event->type() == AuditSingleMediaEvent::eventId())
	{
		result = onAuditSingleMedia(static_cast<AuditSingleMediaEvent &>(*event));
	}
	else if (event->type() == ChatterEvent::eventId())
	{
		result = onChatter(static_cast<ChatterEvent &>(*event));
	}
	else if (event->type() == QEvent::WindowActivate)
	{
		ensureProperFocus();
	}

	// if we have a result, we've handled the event; otherwise we have to pass it on
	// to QMainWindow::event()
	return result.has_value()
		? result.value()
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
	QString dbPath = m_prefs.getMameXmlDatabasePath();
	return m_info_db.load(dbPath, m_mameVersion);
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

	// if this changed, act accordingly
	if (path != m_prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE))
	{
		// set the path
		m_prefs.setGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE, std::move(path));

		// and trigger a version check
		launchVersionCheck(false);
	}

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

	// and show the dialog
	{
		LoadingDialog dlg(*this);
		m_currentLoadingDialog.track(dlg);

		dlg.exec();
		if (dlg.result() != QDialog::DialogCode::Accepted)
		{
			task->abort();
			return false;
		}
	}

	// we've succeeded; load the DB
	if (!m_info_db.load(dbPath))
	{
		// a failure here is likely due to a very strange condition (e.g. - someone deleting the infodb
		// file out from under me)
		return false;
	}

	return true;
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
	return !MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW.has_value()
		|| !isMameVersionAtLeast(MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW.value());
}


//-------------------------------------------------
//  attachWidgetId
//-------------------------------------------------

QString MainWindow::attachWidgetId() const
{
	QString result;

	if (MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW.has_value()
		&& isMameVersionAtLeast(MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW.value()))
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
			args.push_back(std::move(pair.second));
		}
		issue(args);
	}

	// load a saved state associated with the behavior
	QString behaviorSavedStateFileName = m_sessionBehavior->getSavedState();
	if (!behaviorSavedStateFileName.isEmpty() && QFileInfo(behaviorSavedStateFileName).exists())
	{
		// no need to wait for a status update here
		issue({ "state_load", behaviorSavedStateFileName });
	}

	// do we have any images that require images?
	auto iter = std::find_if(m_state->images().get().cbegin(), m_state->images().get().cend(), [&behaviorImages](const status::image &image)
	{
		return image.m_must_be_loaded										// is this an image that must be loaded?
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
}


//-------------------------------------------------
//  getSoftwareListCollection
//-------------------------------------------------

software_list_collection &MainWindow::getSoftwareListCollection()
{
	return m_softwareListCollection;
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
	if (m_state.has_value())
	{
		// prompt the user, if appropriate
		if (m_sessionBehavior->shouldPromptOnStop())
		{
			QString message = "Do you really want to exit?\n"
				"\n"
				"All data in emulated RAM will be lost";
			if (messageBox(message, QMessageBox::Yes | QMessageBox::No) != QMessageBox::StandardButton::Yes)
			{
				event->ignore();
				return;
			}
		}

		// issue exit command so we can shut down the emulation session gracefully
		invokeExit();
		while (m_state.has_value())
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
	// in an ode to MAME's normal UI, ScrollLock toggles the menu bar
	if (event->key() == Qt::Key::Key_ScrollLock)
		m_menu_bar_shown = !m_menu_bar_shown.get();

	// pressing ALT to bring up menus is not friendly when running the emulation
	if (m_state.has_value() && (event->modifiers() & Qt::AltModifier))
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

bool MainWindow::isMameVersionAtLeast(const MameVersion &version) const
{
	return MameVersion(m_mameVersion).isAtLeast(version);
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
	m_mameVersion = std::move(event.m_version);

	// if the current MAME version is problematic, generate a warning
	QString message;
	if (!m_mameVersion.isEmpty() && !isMameVersionAtLeast(MameVersion::Capabilities::MINIMUM_SUPPORTED))
	{
		message = QString("This version of MAME doesn't seem to be supported; BletchMAME requires MAME %1.%2 or newer to function correctly").arg(
			QString::number(MameVersion::Capabilities::MINIMUM_SUPPORTED.major()),
			QString::number(MameVersion::Capabilities::MINIMUM_SUPPORTED.minor()));
	}
	else if (!MameVersion::Capabilities::HAS_ATTACH_WINDOW.has_value())
	{
		message = "MAME on this platform does not support -attach_window, and the MAME window will not properly be embedded within BletchMAME";
	}
	else if (!m_mameVersion.isEmpty() && !isMameVersionAtLeast(MameVersion::Capabilities::HAS_ATTACH_WINDOW.value()))
	{
		message = QString("MAME on this platform requires version of %1.%2 for -attach_window, and consequently the MAME window will not properly be embedded within BletchMAME").arg(
			QString::number(MameVersion::Capabilities::HAS_ATTACH_WINDOW.value().major()),
			QString::number(MameVersion::Capabilities::HAS_ATTACH_WINDOW.value().minor()));
	}

	// ...and display the warning if appropriate
	if (!message.isEmpty())
		messageBox(message);

	// the version check completed and we've dispensed with warnings; does this new version inform anything?
	if (m_mameVersion.isEmpty())
	{
		// no MAME found - reset the database
		m_info_db.reset();

		// and prompt if that is in the plan
		if (m_promptIfMameNotFound)
			promptForMameExecutable();
	}
	else if (m_mameVersion != m_info_db.version())		
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
//  WatchForImageMount
//-------------------------------------------------

void MainWindow::WatchForImageMount(const QString &tag)
{
	// find the current value; we want to monitor for this value changing
	QString current_value;
	const status::image *image = m_state->find_image(tag);
	if (image)
		current_value = image->m_file_name;

	// start watching
	m_watch_subscription = m_state->images().subscribe([this, current_value{std::move(current_value)}, tag]
	{
		// did the value change?
		const status::image *image = m_state->find_image(tag);
		if (image && image->m_file_name != current_value)
		{
			// it did!  place the new file in recent files
			PlaceInRecentFiles(tag, image->m_file_name);

			// and stop subscribing
			m_watch_subscription = observable::unique_subscription();
		}
	});	
}


//-------------------------------------------------
//  PlaceInRecentFiles
//-------------------------------------------------

void MainWindow::PlaceInRecentFiles(const QString &tag, const QString &path)
{
	// get the machine and device type to update recents
	info::machine machine = getRunningMachine();
	const QString &device_type = GetDeviceType(machine, tag);

	// actually edit the recent files; start by getting recent files
	std::vector<QString> &recent_files = m_prefs.getRecentDeviceFiles(machine.name(), device_type);

	// ...and clearing out places where that entry already exists
	std::vector<QString>::iterator iter;
	while ((iter = std::find(recent_files.begin(), recent_files.end(), path)) != recent_files.end())
		recent_files.erase(iter);

	// ...insert the new value
	recent_files.insert(recent_files.begin(), path);

	// and cull the list
	const size_t MAXIMUM_RECENT_FILES = 10;
	if (recent_files.size() > MAXIMUM_RECENT_FILES)
		recent_files.erase(recent_files.begin() + MAXIMUM_RECENT_FILES, recent_files.end());
}


//-------------------------------------------------
//  GetDeviceType
//-------------------------------------------------

const QString &MainWindow::GetDeviceType(const info::machine &machine, const QString &tag)
{
	auto iter = std::find_if(
		machine.devices().begin(),
		machine.devices().end(),
		[&tag](const info::device device)
		{
			return device.tag() == tag;
		});

	return iter != machine.devices().end()
		? iter->type()
		: util::g_empty_string;
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
//  associateFileDialogWithMachinePrefs
//-------------------------------------------------

void MainWindow::associateFileDialogWithMachinePrefs(QFileDialog &fileDialog, const QString &machineName, Preferences::machine_path_type pathType, bool pathIsFile)
{
	const QString &prefsPath = m_prefs.getMachinePath(machineName, pathType);
	if (!prefsPath.isEmpty())
	{
		// set the directory
		QString path = QDir::fromNativeSeparators(prefsPath);
		fileDialog.setDirectory(QFileInfo(path).dir());

		// try to set the file, if appropriate
		if (pathIsFile)
			fileDialog.selectFile(path);
	}

	// monitor dialog acceptance
	connect(&fileDialog, &QFileDialog::accepted, &fileDialog, [this, &fileDialog, machineName, pathType, pathIsFile]()
	{
		// get the result out of the dialog
		QStringList selectedFiles = fileDialog.selectedFiles();
		const QString &result = selectedFiles.first();

		// figure out the path we need to persist
		QString newPrefsPath;
		if (pathIsFile)
		{
			newPrefsPath = QDir::toNativeSeparators(result);
		}
		else
		{
			QString absolutePath = QFileInfo(result).dir().absolutePath();
			if (!absolutePath.endsWith("/"))
				absolutePath += "/";
			newPrefsPath = QDir::toNativeSeparators(absolutePath);
		}

		// and finally persist it
		m_prefs.setMachinePath(machineName, pathType, std::move(newPrefsPath));
	});
}


//-------------------------------------------------
//  getFileDialogFilename
//-------------------------------------------------

QString MainWindow::getFileDialogFilename(const QString &caption, Preferences::machine_path_type pathType, const QString &filter, QFileDialog::AcceptMode acceptMode, bool pathIsFile)
{
	// determine the file mode
	QFileDialog::FileMode fileMode;
	switch (acceptMode)
	{
	case QFileDialog::AcceptMode::AcceptOpen:
		fileMode = QFileDialog::FileMode::ExistingFile;
		break;
	case QFileDialog::AcceptMode::AcceptSave:
		fileMode = QFileDialog::FileMode::AnyFile;
		break;
	default:
		throw false;
	}

	// identify the default file and directory
	const QString &defaultPath = m_prefs.getMachinePath(
		getRunningMachine().name(),
		pathType);
	QFileInfo defaultPathInfo(QDir::fromNativeSeparators(defaultPath));

	// prepare the dialog
	QFileDialog dialog(this, caption, QString(), filter);
	dialog.setFileMode(fileMode);
	dialog.setAcceptMode(acceptMode);
	associateFileDialogWithMachinePrefs(dialog, getRunningMachine().name(), pathType, pathIsFile);

	// show the dialog
	dialog.exec();
	return dialog.result() == QDialog::DialogCode::Accepted
		? dialog.selectedFiles().first()
		: QString();
}


//-------------------------------------------------
//  fileDialogCommand
//-------------------------------------------------

QString MainWindow::fileDialogCommand(std::vector<QString> &&commands, const QString &caption, Preferences::machine_path_type pathType, bool path_is_file, const QString &wildcard_string, QFileDialog::AcceptMode acceptMode)
{
	Pauser pauser(*this);
	QString path = getFileDialogFilename(caption, pathType, wildcard_string, acceptMode, path_is_file);
	if (path.isEmpty())
		return { };

	// append the resulting path to the command list
	commands.push_back(path);

	// finally issue the actual commands
	issue(commands);
	return path;
}


//-------------------------------------------------
//  getTitleBarText
//-------------------------------------------------

QString MainWindow::getTitleBarText()
{
	// we want to append "PAUSED" if and only if the user paused, not as a consequence of a menu
	QString titleTextFormat = m_state->paused().get() && !m_current_pauser
		? "%1: %2 PAUSED"
		: "%1: %2";

	// get the machine description
	const QString &machineDesc = m_currentRunMachineTask->getMachine().description();

	// and apply the format
	return titleTextFormat.arg(
		QCoreApplication::applicationName(),
		machineDesc);
}


//-------------------------------------------------
//  ensureProperFocus
//-------------------------------------------------

void MainWindow::ensureProperFocus()
{
#ifdef WIN32
	// Windows specific hack - for some reason, the act of moving the focus away
	// from the application (even it is for a dialog) seems to cause the rootWidget
	// to lose focus.  Setting the Qt focusPolicy property doesn't seem to help
	//
	// In absence of a better way to handle this, we have some Windows specific
	// code below.  Eventually this code should be retired once there is an understanding
	// of the proper Qt techniques to apply here
	if (!attachToMainWindow() && m_currentRunMachineTask)
	{
		if (::GetFocus() == (HWND)winId())
			::SetFocus((HWND)m_ui->rootWidget->winId());
	}
#endif
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
		if (!m_state.has_value())
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
	if (!m_pinging && m_state.has_value())
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
//  auditIfAppropriate
//-------------------------------------------------

void MainWindow::auditIfAppropriate(const info::machine &machine)
{
	// if we can automatically audit, and this status is unknown...
	if (canAutomaticallyAudit()
		&& m_prefs.getMachineAuditStatus(machine.name()) == AuditStatus::Unknown)
	{
		// then add it to the queue
		MachineAuditIdentifier identifier(machine.name());
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
		SoftwareAuditIdentifier identifier(software.parent().name(), software.name());
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

const QString *MainWindow::auditIdentifierString(const AuditIdentifier &identifier) const
{
	const QString *result = nullptr;

	std::visit([this, &result](auto &&identifier)
	{
		using T = std::decay_t<decltype(identifier)>;
		if constexpr (std::is_same_v<T, MachineAuditIdentifier>)
		{
			std::optional<info::machine> machine = m_info_db.find_machine(identifier.machineName());
			if (machine)
				result = &machine->description();
		}
		else if constexpr (std::is_same_v<T, SoftwareAuditIdentifier>)
		{
			const software_list::software *software = m_softwareListCollection.find_software_by_list_and_name(
				identifier.softwareList(),
				identifier.software());
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
		std::optional<AuditIdentifier> identifier;
		while (m_auditQueue.isCloseToEmpty() && (identifier = m_auditCursor.next(basePosition)).has_value())
		{
			m_auditQueue.push(std::move(identifier.value()), false);
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
	}
	return true;
}


//-------------------------------------------------
//  onAuditSingleMedia
//-------------------------------------------------

bool MainWindow::onAuditSingleMedia(const AuditSingleMediaEvent &event)
{
	if (m_currentAuditDialog)
		m_currentAuditDialog->singleMediaAudited(event.entryIndex(), event.verdict());
	return true;
}
