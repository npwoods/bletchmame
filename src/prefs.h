/***************************************************************************

    prefs.h

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#pragma once

#ifndef PREFS_H
#define PREFS_H

#include <wx/string.h>
#include <wx/gdicmn.h>
#include <wx/xml/xml.h>
#include <array>
#include <ostream>

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

		count
	};

	Preferences();
	Preferences(const Preferences &) = delete;
	Preferences(Preferences &&) = delete;
	~Preferences();

	const wxString &GetPath(path_type type) const			{ return m_paths[static_cast<size_t>(type)]; }
	void SetPath(path_type type, wxString &&mame_path)		{ m_paths[static_cast<size_t>(type)] = std::move(mame_path); }

	const wxString &GetMameExtraArguments() const			{ return m_mame_extra_arguments; }
	void SetMameExtraArguments(wxString &&extra_arguments)	{ m_mame_extra_arguments = std::move(extra_arguments); }

	const wxSize &GetSize() const                           { return m_size; }
	void SetSize(const wxSize &size)                        { m_size = size; }

	int GetColumnWidth(int column_index) const              { return m_column_widths[column_index]; }
	void SetColumnWidth(int column_index, int width)        { m_column_widths[column_index] = width; }

	const wxString &GetSelectedMachine() const              { return m_selected_machine; }
	void SetSelectedMachine(const wxString &machine_name)   { m_selected_machine = machine_name; }
	void SetSelectedMachine(wxString &&machine_name)        { m_selected_machine = std::move(machine_name); }

	static wxString GetConfigDirectory(bool ensure_directory_exists = false);

private:
	std::array<wxString, static_cast<size_t>(path_type::count)>			m_paths;
	wxString															m_mame_extra_arguments;
	wxSize																m_size;
	std::array<int, 4>													m_column_widths;
	wxString															m_selected_machine;

	bool Load();
	void Save();
	void Save(std::ostream &output);
	void ProcessXmlCallback(const std::vector<wxString> &path, const wxXmlNode &node);
	wxString GetFileName(bool ensure_directory_exists);
};

#endif // PREFS_H
