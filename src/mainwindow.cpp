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
#include <QFileDialog>
#include <QSortFilterProxyModel>
#include <QTextStream>

#include "mainwindow.h"
#include "mameversion.h"
#include "ui_mainwindow.h"
#include "machinelistitemmodel.h"
#include "softwarelistitemmodel.h"
#include "profilelistitemmodel.h"
#include "tableviewmanager.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "versiontask.h"
#include "utility.h"
#include "dialogs/about.h"
#include "dialogs/choosesw.h"
#include "dialogs/console.h"
#include "dialogs/images.h"
#include "dialogs/inputs.h"
#include "dialogs/loading.h"
#include "dialogs/paths.h"
#include "dialogs/switches.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// BletchMAME requires MAME 0.213 or later
const MameVersion REQUIRED_MAME_VERSION = MameVersion(0, 213, false);


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
			m_host.ChangePaused(true);

		// track the chain of pausers
		m_host.m_current_pauser = this;
	}

	~Pauser()
	{
		if (m_is_running)
			m_host.ChangePaused(false);
		m_host.m_current_pauser = m_last_pauser;
	}

private:
	MainWindow &	m_host;
	const Pauser *	m_last_pauser;
	bool			m_is_running;
};


// ======================> ImagesHost

class MainWindow::ImagesHost : public IImagesHost
{
public:
	ImagesHost(MainWindow &host)
		: m_host(host)
	{
	}

	virtual info::machine GetMachine()
	{
		return m_host.GetRunningMachine();
	}

	virtual Preferences &GetPreferences()
	{
		return m_host.m_prefs;
	}

	virtual observable::value<std::vector<status::image>> &GetImages()
	{
		return m_host.m_state->images();
	}

	virtual const QString &GetWorkingDirectory() const
	{
		return m_host.m_prefs.GetMachinePath(GetMachineName(), Preferences::machine_path_type::WORKING_DIRECTORY);
	}

	virtual void SetWorkingDirectory(QString &&dir)
	{
		m_host.m_prefs.SetMachinePath(GetMachineName(), Preferences::machine_path_type::WORKING_DIRECTORY, std::move(dir));
	}

	virtual const std::vector<QString> &GetRecentFiles(const QString &tag) const
	{
		info::machine machine = m_host.GetRunningMachine();
		const QString &device_type = GetDeviceType(machine, tag);
		return m_host.m_prefs.GetRecentDeviceFiles(machine.name(), device_type);
	}

	virtual std::vector<QString> GetExtensions(const QString &tag) const
	{
		// find the device declaration
		auto devices = m_host.GetRunningMachine().devices();

		auto iter = std::find_if(devices.begin(), devices.end(), [&tag](info::device dev)
		{
			return dev.tag() == tag;
		});
		assert(iter != devices.end());

		// and return it!
		return util::string_split((*iter).extensions(), [](auto ch) { return ch == ','; });
	}

	virtual void CreateImage(const QString &tag, QString &&path)
	{
		m_host.WatchForImageMount(tag);
		m_host.Issue({ "create", tag, std::move(path) });
	}

	virtual void LoadImage(const QString &tag, QString &&path)
	{
		m_host.WatchForImageMount(tag);
		m_host.Issue({ "load", tag, std::move(path) });
	}

	virtual void UnloadImage(const QString &tag)
	{
		m_host.Issue({ "unload", tag });
	}

private:
	MainWindow &m_host;

	const QString &GetMachineName() const
	{
		return m_host.GetRunningMachine().name();
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
		m_host.Issue(args);
	}

	virtual void StartPolling(const QString &port_tag, ioport_value mask, status::input_seq::type seq_type, const QString &start_seq_tokens) override
	{
		m_host.Issue({ "seq_poll_start", port_tag, QString::number(mask), SeqTypeString(seq_type), start_seq_tokens });
	}

	virtual void StopPolling() override
	{
		m_host.Issue({ "seq_poll_stop" });
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
		m_host.Issue({ "set_input_value", port_tag, QString::number(mask), QString::number(value) });
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
		m_host.m_state->phase().subscribe([this]() { update(); });
		m_host.m_state->speed_percent().subscribe([this]() { update(); });
		m_host.m_state->effective_frameskip().subscribe([this]() { update(); });
		m_host.m_state->startup_text().subscribe([this]() { update(); });
		m_host.m_state->images().subscribe([this]() { update(); });
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
		m_host.m_menu_bar_shown = m_host.m_prefs.GetMenuBarShown();

		// mouse capturing is a bit more involved
		m_mouseCaptured = observable::observe(m_host.m_state->has_input_using_mouse() && !m_host.m_menu_bar_shown);
		m_mouseCaptured.subscribe([this]()
		{
			m_host.Issue({ "SET_MOUSE_ENABLED", m_mouseCaptured ? "true" : "false" });
			m_host.setCursor(m_mouseCaptured.get() ? Qt::BlankCursor : Qt::ArrowCursor);
		});
	}

	virtual void stop()
	{
		m_host.m_prefs.SetMenuBarShown(m_host.m_menu_bar_shown.get());
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

static const TableViewManager::ColumnDesc s_machineListTableViewColumns[] =
{
	{ "name",			85 },
	{ "description",	370 },
	{ "year",			50 },
	{ "manufacturer",	320 },
	{ nullptr }
};

static const TableViewManager::Description s_machineListTableViewDesc =
{
	"machine",
	(int) MachineListItemModel::Column::Machine,
	s_machineListTableViewColumns
};

static const TableViewManager::ColumnDesc s_profileListTableViewColumns[] =
{
	{ "name",			85 },
	{ "machine",		85 },
	{ "path",			600 },
	{ nullptr }
};

static const TableViewManager::Description s_profileListTableViewDesc =
{
	"profile",
	(int)ProfileListItemModel::Column::Path,
	s_profileListTableViewColumns
};


static const int SOUND_ATTENUATION_OFF = -32;
static const int SOUND_ATTENUATION_ON = 0;


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_client(*this, m_prefs)
	, m_softwareListItemModel(nullptr)
	, m_profileListItemModel(nullptr)
	, m_pinging(false)
	, m_current_pauser(nullptr)
	, m_icon_loader(m_prefs)
{
	// set up Qt form
	m_ui = std::make_unique<Ui::MainWindow>();
	m_ui->setupUi(this);

	// initial preferences read
	m_prefs.Load();

	// set up machines view
	QAbstractItemModel &machineListItemModel = *new MachineListItemModel(this, m_info_db, m_icon_loader);
	TableViewManager::setup(
		*m_ui->machinesTableView,
		machineListItemModel,
		m_ui->machinesSearchBox,
		m_prefs,
		s_machineListTableViewDesc);

	// set up software list view
	m_softwareListItemModel = new SoftwareListItemModel(this);
	TableViewManager::setup(
		*m_ui->softwareTableView,
		*m_softwareListItemModel,
		m_ui->softwareSearchBox,
		m_prefs,
		ChooseSoftlistPartDialog::s_tableViewDesc);

	// set up the profile list view
	m_profileListItemModel = new ProfileListItemModel(this, m_prefs, m_info_db, m_icon_loader);
	TableViewManager::setup(
		*m_ui->profilesTableView,
		*m_profileListItemModel,
		nullptr,
		m_prefs,
		s_profileListTableViewDesc);
	m_profileListItemModel->refresh(true, true);

	// set up the ping timer
	QTimer &pingTimer = *new QTimer(this);
	connect(&pingTimer, &QTimer::timeout, this, &MainWindow::InvokePing);
	setupActionAspect([&pingTimer]() { pingTimer.start(500); }, [&pingTimer]() { pingTimer.stop(); });

	// setup properties that pertain to runtime behavior
	setupPropSyncAspect((QWidget &) *m_ui->tabWidget,			&QWidget::isEnabled,	&QWidget::setEnabled,		{ },								false);
	setupPropSyncAspect((QWidget &) *m_ui->tabWidget,			&QWidget::isHidden,		&QWidget::setHidden,		{ },								true);
	setupPropSyncAspect(*m_ui->rootWidget,						&QWidget::isHidden,		&QWidget::setHidden,		{ },								[this]() { return !AttachToRootPanel(); });
	setupPropSyncAspect((QWidget &) *this,						&QWidget::windowTitle,	&QWidget::setWindowTitle,	&status::state::paused,				[this]() { return getTitleBarText(); });

	// actions
	setupPropSyncAspect(*m_ui->actionStop,						&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionPause,						&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionPause,						&QAction::isChecked,	&QAction::setChecked,		&status::state::paused,				[this]() { return m_state->paused().get(); });
	setupPropSyncAspect(*m_ui->actionImages,					&QAction::isEnabled,	&QAction::setEnabled,		&status::state::images,				[this]() { return m_state->images().get().size() > 0;});
	setupPropSyncAspect(*m_ui->actionLoadState,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionSaveState,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionSaveScreenshot,			&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionToggleRecordMovie,			&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionDebugger,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionSoftReset,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionHardReset,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionIncreaseSpeed,				&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionDecreaseSpeed,				&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionWarpMode,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionToggleSound,				&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionToggleSound,				&QAction::isChecked,	&QAction::setChecked,		&status::state::sound_attenuation,	[this]() { return m_state->sound_attenuation().get() != SOUND_ATTENUATION_OFF; });
	setupPropSyncAspect(*m_ui->actionConsole,					&QAction::isEnabled,	&QAction::setEnabled,		{ },								true);
	setupPropSyncAspect(*m_ui->actionJoysticksAndControllers,	&QAction::isEnabled,	&QAction::setEnabled,		&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::CONTROLLER); });
	setupPropSyncAspect(*m_ui->actionKeyboard,					&QAction::isEnabled,	&QAction::setEnabled,		&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::KEYBOARD); });
	setupPropSyncAspect(*m_ui->actionMiscellaneousInput,		&QAction::isEnabled,	&QAction::setEnabled,		&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::MISC); });
	setupPropSyncAspect(*m_ui->actionConfiguration,				&QAction::isEnabled,	&QAction::setEnabled,		&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::CONFIG); });
	setupPropSyncAspect(*m_ui->actionDipSwitches,				&QAction::isEnabled,	&QAction::setEnabled,		&status::state::inputs,				[this]() { return m_state->has_input_class(status::input::input_class::DIPSWITCH); });

	// special setup for throttle dynamic menu
	QAction &throttleSeparator = *m_ui->menuThrottle->actions()[0];
	for (size_t i = 0; i < sizeof(s_throttle_rates) / sizeof(s_throttle_rates[0]); i++)
	{
		float throttle_rate = s_throttle_rates[i];
		QString text = QString::number((int)(throttle_rate * 100)) + "%";
		QAction &action = *new QAction(text, m_ui->menuThrottle);
		m_ui->menuThrottle->insertAction(&throttleSeparator, &action);
		action.setEnabled(false);
		action.setCheckable(true);

		connect(&action, &QAction::triggered, this, [this, throttle_rate]() { ChangeThrottleRate(throttle_rate); });
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

		connect(&action, &QAction::triggered, this, [this, value{ value.toStdString()}]() { Issue({ "frameskip", value }); });
		setupPropSyncAspect(action, &QAction::isEnabled, &QAction::setEnabled, { },							true);
		setupPropSyncAspect(action, &QAction::isChecked, &QAction::setChecked, &status::state::frameskip,	[this, value{std::move(value)}]() { return m_state->frameskip().get() == value; });
	}

	// set up the tab widget
	m_ui->tabWidget->setCurrentIndex(static_cast<int>(m_prefs.GetSelectedTab()));

	// set up other miscellaneous aspects
	m_aspects.push_back(std::make_unique<StatusBarAspect>(*this));
	m_aspects.push_back(std::make_unique<MenuBarAspect>(*this));
	m_aspects.push_back(std::make_unique<ToggleMovieTextAspect>(m_current_recording_movie_filename, *m_ui->actionToggleRecordMovie));

	// time for the initial check
	InitialCheckMameInfoDatabase();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MainWindow::~MainWindow()
{
	m_prefs.Save();
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
	if (shouldPromptOnStop())
	{
		QString message = "Do you really want to stop?\n"
			"\n"
			"All data in emulated RAM will be lost";

		if (messageBox(message, QMessageBox::Yes | QMessageBox::No) != QMessageBox::StandardButton::Yes)
			return;
	}

	InvokeExit();
}


//-------------------------------------------------
//  on_actionPause_triggered
//-------------------------------------------------

void MainWindow::on_actionPause_triggered()
{
	ChangePaused(!m_state->paused().get());
}


//-------------------------------------------------
//  on_actionImages_triggered
//-------------------------------------------------

void MainWindow::on_actionImages_triggered()
{
	Pauser pauser(*this);
	ImagesHost images_host(*this);
	ImagesDialog dialog(*this, images_host, false);
	dialog.exec();
}


//-------------------------------------------------
//  on_actionLoadState_triggered
//-------------------------------------------------

void MainWindow::on_actionLoadState_triggered()
{
	FileDialogCommand(
		{ "state_load" },
		"Load State",
		Preferences::machine_path_type::LAST_SAVE_STATE,
		true,
		s_wc_saved_state,
		QFileDialog::AcceptMode::AcceptOpen);
}


//-------------------------------------------------
//  on_actionSaveState_triggered
//-------------------------------------------------

void MainWindow::on_actionSaveState_triggered()
{
	FileDialogCommand(
		{ "state_save" },
		"Save State",
		Preferences::machine_path_type::LAST_SAVE_STATE,
		true,
		s_wc_saved_state,
		QFileDialog::AcceptMode::AcceptSave);
}


//-------------------------------------------------
//  on_actionSaveScreenshot_triggered
//-------------------------------------------------

void MainWindow::on_actionSaveScreenshot_triggered()
{
	FileDialogCommand(
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
	// recording movies by specifying absolute paths was introduced in MAME 0.221
	const MameVersion REQUIRED_MAME_VERSION_TOGGLE_MOVIE = MameVersion(0, 220, true);

	// Are we the required version? 
	if (!isMameVersionAtLeast(REQUIRED_MAME_VERSION_TOGGLE_MOVIE))
	{
		QString message = QString("Recording movies requires MAME %1 or newer to function")
			.arg(REQUIRED_MAME_VERSION_TOGGLE_MOVIE.NextCleanVersion().ToString());
		messageBox(message);
		return;
	}

	// We're toggling the movie status... so are we recording?
	if (m_state.value().is_recording())
	{
		// If so, stop the recording
		Issue({ "end_recording" });
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
		QString path = GetFileDialogFilename(
			"Record Movie",
			Preferences::machine_path_type::WORKING_DIRECTORY,
			s_wc_record_movie,
			QFileDialog::AcceptMode::AcceptSave);
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
			Issue({ "begin_recording", std::move(nativePath), recordingTypeString });
			m_current_recording_movie_filename = std::move(path);
		}
	}
}


//-------------------------------------------------
//  on_actionDebugger_triggered
//-------------------------------------------------

void MainWindow::on_actionDebugger_triggered()
{
	Issue("debugger");
}


//-------------------------------------------------
//  on_actionSoftReset_triggered
//-------------------------------------------------

void MainWindow::on_actionSoftReset_triggered()
{
	Issue("soft_reset");
}


//-------------------------------------------------
//  on_actionHard_Reset_triggered
//-------------------------------------------------

void MainWindow::on_actionHardReset_triggered()
{
	Issue("hard_reset");
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
	ChangeThrottleRate(-1);
}


//-------------------------------------------------
//  on_actionDecreaseSpeed_triggered
//-------------------------------------------------

void MainWindow::on_actionDecreaseSpeed_triggered()
{
	ChangeThrottleRate(+1);
}


//-------------------------------------------------
//  on_actionWarpMode_triggered
//-------------------------------------------------

void MainWindow::on_actionWarpMode_triggered()
{
	ChangeThrottled(!m_state->throttled());
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
	ChangeSound(!isEnabled);
}


//-------------------------------------------------
//  on_actionConsole_triggered
//-------------------------------------------------

void MainWindow::on_actionConsole_triggered()
{
	ConsoleDialog dialog(this, m_client.GetCurrentTask<RunMachineTask>(), *this);
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
	std::vector<Preferences::global_path_type> changed_paths;

	// show the dialog
	{
		Pauser pauser(*this);
		PathsDialog dialog(*this, m_prefs);
		dialog.exec();
		if (dialog.result() == QDialog::DialogCode::Accepted)
		{
			changed_paths = dialog.persist();
			m_prefs.Save();
		}
	}

	// lambda to simplify "is this path changed?"
	auto is_changed = [&changed_paths](Preferences::global_path_type type) -> bool
	{
		auto iter = std::find(changed_paths.begin(), changed_paths.end(), type);
		return iter != changed_paths.end();
	};

	// did the user change the executable path?
	if (is_changed(Preferences::global_path_type::EMU_EXECUTABLE))
	{
		// they did; check the MAME info DB
		check_mame_info_status status = CheckMameInfoDatabase();
		switch (status)
		{
		case check_mame_info_status::SUCCESS:
			// we're good!
			break;

		case check_mame_info_status::MAME_NOT_FOUND:
		case check_mame_info_status::DB_NEEDS_REBUILD:
			// in both of these scenarios, we need to clear out the list
			m_info_db.reset();

			// start a rebuild if that is the only problem
			if (status == check_mame_info_status::DB_NEEDS_REBUILD)
				refreshMameInfoDatabase();
			break;

		default:
			throw false;
		}
	}

	// did the user change the profiles path?
	if (is_changed(Preferences::global_path_type::PROFILES))
		m_profileListItemModel->refresh(true, true);

	// did the user change the icons path?
	if (is_changed(Preferences::global_path_type::ICONS))
	{
		m_icon_loader.refreshIcons();
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
//  on_machinesTableView_activated
//-------------------------------------------------

void MainWindow::on_machinesTableView_activated(const QModelIndex &index)
{
	// run the machine
	const info::machine machine = machineFromModelIndex(index);
	Run(machine);
}


//-------------------------------------------------
//  on_machinesTableView_customContextMenuRequested
//-------------------------------------------------

void MainWindow::on_machinesTableView_customContextMenuRequested(const QPoint &pos)
{
	LaunchingListContextMenu(m_ui->machinesTableView->mapToGlobal(pos));
}


//-------------------------------------------------
//  on_softwareTableView_activated
//-------------------------------------------------

void MainWindow::on_softwareTableView_activated(const QModelIndex &index)
{
	// identify the machine
	const info::machine machine = m_info_db.find_machine(m_currentSoftwareList).value();

	// map the index to the actual index
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->softwareTableView).mapToSource(index);

	// identify the software
	const software_list::software &software = m_softwareListItemModel->getSoftwareByIndex(actualIndex.row());

	// and run!
	Run(machine, &software);
}


//-------------------------------------------------
//  on_softwareTableView_customContextMenuRequested
//-------------------------------------------------

void MainWindow::on_softwareTableView_customContextMenuRequested(const QPoint &pos)
{
	// identify the selected software
	QModelIndexList selection = m_ui->softwareTableView->selectionModel()->selectedIndexes();
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->softwareTableView).mapToSource(selection[0]);
	const software_list::software &sw = m_softwareListItemModel->getSoftwareByIndex(actualIndex.row());
	
	// and launch the context menu
	LaunchingListContextMenu(m_ui->softwareTableView->mapToGlobal(pos), &sw);
}


//-------------------------------------------------
//  on_profilesTableView_activated
//-------------------------------------------------

void MainWindow::on_profilesTableView_activated(const QModelIndex &index)
{
	// map the index to the actual index
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapToSource(index);

	// identify the profile
	const profiles::profile &profile = m_profileListItemModel->getProfileByIndex(actualIndex.row());

	// and run!
	Run(profile);
}


//-------------------------------------------------
//  on_profilesTableView_customContextMenuRequested
//-------------------------------------------------

void MainWindow::on_profilesTableView_customContextMenuRequested(const QPoint &pos)
{
	// identify the selected software
	QModelIndexList selection = m_ui->profilesTableView->selectionModel()->selectedIndexes();
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapToSource(selection[0]);
	const profiles::profile &profile = m_profileListItemModel->getProfileByIndex(actualIndex.row());

	// build the popup menu
	QMenu popupMenu;
	popupMenu.addAction(QString("Run \"%1\"").arg(profile.name()),	[this, &profile]() { Run(profile); });
	popupMenu.addAction("Duplicate",								[this, &profile]() { DuplicateProfile(profile); });
	popupMenu.addAction("Rename",									[this, &profile]() { RenameProfile(profile); });
	popupMenu.addAction("Delete",									[this, &profile]() { DeleteProfile(profile); });
	popupMenu.addSeparator();
	popupMenu.addAction("Show in folder",							[this, &profile]() { showInGraphicalShell(profile.path()); });
	popupMenu.exec(m_ui->profilesTableView->mapToGlobal(pos));
}


//-------------------------------------------------
//  on_tabWidget_currentChanged
//-------------------------------------------------

void MainWindow::on_tabWidget_currentChanged(int index)
{
	Preferences::list_view_type list_view_type = static_cast<Preferences::list_view_type>(index);
	m_prefs.SetSelectedTab(list_view_type);

	switch (list_view_type)
	{
	case Preferences::list_view_type::SOFTWARELIST:
		updateSoftwareList();
		break;
	}
}


//-------------------------------------------------
//  event
//-------------------------------------------------

bool MainWindow::event(QEvent *event)
{
	std::optional<bool> result = { };
	if (event->type() == VersionResultEvent::eventId())
	{
		result = onVersionCompleted(static_cast<VersionResultEvent &>(*event));
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
	const QString &path = m_prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
	return !path.isEmpty() && wxFileExists(path);
}


//-------------------------------------------------
//  InitialCheckMameInfoDatabase - called when we
//	load up for the very first time
//-------------------------------------------------

void MainWindow::InitialCheckMameInfoDatabase()
{
	bool done = false;
	while (!done)
	{
		switch (CheckMameInfoDatabase())
		{
		case check_mame_info_status::SUCCESS:
			// we're good!
			done = true;
			break;

		case check_mame_info_status::MAME_NOT_FOUND:
			// prompt the user for the MAME executable
			if (!PromptForMameExecutable())
			{
				// the (l)user gave up; guess we're done...
				done = true;
			}
			break;

		case check_mame_info_status::DB_NEEDS_REBUILD:
			// start a rebuild; whether the process succeeds or fails, we're done
			refreshMameInfoDatabase();
			done = true;
			break;

		default:
			throw false;
		}
	}
}


//-------------------------------------------------
//  CheckMameInfoDatabase - checks the version and
//	the MAME info DB
//
//	how to respond to failure conditions is up to
//	the caller
//-------------------------------------------------

MainWindow::check_mame_info_status MainWindow::CheckMameInfoDatabase()
{
	// first thing, check to see if the executable is there
	if (!IsMameExecutablePresent())
		return check_mame_info_status::MAME_NOT_FOUND;

	// get the version - this should be blazingly fast
	m_client.launch(create_version_task());
	while (m_client.IsTaskActive())
	{
		QCoreApplication::processEvents();
		QThread::yieldCurrentThread();
	}

	// we didn't get a version?  treat this as if we cannot find the
	// executable
	if (m_mame_version.isEmpty())
		return check_mame_info_status::MAME_NOT_FOUND;

	// now let's try to open the info DB; we expect a specific version
	QString db_path = m_prefs.GetMameXmlDatabasePath();
	if (!m_info_db.load(db_path, m_mame_version))
		return check_mame_info_status::DB_NEEDS_REBUILD;

	// success!  we can update the machine list
	return check_mame_info_status::SUCCESS;
}


//-------------------------------------------------
//  PromptForMameExecutable
//-------------------------------------------------

bool MainWindow::PromptForMameExecutable()
{
	QString path = PathsDialog::browseForPathDialog(*this, Preferences::global_path_type::EMU_EXECUTABLE, m_prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE));
	if (path.isEmpty())
		return false;

	m_prefs.SetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE, std::move(path));
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
	QString db_path = m_prefs.GetMameXmlDatabasePath();
	m_client.launch(std::make_shared<ListXmlTask>(std::move(db_path)));

	// and show the dialog
	{
		LoadingDialog dlg(*this, [this]() { return !m_client.IsTaskActive(); });
		dlg.exec();
		if (dlg.result() != QDialog::DialogCode::Accepted)
		{
			m_client.abort();
			return false;
		}
	}

	// we've succeeded; load the DB
	if (!m_info_db.load(db_path))
	{
		// a failure here is likely due to a very strange condition (e.g. - someone deleting the infodb
		// file out from under me)
		return false;
	}

	return true;
}


//-------------------------------------------------
//  GetRunningMachine
//-------------------------------------------------

info::machine MainWindow::GetRunningMachine() const
{
	// get the currently running machine
	std::shared_ptr<const RunMachineTask> task = m_client.GetCurrentTask<const RunMachineTask>();

	// this call is only valid if we have a running machine
	assert(task);

	// return the machine
	return task->getMachine();
}


//-------------------------------------------------
//  AttachToRootPanel
//-------------------------------------------------

bool MainWindow::AttachToRootPanel() const
{
	bool result = false;
	if (HAS_ATTACH_WINDOW)
	{
		// Targetting subwindows with -attach_window was introduced in between MAME 0.217 and MAME 0.218
		const MameVersion REQUIRED_MAME_VERSION_ATTACH_TO_CHILD_WINDOW = MameVersion(0, 217, true);

		// Are we the required version?
		result = isMameVersionAtLeast(REQUIRED_MAME_VERSION_ATTACH_TO_CHILD_WINDOW);
	}
	return result;
}


//-------------------------------------------------
//  Run
//-------------------------------------------------

void MainWindow::Run(const info::machine &machine, const software_list::software *software, const profiles::profile *profile)
{
	// run a "preflight check" on MAME, to catch obvious problems that might not be caught or reported well
	QString preflight_errors = preflightCheck();
	if (!preflight_errors.isEmpty())
	{
		messageBox(preflight_errors);
		return;
	}

	// identify the software name; we either used what was passed in, or we use what is in a profile
	// for which no images are mounted (suggesting a fresh launch)
	QString software_name;
	if (software)
		software_name = software->m_name;
	else if (profile && profile->images().empty())
		software_name = profile->software();

	// we need to have full information to support the emulation session; retrieve
	// fake a pauser to forestall "PAUSED" from appearing in the menu bar
	Pauser fake_pauser(*this, false);

	// run the emulation
	Task::ptr task = std::make_shared<RunMachineTask>(
		machine,
		std::move(software_name),
		AttachToRootPanel() ? *m_ui->rootWidget : *this);
	m_client.launch(std::move(task));

	// set up running state and subscribe to events
	m_state.emplace();

	// execute the start handler for all aspects
	for (const auto &aspect : m_aspects)
		aspect->start();

	// set the focus to the main window
	setFocus();

	// wait for first ping
	m_pinging = true;
	while (m_pinging)
	{
		if (!m_state.has_value())
			return;
		QCoreApplication::processEvents();
		QThread::yieldCurrentThread();
	}

	// set up profile (if we have one)
	m_current_profile_path = profile ? profile->path() : util::g_empty_string;
	m_current_profile_auto_save_state = profile ? profile->auto_save_states() : false;
	if (profile)
	{
		// load all images
		for (const auto &image : profile->images())
			Issue({ "load", image.m_tag, image.m_path });

		// if we have a save state, start it
		if (profile->auto_save_states())
		{
			QString save_state_path = profiles::profile::change_path_save_state(profile->path());
			QFileInfo fi(save_state_path);
			if (fi.exists())
				Issue({ "state_load", save_state_path });
		}
	}

	// do we have any images that require images?
	auto iter = std::find_if(m_state->images().get().cbegin(), m_state->images().get().cend(), [](const status::image &image)
	{
		return image.m_must_be_loaded && image.m_file_name.isEmpty();
	});
	if (iter != m_state->images().get().cend())
	{
		// if so, show the dialog
		ImagesHost images_host(*this);
		ImagesDialog dialog(*this, images_host, true);
		dialog.exec();
		if (dialog.result() != QDialog::DialogCode::Accepted)
		{
			Issue("exit");
			return;
		}
	}

	// unpause
	ChangePaused(false);
}


void MainWindow::Run(const profiles::profile &profile)
{
	// find the machine
	std::optional<info::machine> machine = m_info_db.find_machine(profile.machine());
	if (!machine)
	{
		messageBox("Unknown machine: " + profile.machine());
		return;
	}

	Run(machine.value(), nullptr, &profile);
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
	QStringList paths = m_prefs.GetSplitPaths(Preferences::global_path_type::PLUGINS);
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
		bool success = util::find_if_ptr(paths, [&paths, &files](const QString &path)
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
		if (shouldPromptOnStop())
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
		InvokeExit();
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
//  shouldPromptOnStop
//-------------------------------------------------

bool MainWindow::shouldPromptOnStop() const
{
	return m_current_profile_path.isEmpty() || !m_current_profile_auto_save_state;
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
	SwitchesDialog dialog(this, host, input_class, GetRunningMachine());
	dialog.exec();
}


//-------------------------------------------------
//  isMameVersionAtLeast
//-------------------------------------------------

bool MainWindow::isMameVersionAtLeast(const MameVersion &version) const
{
	return MameVersion(m_mame_version).IsAtLeast(version);
}


//-------------------------------------------------
//  onVersionCompleted
//-------------------------------------------------

bool MainWindow::onVersionCompleted(VersionResultEvent &event)
{
	m_mame_version = std::move(event.m_version);

	// warn the user if this is version of MAME is not supported
	if (!isMameVersionAtLeast(REQUIRED_MAME_VERSION))
	{
		QString message = QString("This version of MAME doesn't seem to be supported; BletchMAME requires MAME %1.%2 or newer to function correctly").arg(
			QString::number(REQUIRED_MAME_VERSION.Major()),
			QString::number(REQUIRED_MAME_VERSION.Minor()));
		messageBox(message);
	}

	m_client.waitForCompletion();
	return true;
}


//-------------------------------------------------
//  onListXmlCompleted
//-------------------------------------------------

bool MainWindow::onListXmlCompleted(const ListXmlResultEvent &event)
{
	// check the status
	switch (event.status())
	{
	case ListXmlResultEvent::Status::SUCCESS:
		// if it succeeded, try to load the DB
		{
			QString db_path = m_prefs.GetMameXmlDatabasePath();
			m_info_db.load(db_path);
		}
		break;

	case ListXmlResultEvent::Status::ABORTED:
		// if we aborted, do nothing
		break;

	case ListXmlResultEvent::Status::ERROR:
		// present an error message
		messageBox(!event.errorMessage().isEmpty()
			? event.errorMessage()
			: "Error building MAME info database");
		break;

	default:
		throw false;
	}

	m_client.waitForCompletion();
	return true;
}


//-------------------------------------------------
//  onRunMachineCompleted
//-------------------------------------------------

bool MainWindow::onRunMachineCompleted(const RunMachineCompletedEvent &event)
{
	// update the profile, if present
	if (!m_current_profile_path.isEmpty())
	{
		std::optional<profiles::profile> profile = profiles::profile::load(m_current_profile_path);
		if (profile)
		{
			profile->images().clear();
			for (const status::image &status_image : m_state->images().get())
			{
				if (!status_image.m_file_name.isEmpty())
				{
					profiles::image &profile_image = profile->images().emplace_back();
					profile_image.m_tag = status_image.m_tag;
					profile_image.m_path = status_image.m_file_name;
				}
			}
			profile->save();
		}
	}

	// clear out all of the state
	m_client.waitForCompletion();
	m_state.reset();
	m_current_profile_path = util::g_empty_string;
	m_current_profile_auto_save_state = false;

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
//  updateSoftwareList
//-------------------------------------------------

void MainWindow::updateSoftwareList()
{
	// identify the selection
	QModelIndexList selection = m_ui->machinesTableView->selectionModel()->selectedIndexes();
	if (selection.size() > 0)
	{
		// load software lists for the current machine
		const info::machine machine = machineFromModelIndex(selection[0]);

		// load the software
		m_currentSoftwareList = machine.name();
		m_softwareListCollection.load(m_prefs, machine);

		// and load the model
		m_softwareListItemModel->load(m_softwareListCollection, false);
	}
	else
	{
		// no machines are selected - reset the software list view
		m_softwareListItemModel->reset();
	}
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
	info::machine machine = GetRunningMachine();
	const QString &device_type = GetDeviceType(machine, tag);

	// actually edit the recent files; start by getting recent files
	std::vector<QString> &recent_files = m_prefs.GetRecentDeviceFiles(machine.name(), device_type);

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
	static const QString s_empty;

	auto iter = std::find_if(
		machine.devices().begin(),
		machine.devices().end(),
		[&tag](const info::device device)
		{
			return device.tag() == tag;
		});

	return iter != machine.devices().end()
		? (*iter).type()
		: s_empty;
}


//-------------------------------------------------
//  SetChatterListener
//-------------------------------------------------

void MainWindow::SetChatterListener(std::function<void(const ChatterEvent &chatter)> &&func)
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
//  GetFileDialogFilename
//-------------------------------------------------

QString MainWindow::GetFileDialogFilename(const QString &caption, Preferences::machine_path_type pathType, const QString &filter, QFileDialog::AcceptMode acceptMode)
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
	const QString &defaultPath = m_prefs.GetMachinePath(
		GetRunningMachine().name(),
		pathType);
	QFileInfo defaultPathInfo(QDir::fromNativeSeparators(defaultPath));

	// prepare the dialog
	QFileDialog dialog(this, caption, defaultPathInfo.dir().absolutePath(), filter);
	dialog.setFileMode(fileMode);
	dialog.setAcceptMode(acceptMode);
	if (!defaultPathInfo.fileName().isEmpty())
		dialog.selectFile(defaultPathInfo.fileName());

	// show the dialog
	dialog.exec();
	return dialog.result() == QDialog::DialogCode::Accepted
		? dialog.selectedFiles().first()
		: QString();
}


//-------------------------------------------------
//  FileDialogCommand
//-------------------------------------------------

void MainWindow::FileDialogCommand(std::vector<QString> &&commands, const QString &caption, Preferences::machine_path_type pathType, bool path_is_file, const QString &wildcard_string, QFileDialog::AcceptMode acceptMode)
{
	Pauser pauser(*this);
	QString path = GetFileDialogFilename(caption, pathType, wildcard_string, acceptMode);
	if (path.isEmpty())
		return;

	// append the resulting path to the command list
	commands.push_back(std::move(path));

	// put back the default
	const QString &running_machine_name(GetRunningMachine().name());
	if (path_is_file)
	{
		m_prefs.SetMachinePath(running_machine_name, pathType, QDir::toNativeSeparators(path));
	}
	else
	{
		QString new_dir;
		wxFileName::SplitPath(path, &new_dir, nullptr, nullptr);
		m_prefs.SetMachinePath(running_machine_name, pathType, std::move(new_dir));
	}

	// finally issue the actual commands
	Issue(commands);
}


//-------------------------------------------------
//  LaunchingListContextMenu
//-------------------------------------------------

void MainWindow::LaunchingListContextMenu(const QPoint &pos, const software_list::software *software)
{
	// identify the machine
	QModelIndex index = m_ui->machinesTableView->selectionModel()->selectedIndexes()[0];
	const info::machine machine = machineFromModelIndex(index);

	// identify the description
	const QString &description = software
		? software->m_description
		: machine.description();

	QMenu popupMenu(this);
	popupMenu.addAction(QString("Run \"%1\"").arg(description),	[this, machine, &software]() { Run(machine, std::move(software));	});
	popupMenu.addAction("Create profile",						[this, machine, &software]() { CreateProfile(machine, software);	});
	popupMenu.exec(pos);
}


//-------------------------------------------------
//  GetUniqueFileName
//-------------------------------------------------

template<typename TFunc>
static QString GetUniqueProfilePath(const QString &dir_path, TFunc generate_name)
{
	// prepare for building file paths
	QString file_path;

	// try up to 10 attempts
	for (int attempt = 0; file_path.isEmpty() && attempt < 10; attempt++)
	{
		// build the file path
		QString name = generate_name(attempt);
		QString path = dir_path + "/" + name + ".bletchmameprofile";
		QFileInfo fi(path);

		// get the file path if this file doesn't exist
		if (!fi.exists())
			file_path = fi.absoluteFilePath();
	}
	return file_path;
}


//-------------------------------------------------
//  CreateProfile
//-------------------------------------------------

void MainWindow::CreateProfile(const info::machine &machine, const software_list::software *software)
{
	// find a path to create the new profile in
	QStringList paths = m_prefs.GetSplitPaths(Preferences::global_path_type::PROFILES);
	auto iter = std::find_if(paths.begin(), paths.end(), [](const QString &path)
	{
		QDir dir(path);
		return dir.exists();
	});
	if (iter == paths.end())
	{
		iter = std::find_if(paths.begin(), paths.end(), DirExistsOrMake);
		m_profileListItemModel->refresh(false, true);
	}
	if (iter == paths.end())
	{
		messageBox("Cannot create profile without valid profile path");
		return;
	}
	const QString &path = *iter;

	// identify the description, for use when we create the file name
	const QString &description = software
		? software->m_description
		: machine.name();

	// get the full path for a new profile
	QString new_profile_path = GetUniqueProfilePath(path, [&description](int attempt)
	{
		return attempt == 0
			? QString("%1 - New profile").arg(description)
			: QString("%1 - New profile (%2)").arg(description, QString::number(attempt + 1));
	});

	// create the file stream
	QFile file(new_profile_path);
	if (!file.open(QIODevice::WriteOnly))
	{
		messageBox("Could not create profile");
		return;
	}

	// create the new profile and focus
	QTextStream text_stream(&file);
	profiles::profile::create(text_stream, machine, software);
	FocusOnNewProfile(std::move(new_profile_path));
}


//-------------------------------------------------
//  DirExistsOrMake
//-------------------------------------------------

bool MainWindow::DirExistsOrMake(const QString &path)
{
	bool result = QDir(path).exists();
	if (!result)
		result = QDir().mkdir(path);
	return result;
}


//-------------------------------------------------
//  DuplicateProfile
//-------------------------------------------------

void MainWindow::DuplicateProfile(const profiles::profile &profile)
{
	QString dir_path = profile.directory_path();

	// get the full path for a new profile
	QString new_profile_path = GetUniqueProfilePath(dir_path, [&profile](int attempt)
	{
		return attempt == 0
			? QString("%1 - Copy").arg(profile.name())
			: QString("%1 - Copy (%2)").arg(profile.name(), QString::number(attempt + 1));
	});

	// create the file stream
	QFile file(new_profile_path);
	if (!file.open(QIODevice::WriteOnly))
	{
		messageBox("Could not create profile");
		return;
	}

	// create the new profile
	QTextStream stream(&file);
	profile.save_as(stream);

	// copy the save state file also, if present
	QString old_save_state_file = profiles::profile::change_path_save_state(profile.path());
	if (QFile(old_save_state_file).exists())
	{
		QString new_save_state_file = profiles::profile::change_path_save_state(new_profile_path);
		QFile::copy(old_save_state_file, new_save_state_file);
	}

	// finally focus
	FocusOnNewProfile(std::move(new_profile_path));
}


//-------------------------------------------------
//  RenameProfile
//-------------------------------------------------

void MainWindow::RenameProfile(const profiles::profile &profile)
{
	throw false;
}


//-------------------------------------------------
//  DeleteProfile
//-------------------------------------------------

void MainWindow::DeleteProfile(const profiles::profile &profile)
{
	QString message = QString("Are you sure you want to delete profile \"%1\"").arg(profile.name());
	if (messageBox(message, QMessageBox::Yes | QMessageBox::No) != QMessageBox::StandardButton::Yes)
		return;

	if (!profiles::profile::profile_file_remove(profile.path()))
		messageBox("Could not delete profile");
}


//-------------------------------------------------
//  FocusOnNewProfile
//-------------------------------------------------

void MainWindow::FocusOnNewProfile(QString &&new_profile_path)
{
	// set the profiles tab as selected
	m_ui->tabWidget->setCurrentWidget(m_ui->profilesTab);

	// set the profile as selected, so we focus on it when we rebuild the list view
	m_prefs.SetListViewSelection(s_profileListTableViewDesc.m_name, std::move(new_profile_path));

	// we want the current profile to be renamed - do this with a callback
	m_profileListItemModel->setOneTimeFswCallback([this, profilePath{std::move(new_profile_path)}]()
	{
		QModelIndex actualIndex = m_profileListItemModel->findProfileIndex(profilePath);
		if (!actualIndex.isValid())
			return;

		QModelIndex index = sortFilterProxyModel(*m_ui->profilesTableView).mapFromSource(index);
		if (!index.isValid())
			return;

		m_ui->profilesTableView->edit(actualIndex);
	});
}


//-------------------------------------------------
//  showInGraphicalShell
//-------------------------------------------------

void MainWindow::showInGraphicalShell(const QString &path) const
{
	// Windows-only code here; we really should eventually be doing something like:
	// https://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
	QFileInfo fi(path);
	QStringList args;
	if (!fi.isDir())
		args += QLatin1String("/select,");
	args << QDir::toNativeSeparators(fi.canonicalFilePath());
	QProcess::startDetached("explorer.exe", args);
}


//-------------------------------------------------
//  machineFromModelIndex
//-------------------------------------------------

info::machine MainWindow::machineFromModelIndex(const QModelIndex &index) const
{
	// map the index to the actual index
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->machinesTableView).mapToSource(index);

	// and look up in the info DB
	return m_info_db.machines()[actualIndex.row()];
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
	const QString &machineDesc = m_client.GetCurrentTask<RunMachineTask>()->getMachine().description();

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
	if (AttachToRootPanel() && m_client.GetCurrentTask<RunMachineTask>())
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
//  Issue
//-------------------------------------------------

void MainWindow::Issue(const std::vector<QString> &args)
{
	std::shared_ptr<RunMachineTask> task = m_client.GetCurrentTask<RunMachineTask>();
	if (!task)
		return;

	task->issue(args);
}


void MainWindow::Issue(const std::initializer_list<std::string> &args)
{
	std::vector<QString> qargs;
	qargs.reserve(args.size());

	for (const auto &arg : args)
	{
		QString qarg = QString::fromStdString(arg);
		qargs.push_back(std::move(qarg));
	}

	Issue(qargs);
}


void MainWindow::Issue(const char *command)
{
	QString command_string = command;
	Issue({ command_string });
}


//-------------------------------------------------
//  InvokePing
//-------------------------------------------------

void MainWindow::InvokePing()
{
	// only issue a ping if there is an active session, and there is no ping in flight
	if (!m_pinging && m_state.has_value())
	{
		m_pinging = true;
		Issue("ping");
	}
}


//-------------------------------------------------
//  InvokeExit
//-------------------------------------------------

void MainWindow::InvokeExit()
{
	if (m_current_profile_auto_save_state)
	{
		QString save_state_path = profiles::profile::change_path_save_state(m_current_profile_path);
		Issue({ "state_save_and_exit", QDir::toNativeSeparators(save_state_path) });
	}
	else
	{
		Issue({ "exit" });
	}
}


//-------------------------------------------------
//  ChangePaused
//-------------------------------------------------

void MainWindow::ChangePaused(bool paused)
{
	Issue(paused ? "pause" : "resume");
}


//-------------------------------------------------
//  ChangeThrottled
//-------------------------------------------------

void MainWindow::ChangeThrottled(bool throttled)
{
	Issue({ "throttled", std::to_string(throttled ? 1 : 0) });
}


//-------------------------------------------------
//  ChangeThrottleRate
//-------------------------------------------------

void MainWindow::ChangeThrottleRate(float throttle_rate)
{
	Issue({ "throttle_rate", std::to_string(throttle_rate) });
}


//-------------------------------------------------
//  ChangeThrottleRate
//-------------------------------------------------

void MainWindow::ChangeThrottleRate(int adjustment)
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
	ChangeThrottleRate(s_throttle_rates[index]);
}


//-------------------------------------------------
//  ChangeSound
//-------------------------------------------------

void MainWindow::ChangeSound(bool sound_enabled)
{
	Issue({ "set_attenuation", std::to_string(sound_enabled ? SOUND_ATTENUATION_ON : SOUND_ATTENUATION_OFF) });
}


//-------------------------------------------------
//  sortFilterProxyModel
//-------------------------------------------------

const QSortFilterProxyModel &MainWindow::sortFilterProxyModel(const QTableView &tableView) const
{
	assert(&tableView == m_ui->machinesTableView || &tableView == m_ui->softwareTableView || &tableView == m_ui->profilesTableView);
	return *dynamic_cast<QSortFilterProxyModel *>(tableView.model());
}
