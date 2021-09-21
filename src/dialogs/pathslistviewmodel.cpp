/***************************************************************************

	pathslistviewmodel.cpp

	Paths Dialog List View Model

***************************************************************************/

#include "pathslistviewmodel.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

PathsListViewModel::PathsListViewModel(CanonicalizeAndValidateFunc canonicalizeAndValidateFunc, QObject *parent)
	: QAbstractListModel(parent)
	, m_canonicalizeAndValidateFunc(std::move(canonicalizeAndValidateFunc))
	, m_isMulti(false)
{
}


//-------------------------------------------------
//  setPaths
//-------------------------------------------------

void PathsListViewModel::setPaths(QStringList &&paths, bool isMulti)
{
	beginResetModel();
	m_entries.clear();

	m_isMulti = isMulti;

	for (QString &path : paths)
	{
		if (m_isMulti || m_entries.size() == 0)
			internalSetPath(m_entries.size(), std::move(path));
	}

	endResetModel();
}


//-------------------------------------------------
//  paths
//-------------------------------------------------

QStringList PathsListViewModel::paths() const
{
	QStringList results;
	for (int i = 0; i < m_entries.size(); i++)
		results.push_back(m_entries[i].m_path);
	return results;
}


//-------------------------------------------------
//  getBrowseTargetForSelection
//-------------------------------------------------

std::optional<int> PathsListViewModel::getBrowseTargetForSelection(const QModelIndexList &selectedIndexes) const
{
	std::optional<int> result;
	switch (selectedIndexes.size())
	{
	case 0:
		if (m_isMulti || (m_entries.size() == 0))
			result = m_entries.size();
		break;

	case 1:
		result = selectedIndexes[0].row();
		break;

	default:
		// do nothing
		break;
	}
	return result;
}


//-------------------------------------------------
//  getInsertTargetForSelection
//-------------------------------------------------

std::optional<int> PathsListViewModel::getInsertTargetForSelection(const QModelIndexList &selectedIndexes) const
{
	std::optional<int> result;
	switch (selectedIndexes.size())
	{
	case 0:
		if (m_isMulti || (m_entries.size() == 0))
			result = m_entries.size();
		break;

	case 1:
		if (m_isMulti || (m_entries.size() == 0))
			result = selectedIndexes[0].row();
		break;

	default:
		// do nothing
		break;
	}
	return result;
}


//-------------------------------------------------
//  getDeleteTargetsForSelection
//-------------------------------------------------

std::vector<int> PathsListViewModel::getDeleteTargetsForSelection(const QModelIndexList &selectedIndexes) const
{
	std::vector<int> result;
	for (const QModelIndex &index : selectedIndexes)
	{
		if (index.row() < pathCount())
			result.push_back(index.row());
	}
	return result;
}


//-------------------------------------------------
//  dataChanged
//-------------------------------------------------

void PathsListViewModel::dataChanged(int beginRow, int endRow)
{
	QModelIndex topLeft = index(beginRow, 0);
	QModelIndex bottomRight = index(endRow, 0);
	QAbstractItemModel::dataChanged(topLeft, bottomRight);
}


//-------------------------------------------------
//  setPath
//-------------------------------------------------

void PathsListViewModel::setPath(int index, QString &&path)
{
	internalSetPath(index, std::move(path));
	dataChanged(index, index);
}


//-------------------------------------------------
//  setPath
//-------------------------------------------------

void PathsListViewModel::internalSetPath(int index, QString &&path)
{
	// this can expand if we go one off the edge
	if (index == m_entries.size() && (m_isMulti || index == 0))
		m_entries.resize(m_entries.size() + 1);
	if (index >= m_entries.size())
		throw false;

	// set the path
	m_entries[index].m_path = std::move(path);
	canonicalizeAndValidate(m_entries[index]);
}


//-------------------------------------------------
//  insertPath
//-------------------------------------------------

void PathsListViewModel::insertPath(int index, QString &&path)
{
	if (!m_isMulti && m_entries.size() > 0)
		throw false;

	m_entries.insert(m_entries.begin() + index, Entry());
	setPath(index, std::move(path));
}


//-------------------------------------------------
//  deletePath
//-------------------------------------------------

void PathsListViewModel::deletePath(int index)
{
	m_entries.erase(m_entries.begin() + index);
}


//-------------------------------------------------
//  deletePaths
//-------------------------------------------------

void PathsListViewModel::deletePaths(std::vector<int> &&indexes)
{
	std::sort(indexes.begin(), indexes.end(), [](int a, int b)
	{
		return a < b;
	});

	for (int index : indexes)
		deletePath(index);
}


//-------------------------------------------------
//  canonicalizeAndValidate
//-------------------------------------------------

void PathsListViewModel::canonicalizeAndValidate(Entry &entry)
{
	bool isValid = m_canonicalizeAndValidateFunc
		? m_canonicalizeAndValidateFunc(entry.m_path)
		: true;
	entry.m_isValid = isValid;
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int PathsListViewModel::rowCount() const
{
	return pathCount() + (m_isMulti ? 1 : 0);
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int PathsListViewModel::rowCount(const QModelIndex &parent) const
{
	return rowCount();
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant PathsListViewModel::data(const QModelIndex &index, int role) const
{
	// identify the entry
	const Entry *entry = index.row() < m_entries.size()
		? &m_entries[index.row()]
		: nullptr;

	QVariant result;
	switch (role)
	{
	case Qt::DisplayRole:
		if (entry)
			result = entry->m_path;
		else if (m_isMulti && index.row() == m_entries.size())
			result = "<               >";
		break;

	case Qt::ForegroundRole:
		result = !entry || entry->m_isValid
			? QColorConstants::Black
			: QColorConstants::Red;
		break;

	case Qt::EditRole:
		if (entry)
			result = entry->m_path;
		break;
	}
	return result;
}


//-------------------------------------------------
//  setData
//-------------------------------------------------

bool PathsListViewModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	bool success = false;

	switch (role)
	{
	case Qt::EditRole:
		// convert the variant data to the format we want it in
		QString pathString = value.toString();

		// and set it
		if (pathString.isEmpty())
			deletePath(index.row());
		else
			setPath(index.row(), std::move(pathString));

		// success!
		success = true;
		break;
	}
	return success;
}


//-------------------------------------------------
//  flags
//-------------------------------------------------

Qt::ItemFlags PathsListViewModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::ItemIsEnabled;

	return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}
