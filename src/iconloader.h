/***************************************************************************

	iconloader.h

	Icon management

***************************************************************************/

#pragma once

#ifndef ICONLOADER_H
#define ICONLOADER_H

#include <memory>
#include <unordered_map>
#include <wx/imaglist.h>

class Preferences;

class IconLoader
{
public:
	// ctor
	IconLoader(Preferences &prefs);

	// accessors
	wxImageList &ImageList() { return m_image_list; }

	// methods
	void RefreshIcons();
	int GetIcon(const wxString &icon_name);

private:
	class IconPathEntry
	{
	public:
		typedef std::unique_ptr<IconPathEntry> ptr;

		virtual ~IconPathEntry() { };
		virtual std::unique_ptr<wxInputStream> OpenFile(const wxString &filename) = 0;
	};

	class DirIconPathEntry;
	class ZipIconPathEntry;

	Preferences &						m_prefs;
	wxImageList							m_image_list;
	std::unordered_map<wxString, int>	m_icon_map;
	std::vector<IconPathEntry::ptr>		m_path_entries;

	IconPathEntry::ptr CreateDirIconPathEntry(const wxString &directory_name);
	IconPathEntry::ptr CreateZipIconPathEntry(const wxString &zip_file_name);

	int LoadIcon(wxString &&icon_name, wxInputStream &stream);
};

#endif // ICONLOADER_H
