/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

#include <wx/icon.h>

#include "iconloader.h"
#include "prefs.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define ICON_SIZE_X 16
#define ICON_SIZE_Y 16


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

IconLoader::IconLoader(Preferences &prefs)
	: m_prefs(prefs)
	, m_image_list(ICON_SIZE_X, ICON_SIZE_Y)
{
	RefreshIcons();
}


//-------------------------------------------------
//  RefreshIcons
//-------------------------------------------------

void IconLoader::RefreshIcons()
{
	// clear ourselves out
	m_image_list.RemoveAll();
	m_icon_map.clear();
	m_zip_files.clear();

	// loop through all icon paths
	std::vector<wxString> paths = m_prefs.GetSplitPaths(Preferences::path_type::icons);
	for (const wxString &path : paths)
	{
		if (wxFile::Exists(path))
			LoadIconsFromZipFile(path);
	}
}


//-------------------------------------------------
//  LoadIconsFromZipFile
//-------------------------------------------------

bool IconLoader::LoadIconsFromZipFile(const wxString &zip_file_name)
{
	// filter to sanity check ZIP file entries
	auto filter = [](const wxZipEntry &entry)
	{
		return entry.GetName().EndsWith(wxT(".ico"))
			&& entry.GetSize() > 0
			&& entry.GetSize() < 1000000;
	};

	// open up the zip file
	ZipFile::ptr zip = std::make_unique<ZipFile>(zip_file_name, filter);
	if (zip->EntryCount() <= 0)
		return false;

	// success - push this back
	m_zip_files.push_back(std::move(zip));
	return true;
}


//-------------------------------------------------
//  GetIcon
//-------------------------------------------------

int IconLoader::GetIcon(const wxString &icon_name)
{
	int result = -1;
	auto iter = m_icon_map.find(icon_name);
	if (iter != m_icon_map.end())
	{
		// we've previously loaded (or tried to load) this icon; use our old result
		result = iter->second;
	}
	else
	{
		// we have not tried to load this icon - first determine the real file name
		wxString icon_file_name = icon_name + wxT(".ico");

		// and try to load it from each ZIP file
		for (const auto &zip_file : m_zip_files)
		{
			wxInputStream *stream = zip_file->OpenFile(icon_file_name);
			if (stream)
			{
				// we've found an entry - try to load the icon; note that while this can
				// fail, we want to memoize the failure
				result = LoadIcon(std::move(icon_file_name), *stream);

				// record the result in the icon map
				m_icon_map.emplace(icon_name, result);
				break;
			}
		}
	}
	return result;
}


//-------------------------------------------------
//  LoadIcon
//-------------------------------------------------

int IconLoader::LoadIcon(wxString &&icon_name, wxInputStream &stream)
{
	// load the icon into a file
	wxImage image;
	if (!image.LoadFile(stream, wxBITMAP_TYPE_ICO))
		return -1;

	// rescale it
	wxImage rescaled_image = image.Rescale(ICON_SIZE_X, ICON_SIZE_Y);
	
	// and add it to the image list
	int icon_number = m_image_list.Add(rescaled_image);
	if (icon_number < 0)
		return -1;

	// we have an icon!  add it to the map
	m_icon_map.emplace(std::move(icon_name), icon_number);
	return icon_number;
}


//-------------------------------------------------
//  ZipFile ctor
//-------------------------------------------------

template<class TFilter>
IconLoader::ZipFile::ZipFile(const wxString &zip_filename, TFilter filter)
	: m_file_stream(zip_filename)
	, m_zip_stream(m_file_stream)
{
	if (m_file_stream.IsOk() && m_zip_stream.IsOk())
	{
		wxZipEntry *entry;
		while ((entry = m_zip_stream.GetNextEntry()) != nullptr)
		{
			if (!entry->IsDir() && filter(*entry))
				m_entries.emplace(entry->GetName(), *entry);
		}
	}
}


//-------------------------------------------------
//  ZipFile::EntryCount
//-------------------------------------------------

size_t IconLoader::ZipFile::EntryCount() const
{
	return m_entries.size();
}


//-------------------------------------------------
//  ZipFile::OpenFile
//-------------------------------------------------

wxInputStream *IconLoader::ZipFile::OpenFile(const wxString &filename)
{
	// find the entry
	auto iter = m_entries.find(filename);

	// if we found it, try to open it
	return iter != m_entries.end() && m_zip_stream.OpenEntry(iter->second)
		? &m_zip_stream
		: nullptr;
}
