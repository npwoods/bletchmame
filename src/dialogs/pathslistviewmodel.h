/***************************************************************************

	pathslistviewmodel.h

	Paths Dialog List View Model

***************************************************************************/

#pragma once

#ifndef DIALOGS_PATHSLISTVIEWMODEL_H
#define DIALOGS_PATHSLISTVIEWMODEL_H

#include <QAbstractListModel>

#include "utility.h"


//**************************************************************************
//  TYPES
//**************************************************************************

class PathsListViewModel : public QAbstractListModel
{
public:
	typedef std::function<bool(QString &)> CanonicalizeAndValidateFunc;

	// ctor
	PathsListViewModel(CanonicalizeAndValidateFunc canonicalizeAndValidateFunc = { }, QObject *parent = nullptr);

	// accessors
	const QString &path(int index) const	{ return m_entries[index].m_path; }
	int pathCount() const					{ return util::safe_static_cast<int>(m_entries.size()); }

	// setting/getting full paths
	void setPaths(QStringList &&paths, bool isMulti);
	QStringList paths() const;

	// functions to determine browse/insert/delete entries based on selection
	std::optional<int> getBrowseTargetForSelection(const QModelIndexList &selectedIndexes) const;
	std::optional<int> getInsertTargetForSelection(const QModelIndexList &selectedIndexes) const;
	std::vector<int> getDeleteTargetsForSelection(const QModelIndexList &selectedIndexes) const;

	// actual browse/insert/delete functionality
	void setPath(int index, QString &&path);
	void insertPath(int index, QString &&path);
	void deletePath(int index);
	void deletePaths(std::vector<int> &&indexes);

	// virtuals
	virtual int rowCount(const QModelIndex &parent) const;
	virtual QVariant data(const QModelIndex &index, int role) const;
	virtual bool setData(const QModelIndex &index, const QVariant &value, int role);
	virtual Qt::ItemFlags flags(const QModelIndex &index) const;

private:
	struct Entry
	{
		QString			m_path;
		bool			m_isValid;
	};

	CanonicalizeAndValidateFunc	m_canonicalizeAndValidateFunc;
	std::vector<Entry>			m_entries;
	bool						m_isMulti;

	// private methods
	int rowCount() const;
	void internalSetPath(int index, QString &&path);
	void dataChanged(int beginRow, int endRow);
	void canonicalizeAndValidate(Entry &entry);
};


#endif // DIALOGS_PATHSLISTVIEWMODEL_H
