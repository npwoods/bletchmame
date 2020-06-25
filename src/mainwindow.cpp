/***************************************************************************

	mainwindow.cpp

	Main BletchMAME window

***************************************************************************/

#include <QThread>
#include <QMessageBox>
#include <QTimer>
#include <QStringListModel>
#include <QDesktopServices>
#include <QUrl>

#include "mainwindow.h"
#include "mameversion.h"
#include "ui_mainwindow.h"
#include "collectionviewmodel.h"
#include "softlistviewmodel.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "versiontask.h"
#include "utility.h"
#include "dialogs/about.h"
#include "dialogs/loading.h"
#include "dialogs/paths.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// BletchMAME requires MAME 0.213 or later
const MameVersion REQUIRED_MAME_VERSION = MameVersion(0, 213, false);


//**************************************************************************
//  VERSION INFO
//**************************************************************************

#ifdef _MSC_VER
// we're not supporing build numbers for MSVC builds
static const char build_version[] = "MSVC";
static const char build_revision[] = "MSVC";
static const char build_date_time[] = "MSVC";
#else
extern const char build_version[];
extern const char build_revision[];
extern const char build_date_time[];
#endif


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

static const CollectionViewDesc s_machine_collection_view_desc =
{
	"machine",
	"name",
	{
		{ "name",			"Name",			85 },
		{ "description",	"Description",		370 },
		{ "year",			"Year",			50 },
		{ "manufacturer",	"Manufacturer",	320 }
	}
};


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_client(*this, m_prefs)
	, m_machinesViewModel(nullptr)
	, m_softwareListViewModel(nullptr)
	, m_pingTimer(nullptr)
	, m_pinging(false)
	, m_current_pauser(nullptr)
{
	// set up Qt form
	m_ui = std::make_unique<Ui::MainWindow>();
	m_ui->setupUi(this);

	// initial preferences read
	m_prefs.Load();

	// set up machines view
	m_machinesViewModel = new CollectionViewModel(
		*m_ui->machinesTableView,
		m_prefs,
		s_machine_collection_view_desc,
		[this](long item, long column) -> const QString &{ return GetMachineListItemText(m_info_db.machines()[item], column); },
		[this]() { return m_info_db.machines().size(); },
		false);
	m_info_db.set_on_changed([this]{ m_machinesViewModel->updateListView(); });

	// set up machines search box
	setupSearchBox(*m_ui->machinesSearchBox, "machine", *m_machinesViewModel);

	// set up software list view
	m_softwareListViewModel = new SoftwareListViewModel(
		*m_ui->softwareTableView,
		m_prefs);

	// set up software list search box
	setupSearchBox(*m_ui->softwareSearchBox, SOFTLIST_VIEW_DESC_NAME, *m_softwareListViewModel);

	// set up the tab widget
	m_ui->tabWidget->setCurrentIndex(static_cast<int>(m_prefs.GetSelectedTab()));

	// set up the ping timer
	m_pingTimer = new QTimer(this);
	connect(m_pingTimer, &QTimer::timeout, this, &MainWindow::InvokePing);

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
//  on_actionExit_triggered
//-------------------------------------------------

void MainWindow::on_actionExit_triggered()
{
	close();
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

#if 0
	// did the user change the profiles path?
	if (is_changed(Preferences::global_path_type::PROFILES))
		UpdateProfileDirectories(true, true);

	// did the user change the icons path?
	if (is_changed(Preferences::global_path_type::ICONS))
	{
		m_icon_loader.RefreshIcons();
		if (m_machine_view->GetItemCount() > 0)
			m_machine_view->RefreshItems(0, m_machine_view->GetItemCount() - 1);
	}
#endif
}


//-------------------------------------------------
//  on_actionAbout_triggered
//-------------------------------------------------

void MainWindow::on_actionAbout_triggered()
{
	AboutDialog dlg;
	dlg.exec();
}


//-------------------------------------------------
//  on_actionRefresh_Machine_Info_triggered
//-------------------------------------------------

void MainWindow::on_actionRefresh_Machine_Info_triggered()
{
	refreshMameInfoDatabase();
}


//-------------------------------------------------
//  on_actionBletchMAME_web_site_triggered
//-------------------------------------------------

void MainWindow::on_actionBletchMAME_web_site_triggered()
{
	QDesktopServices::openUrl(QUrl("https://www.bletchmame.org/"));
}


//-------------------------------------------------
//  on_machinesTableView_activated
//-------------------------------------------------

void MainWindow::on_machinesTableView_activated(const QModelIndex &index)
{
	const info::machine machine = GetMachineFromIndex(index.row());
	Run(machine);
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
		m_software_list_collection_machine_name.clear();
		updateSoftwareList();
		break;
	}
}


//-------------------------------------------------
//  event
//-------------------------------------------------

bool MainWindow::event(QEvent *event)
{
	bool result;
	if (event->type() == VersionResultEvent::eventId())
	{
		result = onVersionCompleted(static_cast<VersionResultEvent &>(*event));
	}
	else if (event->type() == ListXmlResultEvent::eventId())
	{
		result = onListXmlCompleted(static_cast<ListXmlResultEvent &>(*event));
	}
	else
	{
		result = QMainWindow::event(event);
	}
	return result;
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
	m_client.launch(create_list_xml_task(std::move(db_path)));

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
//  AttachToRootPanel
//-------------------------------------------------

bool MainWindow::AttachToRootPanel() const
{
	// Targetting subwindows with -attach_window was introduced in between MAME 0.217 and MAME 0.218
	const MameVersion REQUIRED_MAME_VERSION_ATTACH_TO_CHILD_WINDOW = MameVersion(0, 217, true);

	// Are we the required version?
	return isMameVersionAtLeast(REQUIRED_MAME_VERSION_ATTACH_TO_CHILD_WINDOW);
}


//-------------------------------------------------
//  Run
//-------------------------------------------------

void MainWindow::Run(const info::machine &machine, const software_list::software *software, void *profile)
{
	// run a "preflight check" on MAME, to catch obvious problems that might not be caught or reported well
	QString preflight_errors = PreflightCheck();
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
#if 0
	else if (profile && profile->images().empty())
		software_name = profile->software();
#endif

	// we need to have full information to support the emulation session; retrieve
	// fake a pauser to forestall "PAUSED" from appearing in the menu bar
	Pauser fake_pauser(*this, false);

	// run the emulation
	Task::ptr task = std::make_shared<RunMachineTask>(
		machine,
		std::move(software_name),
		AttachToRootPanel() ? *m_ui->centralwidget : *this);
	m_client.launch(std::move(task));

	// set up running state and subscribe to events
	m_state.emplace();
#if 0
	m_state->paused().subscribe([this]() { UpdateTitleBar(); });
	m_state->phase().subscribe([this]() { UpdateStatusBar(); });
	m_state->speed_percent().subscribe([this]() { UpdateStatusBar(); });
	m_state->effective_frameskip().subscribe([this]() { UpdateStatusBar(); });
	m_state->startup_text().subscribe([this]() { UpdateStatusBar(); });
	m_state->images().subscribe([this]() { UpdateStatusBar(); });
#endif

	// mouse capturing is a bit more involved
#if 0
	m_capture_mouse = observable::observe(m_state->has_input_using_mouse() && !m_menu_bar_shown);
	m_capture_mouse.subscribe([this]()
		{
			Issue({ "SET_MOUSE_ENABLED", m_capture_mouse ? "true" : "false" });
			if (m_capture_mouse)
				SetCursor(wxCursor(wxCURSOR_BLANK));
			else
				SetCursor(wxNullCursor);
		});
#endif

	// we have a session running; hide/show things respectively
	UpdateEmulationSession();

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
#if 0
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
			if (wxFile::Exists(save_state_path))
				Issue({ "state_load", save_state_path });
		}
	}
#endif

	// do we have any images that require images?
	auto iter = std::find_if(m_state->images().get().cbegin(), m_state->images().get().cend(), [](const status::image &image)
	{
		return image.m_must_be_loaded && image.m_file_name.isEmpty();
	});
	if (iter != m_state->images().get().cend())
	{
		throw std::logic_error("NYI");
#if 0
		// if so, show the dialog
		ImagesHost images_host(*this);
		if (!show_images_dialog_cancellable(images_host))
		{
			Issue("exit");
			return;
		}
#endif
	}

	// unpause
	ChangePaused(false);
}


//-------------------------------------------------
//  PreflightCheck - run checks on MAME to catch
//	obvious problems
//-------------------------------------------------

QString MainWindow::PreflightCheck()
{
#if 0
	// get a list of the plugin paths, checking for the obvious problem where there are no paths
	std::vector<QString> paths = m_prefs.GetSplitPaths(Preferences::global_path_type::PLUGINS);
	if (paths.empty())
		return QString::Format("No plug-in paths are specified.  Under these circumstances, the required \"%s\" plug-in cannot be loaded.", WORKER_UI_PLUGIN_NAME);

	// apply substitutions and normalize the paths
	QString path_separator = wxFileName::GetPathSeparator();
	for (QString &path : paths)
	{
		path = m_prefs.ApplySubstitutions(path);
		if (!path.EndsWith(path_separator))
			path += path_separator;
	}

	// check to see if worker_ui exists
	QString worker_ui_subpath = QString(WORKER_UI_PLUGIN_NAME) + path_separator;
	bool worker_ui_exists = util::find_if_ptr(paths, [&worker_ui_subpath](const QString &path)
		{
			return wxFile::Exists(path + worker_ui_subpath + wxT("init.lua"))
				&& wxFile::Exists(path + worker_ui_subpath + wxT("plugin.json"));
		});

	// if worker_ui doesn't exist, report an error message
	if (!worker_ui_exists)
	{
		QString message = QString::Format("Could not find the %s plug in in the following directories:\n\n", WORKER_UI_PLUGIN_NAME);
		for (const QString &path : paths)
		{
			message += path;
			message += wxT("\n");
		}
		return message;
	}
#endif

	// success!
	return QString();
}


//-------------------------------------------------
//  messageBox
//-------------------------------------------------

int MainWindow::messageBox(const QString &message, long style, const QString &caption)
{
	Pauser pauser(*this);

	QMessageBox msgBox;
	msgBox.setText(message);
	msgBox.exec();
	return 0;
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
//  setupSearchBox
//-------------------------------------------------

void MainWindow::setupSearchBox(QLineEdit &lineEdit, const char *collection_view_desc_name, CollectionViewModel &collectionViewModel)
{
	const QString &text = m_prefs.GetSearchBoxText(collection_view_desc_name);
	lineEdit.setText(text);

	auto callback = [&collectionViewModel, &lineEdit, collection_view_desc_name, this]()
	{
		QString text = lineEdit.text();
		m_prefs.SetSearchBoxText(collection_view_desc_name, std::move(text));
		collectionViewModel.updateListView();
	};
	connect(&lineEdit, &QLineEdit::textEdited, this, callback);
}


//-------------------------------------------------
//  updateSoftwareList
//-------------------------------------------------

void MainWindow::updateSoftwareList()
{
	long selected = m_machinesViewModel->getFirstSelected();
	if (selected >= 0)
	{
		int actual_selected = m_machinesViewModel->getActualIndex(selected);
		info::machine machine = m_info_db.machines()[actual_selected];
		if (machine.name() != m_software_list_collection_machine_name)
		{
			m_software_list_collection.load(m_prefs, machine);
			m_software_list_collection_machine_name = machine.name();
		}
		m_softwareListViewModel->Load(m_software_list_collection, false);
	}
	else
	{
		m_softwareListViewModel->Clear();
	}
	m_softwareListViewModel->updateListView();
}


//-------------------------------------------------
//  GetMachineFromIndex
//-------------------------------------------------

info::machine MainWindow::GetMachineFromIndex(long item) const
{
	// look up the indirection
	int machine_index = m_machinesViewModel->getActualIndex(item);

	// and look up in the info DB
	return m_info_db.machines()[machine_index];
}


//-------------------------------------------------
//  GetMachineListItemText
//-------------------------------------------------

const QString &MainWindow::GetMachineListItemText(info::machine machine, long column) const
{
	switch (column)
	{
	case 0:	return machine.name();
	case 1:	return machine.description();
	case 2:	return machine.year();
	case 3:	return machine.manufacturer();
	}
	throw false;
}


//-------------------------------------------------
//  UpdateEmulationSession
//-------------------------------------------------

void MainWindow::UpdateEmulationSession()
{
	// is the emulation session active?
	bool is_active = m_state.has_value();

	// if so, hide the machine list UX
	m_ui->tabWidget->setVisible(!is_active);
	m_ui->centralwidget->setVisible(!is_active || AttachToRootPanel());

	// ...and enable pinging
	if (is_active)
		m_pingTimer->start(500);
	else
		m_pingTimer->stop();

	// ...and cascade other updates
	UpdateTitleBar();
	UpdateMenuBar();
}


//-------------------------------------------------
//  UpdateTitleBar
//-------------------------------------------------

void MainWindow::UpdateTitleBar()
{
	QString title_text = QCoreApplication::applicationName();
	if (m_state.has_value())
	{
		title_text += ": " + m_client.GetCurrentTask<RunMachineTask>()->getMachine().description();

		// we want to append "PAUSED" if and only if the user paused, not as a consequence of a menu
		if (m_state->paused().get() && !m_current_pauser)
			title_text += " PAUSED";
	}
	setWindowTitle(title_text);
}


//-------------------------------------------------
//  UpdateMenuBar
//-------------------------------------------------

void MainWindow::UpdateMenuBar()
{
#if 0
	// are we supposed to show the menu bar?
	m_menu_bar_shown = !m_state.has_value() || m_prefs.GetMenuBarShown();

	// is this different than the current state?
	if (m_menu_bar_shown.get() != WindowHasMenuBar(*this))
	{
		// when we hide the menu bar, we disable the accelerators
		m_menu_bar->SetAcceleratorTable(m_menu_bar_shown ? m_menu_bar_accelerators : wxAcceleratorTable());

#ifdef WIN32
		// Win32 specific code
		SetMenu(GetHWND(), m_menu_bar_shown ? m_menu_bar->GetHMenu() : nullptr);
#else
		throw false;
#endif
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


void MainWindow::Issue(const std::initializer_list<QString> &args)
{
	Issue(std::vector<QString>(args));
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
#if 0
	if (m_current_profile_auto_save_state)
	{
		QString save_state_path = profiles::profile::change_path_save_state(m_current_profile_path);
		Issue({ "state_save_and_exit", save_state_path });
	}
	else
#endif
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


//**************************************************************************
//  PAUSER
//**************************************************************************

//-------------------------------------------------
//  Pauser ctor
//-------------------------------------------------

MainWindow::Pauser::Pauser(MainWindow &host, bool actually_pause)
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


//-------------------------------------------------
//  Pauser dtor
//-------------------------------------------------

MainWindow::Pauser::~Pauser()
{
	if (m_is_running)
		m_host.ChangePaused(false);
	m_host.m_current_pauser = m_last_pauser;
}
