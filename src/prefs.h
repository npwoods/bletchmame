/***************************************************************************

    prefs.h

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#pragma once

#ifndef PREFS_H
#define PREFS_H

#include <array>
#include <ostream>
#include <map>
#include <set>

#include <QDataStream>
#include <QSize>

#include "utility.h"


struct ColumnPrefs
{
	int								m_width;
	int								m_order;
	std::optional<Qt::SortOrder>	m_sort;
};


struct FolderPrefs
{
	FolderPrefs();
	bool operator==(const FolderPrefs &that) const = default;

	bool							m_shown;
};


class Preferences
{
public:
	class Test;

	// paths that are "global"
	enum class global_path_type
	{
		EMU_EXECUTABLE,
		ROMS,
		SAMPLES,
		CONFIG,
		NVRAM,
		HASH,
		ARTWORK,
		ICONS,
		PLUGINS,
		PROFILES,
		CHEATS,
		SNAPSHOTS,

		COUNT
	};

	// paths that are per-machine
	enum class machine_path_type
	{
		WORKING_DIRECTORY,
		LAST_SAVE_STATE,

		COUNT
	};

	// is this path type for a file, a directory, or multiple directories?
	enum class PathCategory
	{
		SingleFile,
		SingleDirectory,
		MultipleDirectories,
		MultipleDirectoriesOrArchives
	};

	// we have three different list views, each with a tab; this identifies them
	enum class list_view_type
	{
		MACHINE,
		SOFTWARELIST,
		PROFILE,

		COUNT
	};

	Preferences();
	Preferences(const Preferences &) = delete;
	Preferences(Preferences &&) = delete;

	static PathCategory getPathCategory(global_path_type path_type);
	static PathCategory getPathCategory(machine_path_type path_type);
	static void ensureDirectoryPathsHaveFinalPathSeparator(PathCategory category, QString &path);

	const QString &getGlobalPath(global_path_type type) const											{ return m_paths[static_cast<size_t>(type)]; }
	void setGlobalPath(global_path_type type, QString &&path);
	void setGlobalPath(global_path_type type, const QString &path);
	
	QStringList getSplitPaths(global_path_type type) const;

	QString getGlobalPathWithSubstitutions(global_path_type type) const;

	const QString &getMameExtraArguments() const														{ return m_mame_extra_arguments; }
	void setMameExtraArguments(QString &&extra_arguments)												{ m_mame_extra_arguments = std::move(extra_arguments); }

	const QSize &setSize() const											 							{ return m_size; }
	void setSize(const QSize &size)																		{ m_size = size; }

	list_view_type getSelectedTab()																		{ return m_selected_tab; }
	void setSelectedTab(list_view_type type)															{ m_selected_tab = type; }

	const QString &getMachineFolderTreeSelection() const												{ return m_machine_folder_tree_selection; }
	void setMachineFolderTreeSelection(QString &&selection)												{ m_machine_folder_tree_selection = std::move(selection); }

	const QList<int> &getMachineSplitterSizes() const													{ return m_machine_splitter_sizes; }
	void setMachineSplitterSizes(QList<int> &&sizes)													{ m_machine_splitter_sizes = std::move(sizes); }

	FolderPrefs getFolderPrefs(const QString &folder) const;
	void setFolderPrefs(const QString &folder, FolderPrefs &&prefs);

	const std::map<QString, std::set<QString>> &getCustomFolders() const								{ return m_custom_folders; }
	std::map<QString, std::set<QString>> &getCustomFolders()											{ return m_custom_folders; }

	const std::unordered_map<std::u8string, ColumnPrefs> &getColumnPrefs(const char8_t *view_type)			{ return m_column_prefs[view_type]; }
	void setColumnPrefs(const char8_t *view_type, std::unordered_map<std::u8string, ColumnPrefs> &&prefs)	{ m_column_prefs[view_type]  = std::move(prefs); }

	const QString &getListViewSelection(const char8_t *view_type, const QString &softlist_name) const;
	void setListViewSelection(const char8_t *view_type, const QString &softlist_name, QString &&selection);
	void setListViewSelection(const char8_t *view_type, QString &&selection)								{ setListViewSelection(view_type, util::g_empty_string, std::move(selection)); }

	const QString &getSearchBoxText(const char8_t *view_type) const										{ return m_list_view_filter[view_type]; }
	void setSearchBoxText(const char8_t *view_type, QString &&search_box_text)							{ m_list_view_filter[view_type] = std::move(search_box_text); }

	bool getMenuBarShown() const																		{ return m_menu_bar_shown; }
	void setMenuBarShown(bool menu_bar_shown)															{ m_menu_bar_shown = menu_bar_shown; }

	const QString &getMachinePath(const QString &machine_name, machine_path_type path_type) const;
	void setMachinePath(const QString &machine_name, machine_path_type path_type, QString &&path);

	std::vector<QString> &getRecentDeviceFiles(const QString &machine_name, const QString &device_type);
	const std::vector<QString> &getRecentDeviceFiles(const QString &machine_name, const QString &device_type) const;

	QString getMameXmlDatabasePath(bool ensure_directory_exists = true) const;
	QString applySubstitutions(const QString &path) const;
	static QString internalApplySubstitutions(const QString &src, std::function<QString(const QString &)> func);

	static QString getConfigDirectory(bool ensure_directory_exists = false);

	bool load();
	bool load(QIODevice &input);
	void save();

private:
	struct MachineInfo
	{
		MachineInfo();
		~MachineInfo();
		MachineInfo(const MachineInfo &) = delete;
		MachineInfo(MachineInfo &&) = default;

		bool operator==(const MachineInfo &) const = default;

		QString										m_working_directory;
		QString										m_last_save_state;
		std::map<QString, std::vector<QString>>     m_recent_device_files;
	};

	static std::array<const char *, static_cast<size_t>(Preferences::global_path_type::COUNT)>	s_path_names;

	std::array<QString, static_cast<size_t>(global_path_type::COUNT)>							m_paths;
	QString                                                                 					m_mame_extra_arguments;
	QSize																						m_size;
	mutable std::unordered_map<std::u8string, std::unordered_map<std::u8string, ColumnPrefs>>	m_column_prefs;
	std::map<QString, MachineInfo>																m_machine_info;
	list_view_type																				m_selected_tab;
	QString																						m_machine_folder_tree_selection;
	QList<int>																					m_machine_splitter_sizes;
	std::map<QString, FolderPrefs>																m_folder_prefs;
	std::map<QString, std::set<QString>>														m_custom_folders;
	std::unordered_map<QString, QString>														m_list_view_selection;
	mutable std::unordered_map<QString, QString>												m_list_view_filter;
	bool																						m_menu_bar_shown;

	void save(QIODevice &output);
	QString getFileName(bool ensure_directory_exists);
	const MachineInfo *getMachineInfo(const QString &machine_name) const;
};

#endif // PREFS_H
