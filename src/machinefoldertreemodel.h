/***************************************************************************

	machinefoldertreemodel.h

	QAbstractItemModel implementation for the machine folder tree

***************************************************************************/

#ifndef MACHINEFOLDERTREEMODEL_H
#define MACHINEFOLDERTREEMODEL_H

// bletchmame headers
#include "info.h"

// Qt headers
#include <QAbstractItemModel>

// standard headers
#include <array>
#include <functional>

class Preferences;
class FolderPrefs;


// ======================> MachineFolderTreeModel

class MachineFolderTreeModel : public QAbstractItemModel
{
public:
	class Test;

	class RootFolderDesc
	{
	public:
		RootFolderDesc(const char *id, QString &&displayName);

		// accessors
		const char *id() const				{ return m_id; }
		const QString &displayName() const	{ return m_displayName; }

	private:
		const char *	m_id;
		QString			m_displayName;
	};

	// ctor
	MachineFolderTreeModel(QObject *parent, info::database &infoDb, Preferences &prefs);

	// accessors
	const auto &getRootFolderList() { return m_rootFolderList; }

	// methods
	std::function<bool(const info::machine &machine)> getMachineFilter(const QModelIndex &index);
	QString pathFromModelIndex(const QModelIndex &index) const;
	QModelIndex modelIndexFromPath(const QString &path) const;
	QString customFolderForModelIndex(const QModelIndex &index) const;
	void refresh();
	bool renameFolder(const QModelIndex &index, QString &&newName);
	bool deleteFolder(const QModelIndex &index);

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const final;
	virtual QModelIndex parent(const QModelIndex &child) const final;
	virtual int rowCount(const QModelIndex &parent) const final;
	virtual int columnCount(const QModelIndex &parent) const final;
	virtual QVariant data(const QModelIndex &index, int role) const final;
	virtual bool setData(const QModelIndex &index, const QVariant &value, int role) final;
	virtual bool hasChildren(const QModelIndex &parent) const final;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const final;

private:
	enum class FolderIcon
	{
		Cpu,
		Folder,
		FolderAvailable,
		FolderOpen,
		HardDisk,
		Manufacturer,
		Sound,
		Source,
		Year,

		Max = Year
	};

	class FolderEntry
	{
	public:
		// ctor
		template<typename TFunc> FolderEntry(const QString &id, FolderIcon icon, const QString &text, TFunc filter);
		FolderEntry(const QString &id, FolderIcon icon, const QString &text, const std::vector<FolderEntry> &children);

		// accessors
		const QString &id() const { return m_id; }
		FolderIcon icon() const { return m_icon; }
		const QString &text() const { return m_text; }
		const std::vector<FolderEntry> *children() const { return m_children; }
		const std::function<bool(const info::machine &machine)> &filter() const { return m_filter; }

	private:
		FolderEntry(const QString &id, FolderIcon icon, const QString &text, std::function<bool(const info::machine &machine)> &&filter, const std::vector<FolderEntry> *children);

		// variables
		QString												m_id;
		FolderIcon											m_icon;
		QString												m_text;
		std::function<bool(const info::machine &machine)>	m_filter;
		const std::vector<FolderEntry> *					m_children;
	};

	typedef std::array<const char *, util::enum_count<FolderIcon>()> FolderIconResourceNameArray;

	info::database &							m_infoDb;
	Preferences &								m_prefs;
	std::array<RootFolderDesc, 19>				m_rootFolderList;
	std::vector<FolderEntry>					m_root;
	std::vector<FolderEntry>					m_bios;
	std::vector<FolderEntry>					m_cpu;
	std::vector<FolderEntry>					m_custom;
	std::vector<FolderEntry>					m_dumping;
	std::vector<FolderEntry>					m_manufacturer;
	std::vector<FolderEntry>					m_sound;
	std::vector<FolderEntry>					m_source;
	std::vector<FolderEntry>					m_year;
	std::array<QPixmap, util::enum_count<FolderIcon>()> m_folderIcons;

	static FolderIconResourceNameArray getFolderIconResourceNames();
	static const FolderEntry &folderEntryFromModelIndex(const QModelIndex &index);
	const std::vector<FolderEntry> &childFolderEntriesFromModelIndex(const QModelIndex &parent) const;
	void populateVariableFolders();
};


#endif // MACHINEFOLDERTREEMODEL_H
