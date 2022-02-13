/***************************************************************************

	mainpanel.cpp

	Main BletchMAME panel

***************************************************************************/

// bletchmame headers
#include "assetfinder.h"
#include "auditcursor.h"
#include "audittask.h"
#include "machinefoldertreemodel.h"
#include "machinelistitemmodel.h"
#include "mainpanel.h"
#include "prefs.h"
#include "profilelistitemmodel.h"
#include "sessionbehavior.h"
#include "softwarelistitemmodel.h"
#include "splitterviewtoggler.h"
#include "tableviewmanager.h"
#include "utility.h"
#include "ui_mainpanel.h"
#include "dialogs/audit.h"
#include "dialogs/choosesw.h"
#include "dialogs/newcustomfolder.h"

// Qt headers
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSortFilterProxyModel>

// standard headers
#include <unordered_set>


//**************************************************************************
//  TYPES
//**************************************************************************

// ======================> SnapshotViewEventFilter 

class MainPanel::SnapshotViewEventFilter : public QObject
{
public:
	SnapshotViewEventFilter(MainPanel &host)
		: m_host(host)
	{
	}

protected:
	virtual bool eventFilter(QObject *obj, QEvent *event) override
	{
		bool result = QObject::eventFilter(obj, event);
		if (event->type() == QEvent::Resize)
			m_host.updateSnapshot();
		return result;
	}

private:
	MainPanel &m_host;
};


//**************************************************************************
//  CONSTANTS
//**************************************************************************

static const TableViewManager::ColumnDesc s_machineListTableViewColumns[] =
{
	{ u8"name",			85 },
	{ u8"description",	370 },
	{ u8"year",			50 },
	{ u8"manufacturer",	320 },
	{ nullptr }
};

static const TableViewManager::Description s_machineListTableViewDesc =
{
	u8"machine",
	(int)MachineListItemModel::Column::Machine,
	s_machineListTableViewColumns
};

static const TableViewManager::ColumnDesc s_profileListTableViewColumns[] =
{
	{ u8"name",			85 },
	{ u8"machine",		85 },
	{ u8"path",			600 },
	{ nullptr }
};

static const TableViewManager::Description s_profileListTableViewDesc =
{
	u8"profile",
	(int)ProfileListItemModel::Column::Path,
	s_profileListTableViewColumns
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainPanel::MainPanel(info::database &infoDb, Preferences &prefs, IMainPanelHost &host, QWidget *parent)
	: QWidget(parent)
	, m_prefs(prefs)
	, m_host(host)
	, m_infoDb(infoDb)
	, m_iconLoader(prefs)
{
	// set up Qt form
	m_ui = std::make_unique<Ui::MainPanel>();
	m_ui->setupUi(this);

	// load preferences and refresh icons
	m_prefs.load();
	m_iconLoader.refreshIcons();

	// set up machines view
	MachineListItemModel &machineListItemModel = *new MachineListItemModel(
		this,
		m_infoDb,
		&m_iconLoader,
		[this](info::machine machine) { m_host.auditIfAppropriate(machine); });
	TableViewManager::setup(
		*m_ui->machinesTableView,
		machineListItemModel,
		m_ui->machinesSearchBox,
		m_prefs,
		s_machineListTableViewDesc,
		[this](const QString &machineName) { updateInfoPanel(machineName); });
	connect(m_ui->machinesTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &newSelection, const QItemSelection &oldSelection)
	{
		updateStatusFromSelection();
	});

	// set up machine folder tree
	MachineFolderTreeModel &machineFolderTreeModel = *new MachineFolderTreeModel(this, m_infoDb, m_prefs);
	m_ui->machinesFolderTreeView->setModel(&machineFolderTreeModel);
	connect(m_ui->machinesFolderTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &newSelection, const QItemSelection &oldSelection)
	{
		machineFoldersTreeViewSelectionChanged(newSelection, oldSelection);
	});
	connect(&machineFolderTreeModel, &QAbstractItemModel::modelAboutToBeReset, this, [this]()
	{
		identifyExpandedFolderTreeItems();
	});
	connect(&machineFolderTreeModel, &QAbstractItemModel::modelReset, this, [this, &machineFolderTreeModel]()
	{
		const QString &selectionPath = m_prefs.getMachineFolderTreeSelection();
		QModelIndex selectionIndex = machineFolderTreeModel.modelIndexFromPath(!selectionPath.isEmpty() ? selectionPath : "all");
		m_ui->machinesFolderTreeView->selectionModel()->select(selectionIndex, QItemSelectionModel::Select);
		m_ui->machinesFolderTreeView->scrollTo(selectionIndex);

		// reexpand any items that may have changed
		for (const QString &path : m_expandedTreeItems)
		{
			QModelIndex index = machineFolderTreeModel.modelIndexFromPath(path);
			m_ui->machinesFolderTreeView->setExpanded(index, true);
		}
	});
	connect(&m_prefs, &Preferences::folderPrefsChanged,		this, [&machineFolderTreeModel]() { machineFolderTreeModel.refresh(); });
	connect(&m_prefs, &Preferences::customFoldersChanged,	this, [&machineFolderTreeModel]() { machineFolderTreeModel.refresh(); });

	// set up software list view
	SoftwareListItemModel &softwareListItemModel = *new SoftwareListItemModel(
		&m_iconLoader,
		[this](const software_list::software &software) { m_host.auditIfAppropriate(software); },
		this);
	TableViewManager::setup(
		*m_ui->softwareTableView,
		softwareListItemModel,
		m_ui->softwareSearchBox,
		m_prefs,
		ChooseSoftlistPartDialog::s_tableViewDesc);
	connect(m_ui->softwareTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &newSelection, const QItemSelection &oldSelection)
	{
		updateStatusFromSelection();
	});

	// set up the profile list view
	ProfileListItemModel &profileListItemModel = *new ProfileListItemModel(this, m_prefs, m_infoDb, m_iconLoader);
	TableViewManager::setup(
		*m_ui->profilesTableView,
		profileListItemModel,
		nullptr,
		m_prefs,
		s_profileListTableViewDesc);
	connect(m_ui->profilesTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &newSelection, const QItemSelection &oldSelection)
	{
		updateStatusFromSelection();
	});
	profileListItemModel.refresh(true, true);

	// set up the tab widget
	m_ui->tabWidget->setCurrentIndex(static_cast<int>(m_prefs.getSelectedTab()));

	// set up machine splitters
	const QList<int> &machineSplitterSizes = m_prefs.getMachineSplitterSizes();
	if (!machineSplitterSizes.isEmpty())
		m_ui->machinesSplitter->setSizes(machineSplitterSizes);

	// set up splitter togglers for the machines view
	(void)new SplitterViewToggler(this, *m_ui->machinesFoldersToggleButton, *m_ui->machinesSplitter, 0, 1, [this]() { persistMachineSplitterSizes(); });
	(void)new SplitterViewToggler(this, *m_ui->machinesInfoToggleButton, *m_ui->machinesSplitter, 2, 1, [this]() { persistMachineSplitterSizes(); });

	// set up a resize event filter to resize snapshot imagery
	QObject &eventFilter = *new SnapshotViewEventFilter(*this);
	m_ui->machinesSnapLabel->installEventFilter(&eventFilter);

	// monitor prefs changes
	connect(&m_prefs, &Preferences::globalPathRomsChanged, this, [this](const QString &newPath)
	{
		machineAuditStatusesChanged();
		softwareAuditStatusesChanged();
	});
	connect(&m_prefs, &Preferences::globalPathSamplesChanged, this, [this](const QString &newPath)
	{
		machineAuditStatusesChanged();
	});
	connect(&m_prefs, &Preferences::globalPathIconsChanged, this, [this](const QString &newPath)
	{
		m_iconLoader.refreshIcons();
	});
	connect(&m_prefs, &Preferences::globalPathSnapshotsChanged, this, [this](const QString &newPath)
	{
		std::optional<info::machine> selectedMachine = currentlySelectedMachine();
		QString machineName = selectedMachine ? selectedMachine->name() : QString();
		updateInfoPanel(machineName);
	});

	// update the tab contents
	updateTabContents();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MainPanel::~MainPanel()
{
}


//-------------------------------------------------
//  currentAuditableListItemModel
//-------------------------------------------------

AuditableListItemModel *MainPanel::currentAuditableListItemModel()
{
	AuditableListItemModel *result;
	switch (m_prefs.getSelectedTab())
	{
	case Preferences::list_view_type::MACHINE:
		result = &machineListItemModel();
		break;
	case Preferences::list_view_type::SOFTWARELIST:
		result = &softwareListItemModel();
		break;
	default:
		result = nullptr;
		break;
	}
	return result;
}


//-------------------------------------------------
//  currentlySelectedMachine
//-------------------------------------------------

std::optional<info::machine> MainPanel::currentlySelectedMachine() const
{
	std::optional<info::machine> result;
	QModelIndexList selection = m_ui->machinesTableView->selectionModel()->selectedIndexes();
	if (selection.size() > 0)
		result = machineFromModelIndex(selection[0]);
	return result;
}


//-------------------------------------------------
//  currentlySelectedSoftware
//-------------------------------------------------

const software_list::software *MainPanel::currentlySelectedSoftware() const
{
	const software_list::software *result = nullptr;
	QModelIndexList selection = m_ui->softwareTableView->selectionModel()->selectedIndexes();
	if (selection.size() > 0)
		result = &softwareFromModelIndex(selection[0]);
	return result;
}


//-------------------------------------------------
//  currentlySelectedProfile
//-------------------------------------------------

std::shared_ptr<profiles::profile> MainPanel::currentlySelectedProfile()
{
	std::shared_ptr<profiles::profile> result;

	QModelIndexList selection = m_ui->profilesTableView->selectionModel()->selectedIndexes();
	if (selection.count() > 0)
	{
		QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapToSource(selection[0]);
		result = profileListItemModel().getProfileByIndex(actualIndex.row());
	}
	return result;
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void MainPanel::run(const info::machine &machine, const software_list::software *software)
{
	std::unique_ptr<SessionBehavior> sessionBehavior = std::make_unique<NormalSessionBehavior>(software);
	run(machine, std::move(sessionBehavior));
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void MainPanel::run(std::shared_ptr<profiles::profile> &&profile)
{
	// find the machine
	std::optional<info::machine> machine = m_infoDb.find_machine(profile->machine());
	if (!machine)
	{
		QMessageBox::critical(this, QApplication::applicationDisplayName(), "Unknown machine: " + profile->machine());
		return;
	}

	run(machine.value(), std::make_unique<ProfileSessionBehavior>(std::move(profile)));
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void MainPanel::run(const info::machine &machine, std::unique_ptr<SessionBehavior> &&sessionBehavior)
{
	m_host.run(machine, std::move(sessionBehavior));
}


//-------------------------------------------------
//  launchingListContextMenu
//-------------------------------------------------

void MainPanel::launchingListContextMenu(const QPoint &pos, const software_list::software *software)
{
	// identify the machine
	QModelIndex index = m_ui->machinesTableView->selectionModel()->selectedIndexes()[0];
	const info::machine machine = machineFromModelIndex(index);

	// identify the description
	const QString &description = software
		? software->description()
		: machine.description();

	// build the menu
	QMenu popupMenu(this);
	popupMenu.addAction(QString("Run \"%1\"").arg(description), [this, machine, &software]() { run(machine, std::move(software));	});
	popupMenu.addAction("Create profile", [this, machine, &software]() { createProfile(machine, software);	});

	// build the custom folder menu
	QMenu &customFolderMenu = *popupMenu.addMenu("Add to custom folder");
	const std::map<QString, std::set<QString>> &customFolders = m_prefs.getCustomFolders();
	for (auto &pair : customFolders)
	{
		// create the custom folder menu item
		const QString &customFolderName = pair.first;
		const std::set<QString> &customFolderMachines = pair.second;
		QAction &action = *customFolderMenu.addAction(customFolderName, [this, &customFolderName, &machine]()
		{
			m_prefs.addMachineToCustomFolder(customFolderName, machine.name());
		});

		// disable and check it if its already in the folder
		bool alreadyInFolder = util::contains(customFolderMachines, machine.name());
		action.setEnabled(!alreadyInFolder);
	}
	if (customFolders.size() > 0)
		customFolderMenu.addSeparator();
	customFolderMenu.addAction("Add to new custom folder...", [this, &machine, &customFolders]()
	{
		auto customFolderExistsFunc = [&customFolders](const QString &name)
		{
			return customFolders.find(name) != customFolders.end();
		};
		NewCustomFolderDialog dlg(std::move(customFolderExistsFunc), this);
		dlg.setWindowTitle("Add to new custom folder");
		if (dlg.exec() == QDialog::Accepted)
		{
			// add this machine to the custom folder
			m_prefs.addMachineToCustomFolder(dlg.newCustomFolderName(), machine.name());
		}
	});

	// if we're selecting a custom folder, we can have an item to remove this
	QString currentCustomFolder = currentlySelectedCustomFolder();
	if (!currentCustomFolder.isEmpty())
	{
		popupMenu.addAction(QString("Remove from \"%1\"").arg(currentCustomFolder), [this, &currentCustomFolder, &machine]()
		{
			// erase this item
			m_prefs.removeMachineFromCustomFolder(currentCustomFolder, machine.name());
		});
	}

	// audit action
	popupMenu.addSeparator();
	popupMenu.addAction(auditThisActionText(description), [this, machine, software]()
	{
		if (software)
			manualAudit(*software);
		else
			manualAudit(machine);
	});

	// and execute the popup
	popupMenu.exec(pos);
}


//-------------------------------------------------
//  currentlySelectedCustomFolder
//-------------------------------------------------

QString MainPanel::currentlySelectedCustomFolder() const
{
	QModelIndexList folderSelection = m_ui->machinesFolderTreeView->selectionModel()->selectedIndexes();
	return folderSelection.size() == 1
		? machineFolderTreeModel().customFolderForModelIndex(folderSelection[0])
		: QString();
}


//-------------------------------------------------
//  deleteSelectedFolder
//-------------------------------------------------

void MainPanel::deleteSelectedFolder()
{
	QModelIndexList selection = m_ui->machinesFolderTreeView->selectionModel()->selectedIndexes();
	for (const QModelIndex &index : selection)
		machineFolderTreeModel().deleteFolder(index);
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
//  createProfile
//-------------------------------------------------

void MainPanel::createProfile(const info::machine &machine, const software_list::software *software)
{
	// find a path to create the new profile in
	QStringList paths = m_prefs.getSplitPaths(Preferences::global_path_type::PROFILES);
	auto iter = std::find_if(paths.begin(), paths.end(), [](const QString &path)
	{
		QDir dir(path);
		return dir.exists();
	});
	if (iter == paths.end())
	{
		iter = std::find_if(paths.begin(), paths.end(), DirExistsOrMake);
		profileListItemModel().refresh(false, true);
	}
	if (iter == paths.end())
	{
		QMessageBox::critical(this, QApplication::applicationDisplayName(), "Cannot create profile without valid profile path");
		return;
	}
	const QString &path = *iter;

	// identify the description, for use when we create the file name
	const QString &description = software
		? software->description()
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
		QMessageBox::critical(this, QApplication::applicationDisplayName(), "Could not create profile");
		return;
	}

	// create the new profile and focus
	profiles::profile::create(file, machine, software);
	focusOnNewProfile(std::move(new_profile_path));
}


//-------------------------------------------------
//  DirExistsOrMake
//-------------------------------------------------

bool MainPanel::DirExistsOrMake(const QString &path)
{
	bool result = QDir(path).exists();
	if (!result)
		result = QDir().mkdir(path);
	return result;
}


//-------------------------------------------------
//  DuplicateProfile
//-------------------------------------------------

void MainPanel::duplicateProfile(const profiles::profile &profile)
{
	QFileInfo fi(profile.path());
	QString dir_path = fi.dir().absolutePath();

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
		QMessageBox::critical(this, QApplication::applicationDisplayName(), "Could not create profile");
		return;
	}

	// create the new profile
	profile.save_as(file);

	// copy the save state file also, if present
	QString old_save_state_file = profiles::profile::change_path_save_state(profile.path());
	if (QFile(old_save_state_file).exists())
	{
		QString new_save_state_file = profiles::profile::change_path_save_state(new_profile_path);
		QFile::copy(old_save_state_file, new_save_state_file);
	}

	// finally focus
	focusOnNewProfile(std::move(new_profile_path));
}


//-------------------------------------------------
//  deleteProfile
//-------------------------------------------------

void MainPanel::deleteProfile(const profiles::profile &profile)
{
	QString message = QString("Are you sure you want to delete profile \"%1\"").arg(profile.name());
	auto rc = QMessageBox::question(this, QApplication::applicationDisplayName(), message, QMessageBox::Yes | QMessageBox::No);
	if (rc != QMessageBox::StandardButton::Yes)
		return;

	if (!profiles::profile::profile_file_remove(profile.path()))
		QMessageBox::critical(this, QApplication::applicationDisplayName(), "Could not delete profile");
}


//-------------------------------------------------
//  focusOnNewProfile
//-------------------------------------------------

void MainPanel::focusOnNewProfile(QString &&new_profile_path)
{
	// set the profiles tab as selected
	m_ui->tabWidget->setCurrentWidget(m_ui->profilesTab);

	// set the profile as selected, so we focus on it when we rebuild the list view
	m_prefs.setListViewSelection(s_profileListTableViewDesc.m_name, QString(new_profile_path));

	// we want the current profile to be renamed - do this with a callback
	profileListItemModel().setOneTimeFswCallback([this, profilePath{ std::move(new_profile_path) }]()
	{
		QModelIndex index = profileListItemModel().findProfileIndex(profilePath);
		if (!index.isValid())
			return;

		QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapFromSource(index);
		if (!actualIndex.isValid())
			return;

		m_ui->profilesTableView->edit(actualIndex);
	});
}


//-------------------------------------------------
//  editSelection
//-------------------------------------------------

void MainPanel::editSelection(QAbstractItemView &itemView)
{
	QModelIndexList selection = itemView.selectionModel()->selectedIndexes();
	for (const QModelIndex &modelIndex : selection)
		itemView.edit(modelIndex);
}


//-------------------------------------------------
//  showInGraphicalShell
//-------------------------------------------------

void MainPanel::showInGraphicalShell(const QString &path) const
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

info::machine MainPanel::machineFromModelIndex(const QModelIndex &index) const
{
	// get the proxy model
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->machinesTableView);

	// map the index to the actual index
	QModelIndex actualIndex = proxyModel.mapToSource(index);

	// and return the machine
	return machineListItemModel().machineFromIndex(actualIndex);
}


//-------------------------------------------------
//  softwareFromModelIndex
//-------------------------------------------------

const software_list::software &MainPanel::softwareFromModelIndex(const QModelIndex &index) const
{
	// get the proxy model
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->softwareTableView);

	// map the index to the actual index
	QModelIndex actualIndex = proxyModel.mapToSource(index);

	// and return the software
	return softwareListItemModel().getSoftwareByIndex(actualIndex.row());
}


//-------------------------------------------------
//  machineFolderTreeModel
//-------------------------------------------------

const MachineFolderTreeModel &MainPanel::machineFolderTreeModel() const
{
	return *dynamic_cast<MachineFolderTreeModel *>(m_ui->machinesFolderTreeView->model());
}


//-------------------------------------------------
//  machineFolderTreeModel
//-------------------------------------------------

MachineFolderTreeModel &MainPanel::machineFolderTreeModel()
{
	return *dynamic_cast<MachineFolderTreeModel *>(m_ui->machinesFolderTreeView->model());
}


//-------------------------------------------------
//  machineListItemModel
//-------------------------------------------------

MachineListItemModel &MainPanel::machineListItemModel()
{
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->machinesTableView);
	return *dynamic_cast<MachineListItemModel *>(proxyModel.sourceModel());
}


//-------------------------------------------------
//  machineListItemModel
//-------------------------------------------------

const MachineListItemModel &MainPanel::machineListItemModel() const
{
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->machinesTableView);
	return *dynamic_cast<MachineListItemModel *>(proxyModel.sourceModel());
}


//-------------------------------------------------
//  softwareListItemModel
//-------------------------------------------------

SoftwareListItemModel &MainPanel::softwareListItemModel()
{
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->softwareTableView);
	return *dynamic_cast<SoftwareListItemModel *>(proxyModel.sourceModel());
}


//-------------------------------------------------
//  softwareListItemModel
//-------------------------------------------------

const SoftwareListItemModel &MainPanel::softwareListItemModel() const
{
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->softwareTableView);
	return *dynamic_cast<SoftwareListItemModel *>(proxyModel.sourceModel());
}


//-------------------------------------------------
//  profileListItemModel
//-------------------------------------------------

ProfileListItemModel &MainPanel::profileListItemModel()
{
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->profilesTableView);
	return *dynamic_cast<ProfileListItemModel *>(proxyModel.sourceModel());
}


//-------------------------------------------------
//  sortFilterProxyModel
//-------------------------------------------------

const QSortFilterProxyModel &MainPanel::sortFilterProxyModel(const QTableView &tableView) const
{
	assert(&tableView == m_ui->machinesTableView || &tableView == m_ui->softwareTableView || &tableView == m_ui->profilesTableView);
	return *dynamic_cast<QSortFilterProxyModel *>(tableView.model());
}


//-------------------------------------------------
//  updateTabContents
//-------------------------------------------------

void MainPanel::updateTabContents()
{
	Preferences::list_view_type list_view_type = m_prefs.getSelectedTab();

	switch (list_view_type)
	{
	case Preferences::list_view_type::SOFTWARELIST:
		updateSoftwareList();
		break;

	default:
		// do nothing
		break;
	}
}


//-------------------------------------------------
//  updateSoftwareList
//-------------------------------------------------

void MainPanel::updateSoftwareList()
{
	// identify the selection
	QModelIndexList selection = m_ui->machinesTableView->selectionModel()->selectedIndexes();
	if (selection.size() > 0)
	{
		// load software lists for the current machine
		const info::machine machine = machineFromModelIndex(selection[0]);

		// get the software list collection
		software_list_collection &softwareListCollection = m_host.getSoftwareListCollection();

		// load the software
		m_currentSoftwareList = machine.name();
		softwareListCollection.load(m_prefs, machine);

		// and load the model
		softwareListItemModel().load(softwareListCollection, false);
	}
	else
	{
		// no machines are selected - reset the software list view
		softwareListItemModel().reset();
	}
}


//-------------------------------------------------
//  updateStatusFromSelection
//-------------------------------------------------

void MainPanel::updateStatusFromSelection()
{
	std::optional<info::machine> selectedMachine;
	const software_list::software *selectedSoftware;
	std::shared_ptr<profiles::profile> selectedProfile;
	QString description, statusString, totalString;
	QTableView *tableView;

	Preferences::list_view_type list_view_type = m_prefs.getSelectedTab();
	switch (list_view_type)
	{
	case Preferences::list_view_type::MACHINE:
		selectedMachine = currentlySelectedMachine();
		if (selectedMachine)
		{
			description = selectedMachine->description();
			statusString = machineStatusString(selectedMachine.value());
		}
		totalString = "%1 machines";
		tableView = m_ui->machinesTableView;
		break;

	case Preferences::list_view_type::SOFTWARELIST:
		selectedSoftware = currentlySelectedSoftware();
		if (selectedSoftware)
		{
			description = selectedSoftware->description();
		}
		totalString = "%1 softwares";
		tableView = m_ui->softwareTableView;
		break;

	case Preferences::list_view_type::PROFILE:
		selectedProfile = currentlySelectedProfile();
		if (selectedProfile)
		{
			description = selectedProfile->name();
		}
		totalString = "%1 profiles";
		tableView = m_ui->profilesTableView;
		break;

	default:
		throw false;
	}

	// actually return the status
	std::array<QString, STATUS_ENTRIES> status;
	status[0] = description.isEmpty() ? "No selection" : std::move(description);
	status[1] = std::move(statusString);
	status[2] = totalString.arg(tableView->model()->rowCount());
	setStatus(status);
}


//-------------------------------------------------
//  machineStatusString
//-------------------------------------------------

QString MainPanel::machineStatusString(const info::machine &machine) const
{
	QString result;
	switch (m_prefs.getMachineAuditStatus(machine.name()))
	{
	case AuditStatus::Unknown:
		result = "Unknown";
		break;
	case AuditStatus::Found:
		switch (machine.quality_status())
		{
		case info::machine::driver_quality_t::UNKNOWN:
		case info::machine::driver_quality_t::GOOD:
		default:
			result = "Working";
			break;
		case info::machine::driver_quality_t::IMPERFECT:
			result = "Imperfect";
			break;
		case info::machine::driver_quality_t::PRELIMINARY:
			result = "Preliminary";
			break;
		}
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
//  machineFoldersTreeViewSelectionChanged
//-------------------------------------------------

void MainPanel::machineFoldersTreeViewSelectionChanged(const QItemSelection &newSelection, const QItemSelection &oldSelection)
{
	// awaken the machine audit cursor
	m_host.updateAuditTimer();

	// identify the selection
	QModelIndexList selectedIndexes = newSelection.indexes();
	QModelIndex selectedIndex = !selectedIndexes.empty() ? selectedIndexes[0] : QModelIndex();

	// and configure the filter
	auto machineFilter = machineFolderTreeModel().getMachineFilter(selectedIndex);
	machineListItemModel().setMachineFilter(std::move(machineFilter));

	// update preferences
	QString path = machineFolderTreeModel().pathFromModelIndex(selectedIndex);
	m_prefs.setMachineFolderTreeSelection(std::move(path));
}


//-------------------------------------------------
//  persistMachineSplitterSizes
//-------------------------------------------------

void MainPanel::persistMachineSplitterSizes()
{
	QList<int> splitterSizes = m_ui->machinesSplitter->sizes();
	m_prefs.setMachineSplitterSizes(std::move(splitterSizes));
}


//-------------------------------------------------
//  updateInfoPanel
//-------------------------------------------------

void MainPanel::updateInfoPanel(const QString &machineName)
{
	m_currentSnapshot = QPixmap();

	// do we have a machine?
	if (!machineName.isEmpty())
	{
		// find the snapshot asset
		AssetFinder assetFinder(m_prefs, Preferences::global_path_type::SNAPSHOTS);
		std::optional<QByteArray> byteArray = assetFinder.findAssetBytes(machineName + ".png");

		// and if we got something, load it
		if (byteArray)
			m_currentSnapshot.loadFromData(*byteArray);
	}

	updateSnapshot();
}


//-------------------------------------------------
//  updateSnapshot
//-------------------------------------------------

void MainPanel::updateSnapshot()
{
	setPixmapDevicePixelRatioToFit(m_currentSnapshot, m_ui->machinesSnapLabel->size());
	m_ui->machinesSnapLabel->setPixmap(m_currentSnapshot);
}


//-------------------------------------------------
//  identifyExpandedFolderTreeItems
//-------------------------------------------------

void MainPanel::identifyExpandedFolderTreeItems()
{
	m_expandedTreeItems.clear();

	iterateItemModelIndexes(*m_ui->machinesFolderTreeView->model(), [this](const QModelIndex &index)
	{
		if (m_ui->machinesFolderTreeView->isExpanded(index))
		{
			QString path = machineFolderTreeModel().pathFromModelIndex(index);
			m_expandedTreeItems.push_back(std::move(path));
		}
	});
}


//-------------------------------------------------
//  iterateItemModelIndexes
//-------------------------------------------------

void MainPanel::iterateItemModelIndexes(QAbstractItemModel &model, const std::function<void(const QModelIndex &)> &func, const QModelIndex &index)
{
	if (index.isValid())
		func(index);

	if (model.hasChildren(index) && !(index.flags() & Qt::ItemNeverHasChildren))
	{
		int rows = model.rowCount(index);
		int columns = model.columnCount(index);
		for (int row = 0; row < rows; row++)
		{
			for (int col = 0; col < columns; col++)
			{
				iterateItemModelIndexes(model, func, model.index(row, col, index));
			}
		}
	}
}


//-------------------------------------------------
//  manualAudit
//-------------------------------------------------

void MainPanel::manualAudit(const info::machine &machine)
{
	// set up the audit task
	AuditTask::ptr auditTask = std::make_shared<AuditTask>(true, -1);
	const Audit &audit = auditTask->addMachineAudit(m_prefs, machine);

	// get the icon for this machine
	QPixmap pixmap = m_iconLoader.getIcon(machine, false);

	// and run the dialog
	runAuditDialog(audit, machine.name(), machine.description(), pixmap, auditTask);
}


//-------------------------------------------------
//  manualAudit
//-------------------------------------------------

void MainPanel::manualAudit(const software_list::software &software)
{
	// set up the audit task
	AuditTask::ptr auditTask = std::make_shared<AuditTask>(true, -1);
	const Audit &audit = auditTask->addSoftwareAudit(m_prefs, software);

	// and run the dialog
	runAuditDialog(audit, software.name(), software.description(), QPixmap(), auditTask);
}


//-------------------------------------------------
//  runAuditDialog
//-------------------------------------------------

void MainPanel::runAuditDialog(const Audit &audit, const QString &name, const QString &description, const QPixmap &pixmap, AuditTask::ptr auditTask)
{
	// set up the dialog
	AuditDialog auditDialog(audit, name, description, pixmap, m_iconLoader);

	// kick off the audit dialog process (keep a copy of the auditTask here)
	m_host.auditDialogStarted(auditDialog, std::move(auditTask));

	// and run the dialog
	auditDialog.exec();
}


//-------------------------------------------------
//  setAuditStatuses
//-------------------------------------------------

void MainPanel::setAuditStatuses(const std::vector<AuditResult> &results)
{
	// update all statuses
	for (const AuditResult &result : results)
	{
		// determine the type of audit
		std::visit([this, &result](auto &&identifier)
		{
			using T = std::decay_t<decltype(identifier)>;
			if constexpr (std::is_same_v<T, MachineAuditIdentifier>)
			{
				// does this machine audit result represent a change?
				if (m_prefs.getMachineAuditStatus(identifier.machineName()) != result.status())
				{
					// if so, record it
					m_prefs.setMachineAuditStatus(identifier.machineName(), result.status());
					machineListItemModel().auditStatusChanged(identifier);
				}
			}
			else if constexpr (std::is_same_v<T, SoftwareAuditIdentifier>)
			{
				// does this software audit result represent a change?
				if (m_prefs.getSoftwareAuditStatus(identifier.softwareList(), identifier.software()) != result.status())
				{
					// if so, record it
					m_prefs.setSoftwareAuditStatus(identifier.softwareList(), identifier.software(), result.status());
					softwareListItemModel().auditStatusChanged(identifier);
				}
			}
			else
			{
				throw false;
			}
		}, result.identifier());
	}
}


//-------------------------------------------------
//  machineAuditStatusesChanged
//-------------------------------------------------

void MainPanel::machineAuditStatusesChanged()
{
	machineListItemModel().allAuditStatusesChanged();
}


//-------------------------------------------------
//  softwareAuditStatusesChanged
//-------------------------------------------------

void MainPanel::softwareAuditStatusesChanged()
{
	softwareListItemModel().allAuditStatusesChanged();
}


//-------------------------------------------------
//  auditThisActionText
//-------------------------------------------------

QString MainPanel::auditThisActionText(const QString &text)
{
	return auditThisActionText(QString(text));
}


//-------------------------------------------------
//  auditThisActionText
//-------------------------------------------------

QString MainPanel::auditThisActionText(QString &&text)
{
	QString result;
	if (!text.isEmpty())
	{
		// need to escape this text
		text.replace(QString("&"), QString("&&"));

		// build the result
		result = QString("Audit \"%1\"").arg(text);
	}
	else
	{
		result = "Audit This";
	}
	return result;
}


//-------------------------------------------------
//  setStatus
//-------------------------------------------------

void MainPanel::setStatus(const std::array<QString, 3> &status)
{
	if (status != m_status)
	{
		m_status = status;
		emit statusChanged(status);
	}
}


//-------------------------------------------------
//  on_machinesFolderTreeView_customContextMenuRequested
//-------------------------------------------------

void MainPanel::on_machinesFolderTreeView_customContextMenuRequested(const QPoint &pos)
{
	int editableCount = 0;
	QModelIndexList selection = m_ui->machinesFolderTreeView->selectionModel()->selectedIndexes();
	for (const QModelIndex &index : selection)
	{
		if (machineFolderTreeModel().flags(index) & Qt::ItemIsEditable)
			editableCount++;
	}
	bool isEditable = selection.size() > 0 && selection.size() == editableCount;

	// main popup menu
	QMenu popupMenu;
	popupMenu.addAction("Rename", [this]()	{ editSelection(*m_ui->machinesFolderTreeView); })->setEnabled(isEditable);
	popupMenu.addAction("Delete", [this]()	{ deleteSelectedFolder(); })->setEnabled(isEditable);
	popupMenu.addSeparator();
	QMenu &showFoldersMenu = *popupMenu.addMenu("Show Folders");

	// show folders menu
	for (const MachineFolderTreeModel::RootFolderDesc &desc : machineFolderTreeModel().getRootFolderList())
	{
		QAction &action = *showFoldersMenu.addAction(desc.displayName(), [this, &desc]()
		{
			FolderPrefs folderPrefs = m_prefs.getFolderPrefs(desc.id());
			folderPrefs.m_shown = !folderPrefs.m_shown;
			m_prefs.setFolderPrefs(desc.id(), std::move(folderPrefs));
		});
		action.setCheckable(true);
		action.setChecked(m_prefs.getFolderPrefs(desc.id()).m_shown);
	}

	popupMenu.exec(m_ui->machinesFolderTreeView->mapToGlobal(pos));
}


//-------------------------------------------------
//  on_machinesTableView_activated
//-------------------------------------------------

void MainPanel::on_machinesTableView_activated(const QModelIndex &index)
{
	// run the machine
	const info::machine machine = machineFromModelIndex(index);
	run(machine);
}


//-------------------------------------------------
//  on_machinesTableView_customContextMenuRequested
//-------------------------------------------------

void MainPanel::on_machinesTableView_customContextMenuRequested(const QPoint &pos)
{
	launchingListContextMenu(m_ui->machinesTableView->mapToGlobal(pos));
}


//-------------------------------------------------
//  on_softwareTableView_activated
//-------------------------------------------------

void MainPanel::on_softwareTableView_activated(const QModelIndex &index)
{
	// identify the machine
	const info::machine machine = m_infoDb.find_machine(m_currentSoftwareList).value();

	// map the index to the actual index
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->softwareTableView).mapToSource(index);

	// identify the software
	const software_list::software &software = softwareListItemModel().getSoftwareByIndex(actualIndex.row());

	// and run!
	run(machine, &software);
}


//-------------------------------------------------
//  on_softwareTableView_customContextMenuRequested
//-------------------------------------------------

void MainPanel::on_softwareTableView_customContextMenuRequested(const QPoint &pos)
{
	// identify the selected software
	QModelIndexList selection = m_ui->softwareTableView->selectionModel()->selectedIndexes();
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->softwareTableView).mapToSource(selection[0]);
	const software_list::software &sw = softwareListItemModel().getSoftwareByIndex(actualIndex.row());

	// and launch the context menu
	launchingListContextMenu(m_ui->softwareTableView->mapToGlobal(pos), &sw);
}


//-------------------------------------------------
//  on_profilesTableView_activated
//-------------------------------------------------

void MainPanel::on_profilesTableView_activated(const QModelIndex &index)
{
	// map the index to the actual index
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapToSource(index);

	// identify the profile
	std::shared_ptr<profiles::profile> profile = profileListItemModel().getProfileByIndex(actualIndex.row());

	// and run!
	run(std::move(profile));
}


//-------------------------------------------------
//  on_profilesTableView_customContextMenuRequested
//-------------------------------------------------

void MainPanel::on_profilesTableView_customContextMenuRequested(const QPoint &pos)
{
	// identify the selected profile
	std::shared_ptr<profiles::profile> profile = currentlySelectedProfile();
	if (!profile)
		return;

	// build the popup menu
	QMenu popupMenu;
	popupMenu.addAction(QString("Run \"%1\"").arg(profile->name()), [this, &profile]() { run(std::move(profile)); });
	popupMenu.addAction("Duplicate", [this, &profile]() { duplicateProfile(*profile); });
	popupMenu.addAction("Rename", [this]()				{ editSelection(*m_ui->profilesTableView); });
	popupMenu.addAction("Delete", [this, &profile]()	{ deleteProfile(*profile); });
	popupMenu.addSeparator();
	popupMenu.addAction("Show in folder", [this, &profile]() { showInGraphicalShell(profile->path()); });
	popupMenu.exec(m_ui->profilesTableView->mapToGlobal(pos));
}


//-------------------------------------------------
//  on_tabWidget_currentChanged
//-------------------------------------------------

void MainPanel::on_tabWidget_currentChanged(int index)
{
	Preferences::list_view_type list_view_type = static_cast<Preferences::list_view_type>(index);
	m_prefs.setSelectedTab(list_view_type);
	updateTabContents();
	updateStatusFromSelection();
}


//-------------------------------------------------
//  on_machinesSplitter_splitterMoved
//-------------------------------------------------

void MainPanel::on_machinesSplitter_splitterMoved(int pos, int index)
{
	persistMachineSplitterSizes();
}
