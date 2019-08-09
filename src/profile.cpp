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


//-------------------------------------------------
//  image::operator==
//-------------------------------------------------

bool profiles::image::operator==(const image &that) const
{
	return m_tag == that.m_tag
		|| m_path == that.m_path;
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
		|| m_path == that.m_path
		|| m_images == that.m_images;
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
//  create
//-------------------------------------------------

void profiles::profile::create(wxTextOutputStream &stream, const info::machine &machine)
{
	stream << "<!-- BletchMAME profile -->" << endl;
	stream << "<profile machine=\"" << machine.name() << "\">" << endl;
	stream << "</profile>" << endl;
}
