/***************************************************************************

	profile.h

	Code for representing profiles

***************************************************************************/

#pragma once

#ifndef PROFILE_H
#define PROFILE_H

#include <wx/string.h>
#include <vector>

#include "info.h"

namespace profiles
{
	struct image
	{
		wxString		m_tag;
		wxString		m_path;

		bool operator==(const image &that) const;
		bool is_valid() const;
	};

	class profile
	{
	public:
		profile(profile &&) = default;
		profile &operator =(profile &&) = default;

		// accessors
		const wxString &name() const { return m_name; }
		const wxString &path() const { return m_path; }
		const wxString &machine() const { return m_machine; }
		const std::vector<image> &images() const { return m_images; }

		// methods
		bool is_valid() const;

		// statics
		static std::vector<profile> scan_directories(const std::vector<wxString> &paths);
		static void create(wxTextOutputStream &stream, const info::machine &machine);

		bool operator==(const profile &that) const;

	private:
		profile();
		profile(const profile &) = delete;
		profile &operator =(const profile &) = delete;

		wxString			m_name;
		wxString			m_path;
		wxString			m_machine;
		std::vector<image>	m_images;

		static std::optional<profile> load(wxString &&path);
	};
};

#endif // PROFILE_H
