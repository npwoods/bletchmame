/***************************************************************************

    prefs.h

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#pragma once

#ifndef PREFS_H
#define PREFS_H

#include <wx/string.h>
#include <wx/gdicmn.h>
#include <array>
#include <ostream>
#include <map>

class Preferences
{
public:
	enum class path_type
	{
		emu_exectuable,
		roms,
		samples,
		config,
		nvram,
		hash,
		artwork,
		plugins,
		profiles,

		count
	};

	enum class machine_path_type
	{
		working_directory,
		last_save_state,

		count
	};

	static const int COLUMN_COUNT = 4;

	Preferences();
	Preferences(const Preferences &) = delete;
	Preferences(Preferences &&) = delete;

	const wxString &GetPath(path_type type) const			{ return m_paths[static_cast<size_t>(type)]; }
	void SetPath(path_type type, wxString &&path)			{ m_paths[static_cast<size_t>(type)] = std::move(path); }
	
	wxString GetPathWithSubstitutions(path_type type) const { assert(type != path_type::emu_exectuable); return ApplySubstitutions(GetPath(type)); }

	const wxString &GetMameExtraArguments() const			{ return m_mame_extra_arguments; }
	void SetMameExtraArguments(wxString &&extra_arguments)	{ m_mame_extra_arguments = std::move(extra_arguments); }

	const wxSize &GetSize() const                           { return m_size; }
	void SetSize(const wxSize &size)                        { m_size = size; }

	int GetColumnWidth(int column_index) const              { return m_column_widths[column_index]; }
	void SetColumnWidth(int column_index, int width)        { m_column_widths[column_index] = width; }

	wxArrayInt GetColumnOrder() const						{ return wxArrayInt(m_column_order.begin(), m_column_order.end()); }
	void SetColumnOrder(int column_index, int order)		{ m_column_order[column_index] = order; }
	void SetColumnOrder(const std::array<int, 4> &order)	{ m_column_order = order; }

	const wxString &GetSelectedMachine() const              { return m_selected_machine; }
	void SetSelectedMachine(const wxString &machine_name)   { m_selected_machine = machine_name; }
	void SetSelectedMachine(wxString &&machine_name)        { m_selected_machine = std::move(machine_name); }

	const wxString &GetSearchBoxText() const				{ return m_search_box_text; }
	void SetSearchBoxText(wxString &&search_box_text)		{ m_search_box_text = std::move(search_box_text); }

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

	std::array<wxString, static_cast<size_t>(path_type::count)>	m_paths;
	wxString													m_mame_extra_arguments;
	wxSize														m_size;
	std::array<int, COLUMN_COUNT>								m_column_widths;
	std::array<int, COLUMN_COUNT>								m_column_order;
	std::map<wxString, MachineInfo>								m_machine_info;
	wxString													m_selected_machine;
	wxString													m_search_box_text;
	bool														m_menu_bar_shown;

	void Save(std::ostream &output);
	wxString GetFileName(bool ensure_directory_exists);
	const MachineInfo *GetMachineInfo(const wxString &machine_name) const;
};

#endif // PREFS_H
