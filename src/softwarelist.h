/***************************************************************************

	softwarelist.h

	Support for software lists

***************************************************************************/

#pragma once

#ifndef SOFTWARELIST_H
#define SOFTWARELIST_H

#include <wx/stream.h>
#include <optional>

#include "utility.h"


// ======================> software_list
class software_list
{
public:
	struct software;

	struct part
	{
		wxString			m_name;
		wxString			m_interface;
	};

	struct software
	{
		wxString			m_name;
		wxString			m_description;
		wxString			m_year;
		wxString			m_publisher;
		std::vector<part>	m_parts;
	};


	software_list(const software_list &) = SHOULD_BE_DELETE;
	software_list(software_list &&) = default;

	// attempts to load a software list from a list of paths
	static std::optional<software_list> try_load(const std::vector<wxString> &hash_paths, const wxString &softlist_name);

	// accessors
	const std::vector<software> &get_software() const { return m_software; }

	// validity checks
	static void test();

private:
	wxString				m_name;
	wxString				m_description;
	std::vector<software>	m_software;

	// ctor
	software_list() = default;

	// methods
	bool load(wxInputStream &stream, wxString &error_message);
};


#endif // SOFTWARELIST_H
