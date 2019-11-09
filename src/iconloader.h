/***************************************************************************

	iconloader.h

	Icon management

***************************************************************************/

#pragma once

#ifndef ICONLOADER_H
#define ICONLOADER_H

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
	Preferences &						m_prefs;
	wxImageList							m_image_list;
	std::unordered_map<wxString, int>	m_icon_map;

	bool LoadIconsFromZipFile(const wxString &zip_file_name);
	bool LoadIcon(wxString &&icon_name, wxInputStream &stream);
};

#endif // ICONLOADER_H
