/***************************************************************************

	iconloader.cpp

	Icon management

***************************************************************************/

#include <wx/icon.h>
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/dir.h>

#include "iconloader.h"
#include "prefs.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define ICON_SIZE_X 16
#define ICON_SIZE_Y 16


//**************************************************************************
//  LOCAL CLASSES
//**************************************************************************

namespace
{
	class NonOwningInputStream : public wxInputStream
	{
	public:
		NonOwningInputStream(wxInputStream &inner)
			: m_inner(inner)
		{
		}

	protected:
		virtual size_t OnSysRead(void *buffer, size_t size) override
		{
			m_inner.Read(buffer, size);
			return m_inner.LastRead();
		}

	private:
		wxInputStream &m_inner;
	};
};


// ======================> IconLoader::DirIconPathEntry
class IconLoader::DirIconPathEntry : public IconLoader::IconPathEntry
{
public:
	DirIconPathEntry(const wxString &path);
	DirIconPathEntry(const ZipIconPathEntry &) = delete;
	DirIconPathEntry(ZipIconPathEntry &&) = delete;

	virtual std::unique_ptr<wxInputStream> OpenFile(const wxString &filename) override;

private:
	wxString		m_path;
};


// ======================> IconLoader::ZipIconPathEntry
class IconLoader::ZipIconPathEntry : public IconLoader::IconPathEntry
{
public:
	typedef std::unique_ptr<ZipIconPathEntry> ptr;

	// ctors
	ZipIconPathEntry(const wxString &zip_filename);
	ZipIconPathEntry(const ZipIconPathEntry &) = delete;
	ZipIconPathEntry(ZipIconPathEntry &&) = delete;

	size_t EntryCount() const;
	virtual std::unique_ptr<wxInputStream> OpenFile(const wxString &filename) override;

private:
	wxFileInputStream							m_file_stream;
	wxZipInputStream							m_zip_stream;
	std::unordered_map<wxString, wxZipEntry>	m_entries;
};


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
	m_path_entries.clear();

	// loop through all icon paths
	std::vector<wxString> paths = m_prefs.GetSplitPaths(Preferences::global_path_type::ICONS);
	for (const wxString &path : paths)
	{
		// try to create an appropiate path entry
		IconPathEntry::ptr path_entry;
		if (wxDir::Exists(path))
			path_entry = CreateDirIconPathEntry(path);
		else if (wxFile::Exists(path))
			path_entry = CreateZipIconPathEntry(path);

		// if successful, add it
		if (path_entry)
			m_path_entries.push_back(std::move(path_entry));
	}
}


//-------------------------------------------------
//  CreateDirIconPathEntry
//-------------------------------------------------

IconLoader::IconPathEntry::ptr IconLoader::CreateDirIconPathEntry(const wxString &directory_name)
{
	return std::make_unique<DirIconPathEntry>(directory_name);
}


//-------------------------------------------------
//  CreateZipIconPathEntry
//-------------------------------------------------

IconLoader::IconPathEntry::ptr IconLoader::CreateZipIconPathEntry(const wxString &zip_file_name)
{
	// open up the zip file
	ZipIconPathEntry::ptr result = std::make_unique<ZipIconPathEntry>(zip_file_name);
	if (result->EntryCount() <= 0)
		result.reset();

	// return it
	return result;
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
		for (const auto &path_entry : m_path_entries)
		{
			std::unique_ptr<wxInputStream> stream = path_entry->OpenFile(icon_file_name);
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
//  DirIconPathEntry ctor
//-------------------------------------------------

IconLoader::DirIconPathEntry::DirIconPathEntry(const wxString &path)
	: m_path(path)
{
}


//-------------------------------------------------
//  DirIconPathEntry::OpenFile
//-------------------------------------------------

std::unique_ptr<wxInputStream> IconLoader::DirIconPathEntry::OpenFile(const wxString &filename)
{
	std::unique_ptr<wxInputStream> result;

	// does the file exist?
	wxString full_filename = m_path + wxT("\\") + filename;
	if (wxFile::Exists(full_filename))
	{
		// if so, open it
		result = std::make_unique<wxFileInputStream>(full_filename);

		// but close it if it is bad
		if (!result->IsOk())
			result.reset();
	}
	return result;
}


//-------------------------------------------------
//  ZipIconPathEntry ctor
//-------------------------------------------------

IconLoader::ZipIconPathEntry::ZipIconPathEntry(const wxString &zip_filename)
	: m_file_stream(zip_filename)
	, m_zip_stream(m_file_stream)
{
	if (m_file_stream.IsOk() && m_zip_stream.IsOk())
	{
		wxZipEntry *entry;
		while ((entry = m_zip_stream.GetNextEntry()) != nullptr)
		{
			if (!entry->IsDir()
				&& entry->GetName().EndsWith(wxT(".ico"))
				&& entry->GetSize() > 0
				&& entry->GetSize() < 1000000)
			{
				m_entries.emplace(entry->GetName(), *entry);
			}
		}
	}
}


//-------------------------------------------------
//  ZipIconPathEntry::EntryCount
//-------------------------------------------------

size_t IconLoader::ZipIconPathEntry::EntryCount() const
{
	return m_entries.size();
}


//-------------------------------------------------
//  ZipIconPathEntry::OpenFile
//-------------------------------------------------

std::unique_ptr<wxInputStream> IconLoader::ZipIconPathEntry::OpenFile(const wxString &filename)
{
	// find the entry
	auto iter = m_entries.find(filename);

	// if we found it, try to open it
	std::unique_ptr<wxInputStream> result;
	if (iter != m_entries.end() && m_zip_stream.OpenEntry(iter->second))
		result = std::make_unique<NonOwningInputStream>(m_zip_stream);
	return result;
}
