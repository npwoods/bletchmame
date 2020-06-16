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

#include <QDataStream>
#include <QSize>

#include "utility.h"


struct ColumnPrefs
{
	enum class sort_type
	{
		ASCENDING,
		DESCENDING
	};

	int							m_width;
	int							m_order;
	std::optional<sort_type>	m_sort;
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
	enum class path_category
	{
		FILE,
		SINGLE_DIRECTORY,
		MULTIPLE_DIRECTORIES
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

	static path_category GetPathCategory(global_path_type path_type);
	static path_category GetPathCategory(machine_path_type path_type);
    static void EnsureDirectoryPathsHaveFinalPathSeparator(path_category category, QString &path);

    const QString &GetGlobalPath(global_path_type type) const									{ return m_paths[static_cast<size_t>(type)]; }
	void SetGlobalPath(global_path_type type, QString &&path);
	void SetGlobalPath(global_path_type type, const QString &path);
	
    std::vector<QString> GetSplitPaths(global_path_type type) const;

    QString GetGlobalPathWithSubstitutions(global_path_type type) const;

    const QString &GetMameExtraArguments() const												{ return m_mame_extra_arguments; }
    void SetMameExtraArguments(QString &&extra_arguments)										{ m_mame_extra_arguments = std::move(extra_arguments); }

	const QSize &GetSize() const											 					{ return m_size; }
	void SetSize(const QSize &size)																{ m_size = size; }

	list_view_type GetSelectedTab()																		{ return m_selected_tab; }
	void SetSelectedTab(list_view_type type)															{ m_selected_tab = type; }

	const std::unordered_map<std::string, ColumnPrefs> &GetColumnPrefs(const char *view_type)			{ return m_column_prefs[view_type]; }
	void SetColumnPrefs(const char *view_type, std::unordered_map<std::string, ColumnPrefs> &&prefs)	{ m_column_prefs[view_type]  = std::move(prefs); }

    const QString &GetListViewSelection(const char *view_type, const QString &softlist_name) const;
	void SetListViewSelection(const char *view_type, const QString &softlist_name, QString &&selection);
	void SetListViewSelection(const char *view_type, QString &&selection)						{ SetListViewSelection(view_type, util::g_empty_string, std::move(selection)); }

    const QString &GetSearchBoxText(const char *view_type) const								{ return m_list_view_filter[view_type]; }
	void SetSearchBoxText(const char *view_type, QString &&search_box_text)					{ m_list_view_filter[view_type] = std::move(search_box_text); }

	bool GetMenuBarShown() const							{ return m_menu_bar_shown; }
	void SetMenuBarShown(bool menu_bar_shown)				{ m_menu_bar_shown = menu_bar_shown; }

    const QString &GetMachinePath(const QString &machine_name, machine_path_type path_type) const;
    void SetMachinePath(const QString &machine_name, machine_path_type path_type, QString &&path);

    std::vector<QString> &GetRecentDeviceFiles(const QString &machine_name, const QString &device_type);
    const std::vector<QString> &GetRecentDeviceFiles(const QString &machine_name, const QString &device_type) const;

    QString GetMameXmlDatabasePath(bool ensure_directory_exists = true) const;
    QString ApplySubstitutions(const QString &path) const;
	static QString InternalApplySubstitutions(const QString &src, std::function<QString(const QString &)> func);

    static QString GetConfigDirectory(bool ensure_directory_exists = false);

	bool Load();
	bool Load(QDataStream &input);
	void Save();

private:
	struct MachineInfo
	{
		QString									m_working_directory;
		QString									m_last_save_state;
        std::map<QString, std::vector<QString>>     m_recent_device_files;
	};

	static std::array<const char *, static_cast<size_t>(Preferences::global_path_type::COUNT)>	Preferences::s_path_names;

    std::array<QString, static_cast<size_t>(global_path_type::COUNT)>						m_paths;
    QString                                                                 				m_mame_extra_arguments;
	QSize																					m_size;
	mutable std::unordered_map<std::string, std::unordered_map<std::string, ColumnPrefs>>	m_column_prefs;
	std::map<QString, MachineInfo>															m_machine_info;
	list_view_type																			m_selected_tab;
    std::unordered_map<QString, QString>													m_list_view_selection;
	mutable std::unordered_map<QString, QString>											m_list_view_filter;
	bool																					m_menu_bar_shown;

	void Save(std::ostream &output);
    QString GetFileName(bool ensure_directory_exists);
    const MachineInfo *GetMachineInfo(const QString &machine_name) const;
};

#endif // PREFS_H
