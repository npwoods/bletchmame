/***************************************************************************

    prefs.h

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#pragma once

#ifndef PREFS_H
#define PREFS_H

#include <wx/string.h>
#include <wx/gdicmn.h>
#include <wx/stream.h>
#include <array>
#include <ostream>
#include <map>

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

	const wxString &GetGlobalPath(global_path_type type) const									{ return m_paths[static_cast<size_t>(type)]; }
	void SetGlobalPath(global_path_type type, wxString &&path);
	
	std::vector<wxString> GetSplitPaths(global_path_type type) const;

	wxString GetGlobalPathWithSubstitutions(global_path_type type) const;

	const wxString &GetMameExtraArguments() const												{ return m_mame_extra_arguments; }
	void SetMameExtraArguments(wxString &&extra_arguments)										{ m_mame_extra_arguments = std::move(extra_arguments); }

	const wxSize &GetSize() const											 					{ return m_size; }
	void SetSize(const wxSize &size)															{ m_size = size; }

	list_view_type GetSelectedTab()																		{ return m_selected_tab; }
	void SetSelectedTab(list_view_type type)															{ m_selected_tab = type; }

	const std::unordered_map<std::string, ColumnPrefs> &GetColumnPrefs(const char *view_type)			{ return m_column_prefs[view_type]; }
	void SetColumnPrefs(const char *view_type, std::unordered_map<std::string, ColumnPrefs> &&prefs)	{ m_column_prefs[view_type]  = std::move(prefs); }

	const wxString &GetListViewSelection(const char *view_type, const wxString &softlist_name) const;
	void SetListViewSelection(const char *view_type, const wxString &softlist_name, wxString &&selection);
	void SetListViewSelection(const char *view_type, wxString &&selection)						{ SetListViewSelection(view_type, util::g_empty_string, std::move(selection)); }

	const wxString &GetSearchBoxText(const char *view_type) const								{ return m_list_view_filter[view_type]; }
	void SetSearchBoxText(const char *view_type, wxString &&search_box_text)					{ m_list_view_filter[view_type] = std::move(search_box_text); }

	bool GetMenuBarShown() const							{ return m_menu_bar_shown; }
	void SetMenuBarShown(bool menu_bar_shown)				{ m_menu_bar_shown = menu_bar_shown; }

	const wxString &GetMachinePath(const wxString &machine_name, machine_path_type path_type) const;
	void SetMachinePath(const wxString &machine_name, machine_path_type path_type, wxString &&path);

	std::vector<wxString> &GetRecentDeviceFiles(const wxString &machine_name, const wxString &device_type);
	const std::vector<wxString> &GetRecentDeviceFiles(const wxString &machine_name, const wxString &device_type) const;

	wxString GetMameXmlDatabasePath(bool ensure_directory_exists = true) const;
	wxString ApplySubstitutions(const wxString &path) const;

	static wxString GetConfigDirectory(bool ensure_directory_exists = false);

	bool Load();
	bool Load(wxInputStream &input);
	void Save();

private:
	struct MachineInfo
	{
		wxString									m_working_directory;
		wxString									m_last_save_state;
		std::map<wxString, std::vector<wxString>>	m_recent_device_files;
	};

	std::array<wxString, static_cast<size_t>(global_path_type::COUNT)>						m_paths;
	wxString																				m_mame_extra_arguments;
	wxSize																					m_size;
	mutable std::unordered_map<std::string, std::unordered_map<std::string, ColumnPrefs>>	m_column_prefs;
	std::map<wxString, MachineInfo>															m_machine_info;
	list_view_type																			m_selected_tab;
	std::unordered_map<wxString, wxString>													m_list_view_selection;
	mutable std::unordered_map<wxString, wxString>											m_list_view_filter;
	bool																					m_menu_bar_shown;

	void Save(std::ostream &output);
	wxString GetFileName(bool ensure_directory_exists);
	const MachineInfo *GetMachineInfo(const wxString &machine_name) const;
};

#endif // PREFS_H
