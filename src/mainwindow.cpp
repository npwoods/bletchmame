/***************************************************************************

	mainwindow.cpp

	Main BletchMAME window

***************************************************************************/

#include <QThread>
#include <QMessageBox>
#include <QStringListModel>

#include "mainwindow.h"
#include "mameversion.h"
#include "ui_mainwindow.h"
#include "collectionviewmodel.h"
#include "versiontask.h"
#include "utility.h"
#include "dialogs/about.h"


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
{
	// set up Qt form
	m_ui = std::make_unique<Ui::MainWindow>();
	m_ui->setupUi(this);

	// initial preferences read
	m_prefs.Load();

	// set up machines view
	CollectionViewModel &machinesViewModel = *new CollectionViewModel(
		this,
		m_prefs,
		s_machine_collection_view_desc,
		[this](long item, long column) -> const QString &{ return GetMachineListItemText(m_info_db.machines()[item], column); },
		[this]() { return m_info_db.machines().size(); },
		false);
	m_info_db.set_on_changed([&machinesViewModel]{ machinesViewModel.updateListView(); });
	m_ui->machinesTableView->setModel(&machinesViewModel);

	// set up machines search box
	setupSearchBox(*m_ui->machinesSearchBox, "machine", machinesViewModel);

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
//  on_actionAbout_triggered
//-------------------------------------------------

void MainWindow::on_actionAbout_triggered()
{
	AboutDialog dlg;
	dlg.exec();
}


//-------------------------------------------------
//  on_actionExit_triggered
//-------------------------------------------------

void MainWindow::on_actionExit_triggered()
{
	close();
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
			RefreshMameInfoDatabase();
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
	m_client.Launch(create_version_task());
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
#if 1
	throw std::logic_error("NYI");
#else
	QString path = show_specify_single_path_dialog(*this, Preferences::global_path_type::EMU_EXECUTABLE, m_prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE));
	if (path.empty())
		return false;

	m_prefs.SetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE, std::move(path));
	return true;
#endif
}


//-------------------------------------------------
//  RefreshMameInfoDatabase
//-------------------------------------------------

bool MainWindow::RefreshMameInfoDatabase()
{
	// sanity check; bail if we can't find the executable
	if (!IsMameExecutablePresent())
		return false;

#if 1
	throw std::logic_error("NYI");
#else
	// list XML
	QString db_path = m_prefs.GetMameXmlDatabasePath();
	m_client.Launch(create_list_xml_task(QString(db_path)));

	// and show the dialog
	if (!show_loading_mame_info_dialog(*this, [this]() { return !m_client.IsTaskActive(); }))
	{
		m_client.Abort();
		return false;
	}

	// we've succeeded; load the DB
	if (!m_info_db.load(db_path))
	{
		// a failure here is likely due to a very strange condition (e.g. - someone deleting the infodb
		// file out from under me)
		return false;
	}

	return true;
#endif
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
		QString message = QString::asprintf(
			"This version of MAME doesn't seem to be supported; BletchMAME requires MAME %d.%d or newer to function correctly",
			REQUIRED_MAME_VERSION.Major(),
			REQUIRED_MAME_VERSION.Minor());
		messageBox(message);
	}

	m_client.Reset();
	return true;
}


//-------------------------------------------------
//  setupSearchBox
//-------------------------------------------------

void MainWindow::setupSearchBox(QLineEdit &lineEdit, const char *collection_view_desc_name, CollectionViewModel &collectionViewModel)
{
	const QString &text = m_prefs.GetSearchBoxText(collection_view_desc_name);
	lineEdit.setText(text);

	auto callback = [&collectionViewModel, collection_view_desc_name, this]()
	{
		QString text = m_ui->machinesSearchBox->text();
		m_prefs.SetSearchBoxText(collection_view_desc_name, std::move(text));
		collectionViewModel.updateListView();
	};
	connect(&lineEdit, &QLineEdit::textEdited, this, callback);
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




