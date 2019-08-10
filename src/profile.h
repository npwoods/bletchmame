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
		const wxString &name() const				{ return m_name; }
		const wxString &path() const				{ return m_path; }
		const wxString &machine() const				{ return m_machine; }
		const std::vector<image> &images() const	{ return m_images; }
		std::vector<image> &images()				{ return m_images; }
		bool auto_save_states() const				{ return true; }
		
		// methods
		bool is_valid() const;
		void save() const;
		void save_as(wxTextOutputStream &stream) const;

		wxString directory_path() const
		{
			wxString result;
			wxFileName::SplitPath(path(), &result, nullptr, nullptr);
			return result;
		}

		// statics
		static std::vector<profile> scan_directories(const std::vector<wxString> &paths);
		static void create(wxTextOutputStream &stream, const info::machine &machine);
		static std::optional<profile> load(wxString &&path);
		static std::optional<profile> load(const wxString &path);

		// utility
		static wxString change_path_save_state(const wxString &path);
		static bool profile_file_rename(const wxString &old_path, const wxString &new_path);
		static bool profile_file_remove(const wxString &path);

		bool operator==(const profile &that) const;

	private:
		profile();
		profile(const profile &) = delete;
		profile &operator =(const profile &) = delete;

		wxString			m_name;
		wxString			m_path;
		wxString			m_machine;
		std::vector<image>	m_images;
	};
};

#endif // PROFILE_H
