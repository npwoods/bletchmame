/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

#include <wx/icon.h>
#include <wx/zipstrm.h>
#include <wx/wfstream.h>

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
	// first open the file
	wxFileInputStream input(zip_file_name);
	if (!input.IsOk())
		return false;

	// then open as a zip
	wxZipInputStream zip(input);
	if (!zip.IsOk())
		return false;

	// ok now we have a real zip file; get down to business
	wxZipEntry *entry;
	const wxString icon_extension = wxT(".ico");

	// get the next entry
	while ((entry = zip.GetNextEntry()) != nullptr)
	{
		// sanity check the entry
		if (!entry->IsDir() && entry->GetSize() > 0 && entry->GetSize() < 1000000)
		{
			// is this a *.ico file?
			wxString name = entry->GetName();
			if (name.EndsWith(icon_extension))
			{
				// if so, get the real name
				name.resize(name.size() - icon_extension.size());

				// open up the ZIP entry
				zip.OpenEntry(*entry);

				// and load it!
				LoadIcon(std::move(name), zip);
			}
		}
	}
	return true;
}


//-------------------------------------------------
//  LoadIcon
//-------------------------------------------------

bool IconLoader::LoadIcon(wxString &&icon_name, wxInputStream &stream)
{
	// load the icon into a file
	wxImage image;
	if (!image.LoadFile(stream, wxBITMAP_TYPE_ICO))
		return false;

	// rescale it
	wxImage rescaled_image = image.Rescale(ICON_SIZE_X, ICON_SIZE_Y);
	
	// and add it to the image list
	int icon_number = m_image_list.Add(rescaled_image);
	if (icon_number < 0)
		return false;

	// we have an icon!  add it to the map
	m_icon_map.emplace(std::move(icon_name), icon_number);
	return true;
}


//-------------------------------------------------
//  GetIcon
//-------------------------------------------------

int IconLoader::GetIcon(const wxString &icon_name)
{
	auto iter = m_icon_map.find(icon_name);
	return iter != m_icon_map.end()
		? iter->second
		: -1;
}
