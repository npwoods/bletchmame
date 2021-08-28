/***************************************************************************

	mainpanel.cpp

	Main BletchMAME panel

***************************************************************************/

#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSortFilterProxyModel>

#include "machinefoldertreemodel.h"
#include "machinelistitemmodel.h"
#include "mainpanel.h"
#include "prefs.h"
#include "profilelistitemmodel.h"
#include "sessionbehavior.h"
#include "softwarelistitemmodel.h"
#include "splitterviewtoggler.h"
#include "tableviewmanager.h"
#include "ui_mainpanel.h"
#include "dialogs/choosesw.h"
#include "dialogs/newcustomfolder.h"


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
	{ "name",			85 },
	{ "description",	370 },
	{ "year",			50 },
	{ "manufacturer",	320 },
	{ nullptr }
};

static const TableViewManager::Description s_machineListTableViewDesc =
{
	"machine",
	(int)MachineListItemModel::Column::Machine,
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


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainPanel::MainPanel(info::database &infoDb, Preferences &prefs, std::function<void(const info::machine &, std::unique_ptr<SessionBehavior> &&)> &&runCallback, QWidget *parent)
	: QWidget(parent)
	, m_infoDb(infoDb)
	, m_prefs(prefs)
	, m_runCallback(std::move(runCallback))
	, m_softwareListItemModel(nullptr)
	, m_profileListItemModel(nullptr)
	, m_iconLoader(prefs)
{
	// set up Qt form
	m_ui = std::make_unique<Ui::MainPanel>();
	m_ui->setupUi(this);

	// set up machines view
	MachineListItemModel &machineListItemModel = *new MachineListItemModel(this, m_infoDb, m_iconLoader);
	TableViewManager::setup(
		*m_ui->machinesTableView,
		machineListItemModel,
		m_ui->machinesSearchBox,
		m_prefs,
		s_machineListTableViewDesc,
		[this](const QString &machineName) { updateInfoPanel(machineName); });

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

	// set up software list view
	m_softwareListItemModel = new SoftwareListItemModel(this);
	TableViewManager::setup(
		*m_ui->softwareTableView,
		*m_softwareListItemModel,
		m_ui->softwareSearchBox,
		m_prefs,
		ChooseSoftlistPartDialog::s_tableViewDesc);

	// set up the profile list view
	m_profileListItemModel = new ProfileListItemModel(this, m_prefs, m_infoDb, m_iconLoader);
	TableViewManager::setup(
		*m_ui->profilesTableView,
		*m_profileListItemModel,
		nullptr,
		m_prefs,
		s_profileListTableViewDesc);
	m_profileListItemModel->refresh(true, true);

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
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MainPanel::~MainPanel()
{
}


//-------------------------------------------------
//  pathsChanged
//-------------------------------------------------

void MainPanel::pathsChanged(const std::vector<Preferences::global_path_type> &changedPaths)
{
	// did the user change the profiles path?
	if (util::contains(changedPaths, Preferences::global_path_type::PROFILES))
		m_profileListItemModel->refresh(true, true);

	// did the user change the icons path?
	if (util::contains(changedPaths, Preferences::global_path_type::ICONS))
		m_iconLoader.refreshIcons();
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
	m_runCallback(machine, std::move(sessionBehavior));
}


//-------------------------------------------------
//  LaunchingListContextMenu
//-------------------------------------------------

void MainPanel::LaunchingListContextMenu(const QPoint &pos, const software_list::software *software)
{
	// identify the machine
	QModelIndex index = m_ui->machinesTableView->selectionModel()->selectedIndexes()[0];
	const info::machine machine = machineFromModelIndex(index);

	// identify the description
	const QString &description = software
		? software->m_description
		: machine.description();

	// build the menu
	QMenu popupMenu(this);
	popupMenu.addAction(QString("Run \"%1\"").arg(description), [this, machine, &software]() { run(machine, std::move(software));	});
	popupMenu.addAction("Create profile", [this, machine, &software]() { createProfile(machine, software);	});

	// build the custom folder menu
	QMenu &customFolderMenu = *popupMenu.addMenu("Add to custom folder");
	std::map<QString, std::set<QString>> &customFolders = m_prefs.getCustomFolders();
	for (auto &pair : customFolders)
	{
		// create the custom folder menu item
		const QString &customFolderName = pair.first;
		auto &customFolderSystems = pair.second;
		QAction &action = *customFolderMenu.addAction(customFolderName, [&customFolderSystems, &machine]()
		{
			customFolderSystems.emplace(machine.name());
		});

		// disable and check it if its already in the folder
		bool alreadyInFolder = util::contains(customFolderSystems, machine.name());
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
			// create the new folder
			std::set<QString> &newFolder = customFolders.emplace(dlg.newCustomFolderName(), std::set<QString>()).first->second;

			// add this machine
			newFolder.emplace(machine.name());

			// and refresh the folder list
			machineFolderTreeModel().refresh();
		}
	});

	// if we're selecting a custom folder, we can have an item to remove this
	QString currentCustomFolder = currentlySelectedCustomFolder();
	if (!currentCustomFolder.isEmpty())
	{
		popupMenu.addAction(QString("Remove from \"%1\"").arg(currentCustomFolder), [this, &customFolders, &currentCustomFolder, &machine]()
		{
			// erase this item
			customFolders[currentCustomFolder].erase(machine.name());

			// and refresh the folder list
			machineFolderTreeModel().refresh();
		});
	}

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
		m_profileListItemModel->refresh(false, true);
	}
	if (iter == paths.end())
	{
		QMessageBox::critical(this, QApplication::applicationDisplayName(), "Cannot create profile without valid profile path");
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
	m_prefs.setListViewSelection(s_profileListTableViewDesc.m_name, std::move(new_profile_path));

	// we want the current profile to be renamed - do this with a callback
	m_profileListItemModel->setOneTimeFswCallback([this, profilePath{ std::move(new_profile_path) }]()
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

	// get the machine list item model
	const MachineListItemModel &machineModel = *dynamic_cast<const MachineListItemModel *>(proxyModel.sourceModel());

	// and return the machine
	return machineModel.machineFromIndex(actualIndex);
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
//  sortFilterProxyModel
//-------------------------------------------------

const QSortFilterProxyModel &MainPanel::sortFilterProxyModel(const QTableView &tableView) const
{
	assert(&tableView == m_ui->machinesTableView || &tableView == m_ui->softwareTableView || &tableView == m_ui->profilesTableView);
	return *dynamic_cast<QSortFilterProxyModel *>(tableView.model());
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
//  machineFoldersTreeViewSelectionChanged
//-------------------------------------------------

void MainPanel::machineFoldersTreeViewSelectionChanged(const QItemSelection &newSelection, const QItemSelection &oldSelection)
{
	// get the relevant models
	const QSortFilterProxyModel &proxyModel = sortFilterProxyModel(*m_ui->machinesTableView);
	MachineListItemModel &machineListItemModel = *dynamic_cast<MachineListItemModel *>(proxyModel.sourceModel());

	// identify the selection
	QModelIndexList selectedIndexes = newSelection.indexes();
	QModelIndex selectedIndex = !selectedIndexes.empty() ? selectedIndexes[0] : QModelIndex();

	// and configure the filter
	auto machineFilter = machineFolderTreeModel().getMachineFilter(selectedIndex);
	machineListItemModel.setMachineFilter(std::move(machineFilter));

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
		// look for the pertinent snapshot file in every snapshot directory
		QStringList snapPaths = m_prefs.getSplitPaths(Preferences::global_path_type::SNAPSHOTS);
		for (const QString &path : snapPaths)
		{
			QString snapshotFileName = QString("%1/%2.png").arg(path, machineName);
			if (QFileInfo(snapshotFileName).exists())
			{
				m_currentSnapshot = QPixmap(snapshotFileName);
				if (m_currentSnapshot.width() > 0 && m_currentSnapshot.height() > 0)
					break;
			}
		}
	}

	updateSnapshot();
}


//-------------------------------------------------
//  updateSnapshot
//-------------------------------------------------

void MainPanel::updateSnapshot()
{
	QPixmap scaledSnapshot = m_currentSnapshot.scaled(
		m_ui->machinesSnapLabel->size(),
		Qt::KeepAspectRatio,
		Qt::SmoothTransformation);
	m_ui->machinesSnapLabel->setPixmap(scaledSnapshot);
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
			machineFolderTreeModel().refresh();
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
	LaunchingListContextMenu(m_ui->machinesTableView->mapToGlobal(pos));
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
	const software_list::software &software = m_softwareListItemModel->getSoftwareByIndex(actualIndex.row());

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
	const software_list::software &sw = m_softwareListItemModel->getSoftwareByIndex(actualIndex.row());

	// and launch the context menu
	LaunchingListContextMenu(m_ui->softwareTableView->mapToGlobal(pos), &sw);
}


//-------------------------------------------------
//  on_profilesTableView_activated
//-------------------------------------------------

void MainPanel::on_profilesTableView_activated(const QModelIndex &index)
{
	// map the index to the actual index
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapToSource(index);

	// identify the profile
	std::shared_ptr<profiles::profile> profile = m_profileListItemModel->getProfileByIndex(actualIndex.row());

	// and run!
	run(std::move(profile));
}


//-------------------------------------------------
//  on_profilesTableView_customContextMenuRequested
//-------------------------------------------------

void MainPanel::on_profilesTableView_customContextMenuRequested(const QPoint &pos)
{
	// identify the selected software
	QModelIndexList selection = m_ui->profilesTableView->selectionModel()->selectedIndexes();
	if (selection.count() <= 0)
		return;
	QModelIndex actualIndex = sortFilterProxyModel(*m_ui->profilesTableView).mapToSource(selection[0]);
	std::shared_ptr<profiles::profile> profile = m_profileListItemModel->getProfileByIndex(actualIndex.row());

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

	switch (list_view_type)
	{
	case Preferences::list_view_type::SOFTWARELIST:
		updateSoftwareList();
		break;
	}
}


//-------------------------------------------------
//  on_machinesSplitter_splitterMoved
//-------------------------------------------------

void MainPanel::on_machinesSplitter_splitterMoved(int pos, int index)
{
	persistMachineSplitterSizes();
}
