/***************************************************************************

	machinefoldertreemodel.cpp

	QAbstractItemModel implementation for the machine folder tree

***************************************************************************/

#include <set>
#include <QPixmap>

#include "machinefoldertreemodel.h"
#include "prefs.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define ICON_SIZE_X 16
#define ICON_SIZE_Y 16


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MachineFolderTreeModel::MachineFolderTreeModel(QObject *parent, info::database &infoDb, Preferences &prefs)
	: QAbstractItemModel(parent)
	, m_infoDb(infoDb)
	, m_prefs(prefs)
{
	// load all folder icons
	auto folderIconResourceNames = getFolderIconResourceNames();
	if (folderIconResourceNames.size() != m_folderIcons.size())
		throw false;
	for (int i = 0; i < folderIconResourceNames.size(); i++)
	{
		QPixmap pixmap(folderIconResourceNames[i]);
		m_folderIcons[i] = pixmap.scaled(ICON_SIZE_X, ICON_SIZE_Y);
	}

	// root folder
	m_root.emplace_back("all",			FolderIcon::Folder,		"All Machines",		std::function<bool(const info::machine &machine)>());
	m_root.emplace_back("clones",		FolderIcon::Folder,		"Clones",			[](const info::machine &machine) { return !machine.clone_of().isEmpty(); });
	m_root.emplace_back("custom",		FolderIcon::Folder,		"Custom",			m_custom);
	m_root.emplace_back("originals",	FolderIcon::Folder,		"Originals",		[](const info::machine &machine) { return machine.clone_of().isEmpty(); });
	m_root.emplace_back("source",		FolderIcon::Folder,		"Source",			m_source);
	m_root.emplace_back("year",			FolderIcon::Folder,		"Year",				m_year);

	// a number of folders are variable, and depend on info DB info; populate them separately
	m_infoDb.addOnChangedHandler([this]
	{
		refresh();
	});
}


//-------------------------------------------------
//  getFolderIconResourceNames
//-------------------------------------------------

std::array<const char *, (int)MachineFolderTreeModel::FolderIcon::Count> MachineFolderTreeModel::getFolderIconResourceNames()
{
	std::array<const char *, (int)MachineFolderTreeModel::FolderIcon::Count> result;
	std::fill(result.begin(), result.end(), nullptr);
	result[(int)FolderIcon::Folder]			= ":/resources/folder.ico";
	result[(int)FolderIcon::FolderOpen]		= ":/resources/foldopen.ico";
	result[(int)FolderIcon::Manufacturer]	= ":/resources/manufact.ico";
	result[(int)FolderIcon::Source]			= ":/resources/source.ico";
	result[(int)FolderIcon::Year]			= ":/resources/year.ico";
	return result;
}


//-------------------------------------------------
//  containsEntry
//-------------------------------------------------

template<class T>
bool containsEntry(const std::vector<T> &vec, const T &obj)
{
	bool result = false;
	if (vec.size() > 0)
	{
		size_t offset = &obj - &vec[0];
		result = offset >= 0 && offset < vec.size();
	}
	return result;
}


//-------------------------------------------------
//  folderEntryFromModelIndex
//-------------------------------------------------

const MachineFolderTreeModel::FolderEntry &MachineFolderTreeModel::folderEntryFromModelIndex(const QModelIndex &index)
{
	assert(index.isValid());
	const FolderEntry *entry = static_cast<FolderEntry *>(index.internalPointer());
	return *entry;
}


//-------------------------------------------------
//  childFolderEntriesFromModelIndex
//-------------------------------------------------

const std::vector<MachineFolderTreeModel::FolderEntry> &MachineFolderTreeModel::childFolderEntriesFromModelIndex(const QModelIndex &parent) const
{
	return parent.isValid()
		? *folderEntryFromModelIndex(parent).children()
		: m_root;
}


//-------------------------------------------------
//  refresh
//-------------------------------------------------

void MachineFolderTreeModel::refresh()
{
	beginResetModel();
	populateVariableFolders();
	endResetModel();
}


//-------------------------------------------------
//  populateVariableFolders
//-------------------------------------------------

void MachineFolderTreeModel::populateVariableFolders()
{
	// iterate through all machines and accumulate the pertinent data; because
	// this can be expensive we're using reference wrappers
	std::set<std::reference_wrapper<const QString>> manufacturers;
	std::set<std::reference_wrapper<const QString>> sourceFiles;
	std::set<std::reference_wrapper<const QString>> years;
	for (info::machine machine : m_infoDb.machines())
	{
		if (machine.runnable())
		{
			manufacturers.emplace(machine.manufacturer());
			sourceFiles.emplace(machine.sourcefile());
			years.emplace(machine.year());
		}
	}

	// set up the custom folder
	const auto &customFolders = m_prefs.GetCustomFolders();
	m_custom.clear();
	m_custom.reserve(customFolders.size());
	for (const auto &pair : customFolders)
	{
		const QString &folderName = pair.first;
		const std::set<QString> &folderContents = pair.second;
		auto predicate = [&folderContents](const info::machine &machine) { return util::contains(folderContents, machine.name()); };
		m_custom.emplace_back(QString(folderName), FolderIcon::Folder, QString(folderName), std::move(predicate));
	}	

	// set up the manufacturers folder
	m_manufacturer.clear();
	m_manufacturer.reserve(manufacturers.size());
	for (const QString &manufacturer : manufacturers)
	{
		auto predicate = [&manufacturer](const info::machine &machine) { return machine.manufacturer() == manufacturer; };
		m_manufacturer.emplace_back(QString(manufacturer), FolderIcon::Manufacturer, QString(manufacturer), std::move(predicate));
	}

	// set up the sources folder
	m_source.clear();
	m_source.reserve(sourceFiles.size());
	for (const QString &sourceFile : sourceFiles)
	{
		auto predicate = [&sourceFile](const info::machine &machine) { return machine.sourcefile() == sourceFile; };
		m_source.emplace_back(QString(sourceFile), FolderIcon::Source, QString(sourceFile), std::move(predicate));
	}

	// set up the years folder
	m_year.clear();
	m_year.reserve(years.size());
	for (const QString &year : years)
	{
		auto predicate = [&year](const info::machine &machine) { return machine.year() == year; };
		m_year.emplace_back(QString(year), FolderIcon::Year, QString(year), std::move(predicate));
	}
}


//-------------------------------------------------
//  renameFolder
//-------------------------------------------------

bool MachineFolderTreeModel::renameFolder(const QModelIndex &index, QString &&newName)
{
	QString customFolder = customFolderForModelIndex(index);
	if (customFolder.isEmpty())
		return false;

	// is this just a no-op?
	if (customFolder == newName)
		return true;

	// get the custom folders map
	std::map<QString, std::set<QString>> &customFolders = m_prefs.GetCustomFolders();

	// find this entry
	auto iter = customFolders.find(customFolder);
	if (iter == customFolders.end())
		return false;

	// detach the list of systems
	std::set<QString> systems = std::move(iter->second);
	customFolders.erase(iter);

	// and readd it
	customFolders.emplace(std::move(newName), std::move(systems));

	// refresh and we're done!
	refresh();
	return true;
}


//-------------------------------------------------
//  deleteFolder
//-------------------------------------------------

bool MachineFolderTreeModel::deleteFolder(const QModelIndex &index)
{
	QString customFolder = customFolderForModelIndex(index);
	if (customFolder.isEmpty())
		return false;

	// get the custom folders map
	std::map<QString, std::set<QString>> &customFolders = m_prefs.GetCustomFolders();

	// find this entry
	auto iter = customFolders.find(customFolder);
	if (iter == customFolders.end())
		return false;

	// erase it
	customFolders.erase(iter);

	// refresh and we're done!
	refresh();
	return true;
}


//-------------------------------------------------
//  getMachineFilter
//-------------------------------------------------

std::function<bool(const info::machine &machine)> MachineFolderTreeModel::getMachineFilter(const QModelIndex &index)
{
	std::function<bool(const info::machine &machine)> result;
	if (index.isValid())
	{
		const FolderEntry &entry = folderEntryFromModelIndex(index);
		result = entry.filter();
	}
	return result;
}


//-------------------------------------------------
//  pathFromModelIndex
//-------------------------------------------------

QString MachineFolderTreeModel::pathFromModelIndex(const QModelIndex &index) const
{
	QString result;
	QModelIndex currentIndex = index;

	while (currentIndex.isValid())
	{
		// prepend this ID
		const QString &id = folderEntryFromModelIndex(currentIndex).id();
		result = !result.isEmpty()
			? id + "/" + result
			: id;

		// and go up the tree
		currentIndex = currentIndex.parent();
	};
	return result;
}


//-------------------------------------------------
//  modelIndexFromPath
//-------------------------------------------------

QModelIndex MachineFolderTreeModel::modelIndexFromPath(const QString &path) const
{
	QModelIndex result;
	const std::vector<FolderEntry> *entries = &m_root;

	for (const QString &part : path.split('/'))
	{
		auto iter = std::find_if(entries->begin(), entries->end(), [&part](const FolderEntry &x)
		{
			return x.id() == part;
		});
		if (iter == entries->end())
			return QModelIndex();

		// dive down the hierarchy
		result = index(iter - entries->begin(), 0, result);
		entries = iter->children();
	}
	return result;
}


//-------------------------------------------------
//  customFolderForModelIndex
//-------------------------------------------------

QString MachineFolderTreeModel::customFolderForModelIndex(const QModelIndex &index) const
{
	QString result;
	if (index.isValid() && m_custom.size() > 0)
	{
		const FolderEntry &entry = folderEntryFromModelIndex(index);
		if ((&entry - &m_custom[0] >= 0) && (&entry - &m_custom[0] < m_custom.size()))
			result = entry.id();
	}
	return result;
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex MachineFolderTreeModel::index(int row, int column, const QModelIndex &parent) const
{
	// determine the entry list for the parent
	const std::vector<FolderEntry> &entries = childFolderEntriesFromModelIndex(parent);

	// and create the index
	return createIndex(row, column, (void *)&entries[row]);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex MachineFolderTreeModel::parent(const QModelIndex &child) const
{
	QModelIndex result;
	if (child.isValid())
	{
		const FolderEntry &entry = folderEntryFromModelIndex(child);
		if (!containsEntry(m_root, entry))
		{
			auto iter = std::find_if(m_root.begin(), m_root.end(), [&entry](const FolderEntry &x)
			{
				return x.children() && containsEntry(*x.children(), entry);
			});
			if (iter != m_root.end())
			{
				size_t row = iter - m_root.begin();
				result = index(row, 0);
			}
		}
	}
	return result;
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int MachineFolderTreeModel::rowCount(const QModelIndex &parent) const
{
	return childFolderEntriesFromModelIndex(parent).size();
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int MachineFolderTreeModel::columnCount(const QModelIndex &parent) const
{
	return 1;
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant MachineFolderTreeModel::data(const QModelIndex &index, int role) const
{
	const FolderEntry &entry = folderEntryFromModelIndex(index);

	QVariant result;
	switch (role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole:
		result = entry.text();
		break;

	case Qt::DecorationRole:
		result = m_folderIcons[(int)entry.icon()];
		break;
	}

	return result;
}


//-------------------------------------------------
//  setData
//-------------------------------------------------

bool MachineFolderTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	bool result;
	switch (role)
	{
	case Qt::EditRole:
		result = renameFolder(index, value.toString());
		break;

	default:
		result = QAbstractItemModel::setData(index, value, role);
		break;
	}
	return result;
}


//-------------------------------------------------
//  hasChildren
//-------------------------------------------------

bool MachineFolderTreeModel::hasChildren(const QModelIndex &parent) const
{
	return !parent.isValid() || folderEntryFromModelIndex(parent).children();
}


//-------------------------------------------------
//  flags
//-------------------------------------------------

Qt::ItemFlags MachineFolderTreeModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags result = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	QString customFolder = customFolderForModelIndex(index);
	if (!customFolder.isEmpty())
		result |= Qt::ItemIsEditable;
	return result;
}


//-------------------------------------------------
//  FolderEntry ctor
//-------------------------------------------------

MachineFolderTreeModel::FolderEntry::FolderEntry(QString &&id, FolderIcon icon, QString &&text, std::function<bool(const info::machine &machine)> &&filter)
	: FolderEntry(std::move(id), icon, std::move(text), std::move(filter), nullptr)
{
}


//-------------------------------------------------
//  FolderEntry ctor
//-------------------------------------------------

MachineFolderTreeModel::FolderEntry::FolderEntry(QString &&id, FolderIcon icon, QString &&text, const std::vector<FolderEntry> &children)
	: FolderEntry(std::move(id), icon, std::move(text), { }, &children)
{
}


//-------------------------------------------------
//  FolderEntry ctor
//-------------------------------------------------

MachineFolderTreeModel::FolderEntry::FolderEntry(QString &&id, FolderIcon icon, QString &&text, std::function<bool(const info::machine &machine)> &&filter, const std::vector<FolderEntry> *children)
	: m_id(id)
	, m_icon(icon)
	, m_text(std::move(text))
	, m_filter(std::move(filter))
	, m_children(children)
{
}
