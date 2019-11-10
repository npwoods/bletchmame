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
#include <wx/zipstrm.h>
#include <wx/wfstream.h>

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
	class ZipFile
	{
	public:
		typedef std::unique_ptr<ZipFile> ptr;

		// ctors
		template<class TFilter> ZipFile(const wxString &zip_filename, TFilter filter);
		ZipFile(const ZipFile &) = delete;
		ZipFile(ZipFile &&) = delete;

		// methods
		size_t EntryCount() const;
		wxInputStream *OpenFile(const wxString &filename);

	private:
		wxFileInputStream							m_file_stream;
		wxZipInputStream							m_zip_stream;
		std::unordered_map<wxString, wxZipEntry>	m_entries;
	};

	Preferences &						m_prefs;
	wxImageList							m_image_list;
	std::unordered_map<wxString, int>	m_icon_map;
	std::vector<ZipFile::ptr>			m_zip_files;

	bool LoadIconsFromZipFile(const wxString &zip_file_name);
	int LoadIcon(wxString &&icon_name, wxInputStream &stream);
};

#endif // ICONLOADER_H
