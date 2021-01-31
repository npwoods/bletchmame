/***************************************************************************

	machinefoldertreemodel.cpp

	QAbstractItemModel implementation for the machine folder tree

***************************************************************************/

#include <set>
#include <QPixmap>

#include "machinefoldertreemodel.h"


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

MachineFolderTreeModel::MachineFolderTreeModel(QObject *parent, info::database &infoDb)
	: QAbstractItemModel(parent)
	, m_infoDb(infoDb)
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
	m_root.emplace_back("originals",	FolderIcon::Folder,		"Originals",		[](const info::machine &machine) { return machine.clone_of().isEmpty(); });
	m_root.emplace_back("source",		FolderIcon::Folder,		"Source",			m_source);
	m_root.emplace_back("year",			FolderIcon::Folder,		"Year",				m_year);

	// a number of folders are variable, and depend on info DB info; populate them separately
	m_infoDb.addOnChangedHandler([this]
	{
		beginResetModel();
		populateVariableFolders();
		endResetModel();
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
		result = entry.text();
		break;

	case Qt::DecorationRole:
		result = m_folderIcons[(int)entry.icon()];
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
	return true
		? QAbstractItemModel::flags(index)
		: Qt::NoItemFlags;
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
