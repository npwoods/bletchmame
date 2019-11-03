/***************************************************************************

	profile.cpp

	Code for representing profiles

***************************************************************************/

#include <wx/dir.h>
#include <wx/wfstream.h>
#include <wx/filename.h>
#include <wx/txtstrm.h>
#include <optional>

#include "profile.h"
#include "xmlparser.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  image::operator==
//-------------------------------------------------

bool profiles::image::operator==(const image &that) const
{
	return m_tag == that.m_tag
		&& m_path == that.m_path;
}


//-------------------------------------------------
//  image::is_valid
//-------------------------------------------------

bool profiles::image::is_valid() const
{
	return !m_tag.empty() && !m_path.empty();
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

profiles::profile::profile()
{
}


//-------------------------------------------------
//  operator==
//-------------------------------------------------

bool profiles::profile::operator==(const profile &that) const
{
	return m_name == that.m_name
		&& m_software == that.m_software
		&& m_path == that.m_path
		&& m_images == that.m_images;
}


//-------------------------------------------------
//  is_valid
//-------------------------------------------------

bool profiles::profile::is_valid() const
{
	return !m_name.empty()
		&& !m_path.empty()
		&& !m_machine.empty()
		&& std::find_if(m_images.begin(), m_images.end(), [](const image &x) { return !x.is_valid(); }) == m_images.end();
}


//-------------------------------------------------
//  scan_directory
//-------------------------------------------------

std::vector<profiles::profile> profiles::profile::scan_directories(const std::vector<wxString> &paths)
{
	std::vector<profile> results;

	for (const wxString &path : paths)
	{
		wxArrayString files;
		wxDir::GetAllFiles(path, &files, wxT("*.bletchmameprofile"));
		for (wxString &file : files)
		{
			std::optional<profile> p = load(std::move(file));
			if (p)
				results.push_back(std::move(p.value()));
		}
	}
	return results;
}


//-------------------------------------------------
//  load
//-------------------------------------------------

std::optional<profiles::profile> profiles::profile::load(const wxString &path)
{
	return load(wxString(path));
}


//-------------------------------------------------
//  load
//-------------------------------------------------

std::optional<profiles::profile> profiles::profile::load(wxString &&path)
{
	// open the stream
	wxFileInputStream stream(path);

	// start setting up the profile
	profile result;
	result.m_path = std::move(path);
	wxFileName::SplitPath(result.m_path, nullptr, nullptr, &result.m_name, nullptr, wxPathFormat::wxPATH_NATIVE);

	// and parse the XML
	XmlParser xml;
	xml.OnElementBegin({ "profile" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("machine", result.m_machine);
		attributes.Get("software", result.m_software);
	});
	xml.OnElementBegin({ "profile", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &i = result.m_images.emplace_back();
		attributes.Get("tag",	i.m_tag);
		attributes.Get("path",	i.m_path);
	});

	return xml.Parse(result.m_path) && result.is_valid()
		? std::optional<profiles::profile>(std::move(result))
		: std::optional<profiles::profile>();
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void profiles::profile::save() const
{
	wxFileOutputStream stream(path());
	wxTextOutputStream text_stream(stream);
	save_as(text_stream);
}


//-------------------------------------------------
//  save_as
//-------------------------------------------------

void profiles::profile::save_as(wxTextOutputStream &stream) const
{
	stream << "<!-- BletchMAME profile -->" << endl;
	stream << "<profile";
	stream << " machine=\"" << machine() << "\"";
	if (!software().empty())
		stream << " software=\"" << software() << "\"";
	stream << ">" << endl;

	for (const image &image : images())
		stream << "\t<image tag=\"" << image.m_tag << "\" path=\"" << image.m_path << "\"/>" << endl;
	stream << "</profile>" << endl;
}


//-------------------------------------------------
//  create
//-------------------------------------------------

void profiles::profile::create(wxTextOutputStream &stream, const info::machine &machine, const software_list::software *software)
{
	profile new_profile;
	new_profile.m_machine = machine.name();
	if (software)
		new_profile.m_software = software->m_name;
	new_profile.save_as(stream);
}


//**************************************************************************
//  UTILITY
//**************************************************************************

//-------------------------------------------------
//  change_path_save_state
//-------------------------------------------------

wxString profiles::profile::change_path_save_state(const wxString &path)
{
	wxFileName file_name(path);
	file_name.SetExt("sta");
	return file_name.GetFullPath();
}


//-------------------------------------------------
//  profile_file_rename
//-------------------------------------------------

bool profiles::profile::profile_file_rename(const wxString &old_path, const wxString &new_path)
{
	bool success;

	if (new_path == old_path)
	{
		// if the new path is equal to the old path, renaming is easy - its a no-op
		success = true;
	}
	else
	{
		// this is a crude multi file renaming; unfortunately there is not a robust way to do this
		success = wxRenameFile(old_path, new_path, false);
		if (success)
		{
			wxString old_save_state_path = change_path_save_state(old_path);
			wxString new_save_state_path = change_path_save_state(new_path);
			if (wxFile::Exists(old_save_state_path))
				wxRenameFile(old_save_state_path, new_save_state_path, true);
		}
	}
	return success;
}


//-------------------------------------------------
//  profile_file_remove
//-------------------------------------------------

bool profiles::profile::profile_file_remove(const wxString &path)
{
	// this is also crude
	bool success = wxRemoveFile(path);
	if (success)
	{
		wxString save_state_path = change_path_save_state(path);
		if (wxFile::Exists(save_state_path))
			wxRemoveFile(save_state_path);
	}
	return success;
}
