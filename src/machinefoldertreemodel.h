/***************************************************************************

	machinefoldertreemodel.h

	QAbstractItemModel implementation for the machine folder tree

***************************************************************************/

#ifndef MACHINEFOLDERTREEMODEL_H
#define MACHINEFOLDERTREEMODEL_H

#include <array>
#include <functional>

#include <QAbstractItemModel>

#include "info.h"


// ======================> MachineFolderTreeModel

class MachineFolderTreeModel : public QAbstractItemModel
{
public:
	class Test;

	// ctor
	MachineFolderTreeModel(QObject *parent, info::database &infoDb);

	// methods
	std::function<bool(const info::machine &machine)> getMachineFilter(const QModelIndex &index);
	QString pathFromModelIndex(const QModelIndex &index) const;
	QModelIndex modelIndexFromPath(const QString &path) const;

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const final;
	virtual QModelIndex parent(const QModelIndex &child) const final;
	virtual int rowCount(const QModelIndex &parent) const final;
	virtual int columnCount(const QModelIndex &parent) const final;
	virtual QVariant data(const QModelIndex &index, int role) const final;
	virtual bool hasChildren(const QModelIndex &parent) const final;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const final;

private:
	enum class FolderIcon
	{
		Folder,
		FolderOpen,
		Manufacturer,
		Source,
		Year,
		Count
	};

	class FolderEntry
	{
	public:
		// ctor
		FolderEntry(QString &&id, FolderIcon icon, QString &&text, std::function<bool(const info::machine &machine)> &&filter);
		FolderEntry(QString &&id, FolderIcon icon, QString &&text, const std::vector<FolderEntry> &children);

		// accessors
		const QString &id() const { return m_id; }
		FolderIcon icon() const { return m_icon; }
		const QString &text() const { return m_text; }
		const std::vector<FolderEntry> *children() const { return m_children; }
		const std::function<bool(const info::machine &machine)> &filter() const { return m_filter; }

	private:
		FolderEntry(QString &&id, FolderIcon icon, QString &&text, std::function<bool(const info::machine &machine)> &&filter, const std::vector<FolderEntry> *children);

		// variables
		QString												m_id;
		FolderIcon											m_icon;
		QString												m_text;
		std::function<bool(const info::machine &machine)>	m_filter;
		const std::vector<FolderEntry> *					m_children;
	};

	info::database &							m_infoDb;
	std::vector<FolderEntry>					m_root;
	std::vector<FolderEntry>					m_manufacturer;
	std::vector<FolderEntry>					m_source;
	std::vector<FolderEntry>					m_year;
	std::array<QPixmap, (int)FolderIcon::Count> m_folderIcons;

	static std::array<const char *, (int)FolderIcon::Count> getFolderIconResourceNames();
	static const FolderEntry &folderEntryFromModelIndex(const QModelIndex &index);
	const std::vector<FolderEntry> &childFolderEntriesFromModelIndex(const QModelIndex &parent) const;
	void populateVariableFolders();
};


#endif // MACHINEFOLDERTREEMODEL_H
