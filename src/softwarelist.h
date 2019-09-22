/***************************************************************************

	softwarelist.h

	Support for software lists

***************************************************************************/

#pragma once

#ifndef SOFTWARELIST_H
#define SOFTWARELIST_H

#include <wx/stream.h>
#include <optional>


// ======================> software_list
class software_list
{
public:
	software_list(const software_list &) = delete;
	software_list(software_list &&) = default;

	static std::optional<software_list> try_load(const std::vector<wxString> &hash_paths, const wxString &softlist_name);

	// validity checks
	static void test();

private:
	struct software_part
	{
		wxString					m_name;
		wxString					m_interface;
	};

	struct software
	{
		wxString					m_name;
		wxString					m_description;
		wxString					m_year;
		wxString					m_publisher;
		std::vector<software_part>	m_parts;
	};

	wxString						m_name;
	wxString						m_description;
	std::vector<software>			m_software;

	// ctor
	software_list() = default;

	// methods
	bool load(wxInputStream &stream, wxString &error_message);
};


#endif // SOFTWARELIST_H
