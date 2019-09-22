/***************************************************************************

	softwarelist.cpp

	Support for software lists

***************************************************************************/

#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/filename.h>

#include "softwarelist.h"
#include "xmlparser.h"
#include "validity.h"


//-------------------------------------------------
//  load
//-------------------------------------------------

bool software_list::load(wxInputStream &stream, wxString &error_message)
{
	XmlParser xml;
	std::string current_device_extensions;
	xml.OnElementBegin({ "softwarelist" }, [this](const XmlParser::Attributes &attributes)
	{
		attributes.Get("name", m_name);
		attributes.Get("description", m_description);
	});
	xml.OnElementBegin({ "softwarelist", "software" }, [this](const XmlParser::Attributes &attributes)
	{
		software &s = m_software.emplace_back();
		attributes.Get("name", s.m_name);
		s.m_parts.reserve(16);
	});
	xml.OnElementEnd({ "softwarelist", "software" }, [this](wxString &&)
	{
		util::last(m_software).m_parts.shrink_to_fit();
	});
	xml.OnElementEnd({ "softwarelist", "software", "description" }, [this](wxString &&content)
	{
		util::last(m_software).m_description = std::move(content);
	});
	xml.OnElementEnd({ "softwarelist", "software", "year" }, [this](wxString &&content)
	{
		util::last(m_software).m_year = std::move(content);
	});
	xml.OnElementEnd({ "softwarelist", "software", "publisher" }, [this](wxString &&content)
	{
		util::last(m_software).m_publisher = std::move(content);
	});
	xml.OnElementBegin({ "softwarelist", "software", "part" }, [this](const XmlParser::Attributes &attributes)
	{
		software_part &p = util::last(m_software).m_parts.emplace_back();
		attributes.Get("name", p.m_name);
		attributes.Get("interface", p.m_interface);
	});

	// parse the XML, but be bold and try to reserve lots of space
	m_software.reserve(4000);
	bool success = xml.Parse(stream);
	m_software.shrink_to_fit();

	// did we succeed?
	if (!success)
	{
		error_message = xml.ErrorMessage();
		return false;
	}

	error_message.clear();
	return true;
}


//-------------------------------------------------
//  try_load
//-------------------------------------------------

std::optional<software_list> software_list::try_load(const std::vector<wxString> &hash_paths, const wxString &softlist_name)
{
	for (const wxString &path : hash_paths)
	{
		wxString full_path = path + wxFileName::GetPathSeparator() + softlist_name + wxT(".xml");

		if (wxFile::Exists(full_path))
		{
			software_list softlist;
			wxString error_message;
			wxFileStream file_stream(full_path);

			if (file_stream.IsOk() && softlist.load(file_stream, error_message))
				return std::move(softlist);
		}
	}

	return { };
}


//-------------------------------------------------
//  test
//-------------------------------------------------

void software_list::test()
{
	// get the test asset
	std::optional<std::string_view> asset = load_test_asset("softlist");
	if (!asset.has_value())
		return;

	// try to load it
	wxMemoryInputStream stream(asset.value().data(), asset.value().size());
	software_list softlist;
	wxString error_message;
	bool success = softlist.load(stream, error_message);

	// did we succeed?
	if (!success || !error_message.empty())
		throw false;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	software_list::test
};
