/***************************************************************************

	softwarelist.cpp

	Support for software lists

***************************************************************************/

#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/filename.h>

#include "softwarelist.h"
#include "prefs.h"
#include "xmlparser.h"
#include "validity.h"


//**************************************************************************
//  SOFTWARE LIST
//**************************************************************************

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
		part &p = util::last(m_software).m_parts.emplace_back();
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

	for (const software_list::software &sw : softlist.get_software())
	{
		for (const software_list::part &part : sw.m_parts)
		{
			if (part.m_interface.empty() || part.m_name.empty())
				throw false;
		}
	}
}


//**************************************************************************
//  SOFTWARE LIST COLLECTION
//**************************************************************************

//-------------------------------------------------
//  software_list_collection::load
//-------------------------------------------------

void software_list_collection::load(const Preferences &prefs, info::machine machine)
{
	m_software_lists.clear();
	std::vector<wxString> hash_paths = prefs.GetSplitPaths(Preferences::global_path_type::HASH);
	for (const info::software_list softlist_info : machine.software_lists())
	{
		std::optional<software_list> softlist = software_list::try_load(hash_paths, softlist_info.name());
		if (softlist.has_value())
			m_software_lists.push_back(std::move(softlist.value()));
	}
}


//-------------------------------------------------
//  software_list_collection::find_software_by_name
//-------------------------------------------------
 
const software_list::software *software_list_collection::find_software_by_name(const wxString &name, const wxString &dev_interface) const
{
	// local function to determine if there is a special character
	auto has_special_character = [&name]()
	{
		return std::find_if(name.cbegin(), name.cend(), [](wxChar ch)
		{
			return ch == '.' || ch == ':' || ch == '/' || ch == '\\';
		}) != name.cend();
	};

	if (!name.empty() && !has_special_character())
	{
		for (const software_list &swlist : software_lists())
		{
			const software_list::software *sw = util::find_if_ptr(swlist.get_software(), [&name, &dev_interface](const software_list::software &sw)
			{
				return sw.m_name == name
					&& (dev_interface.empty() || util::find_if_ptr(sw.m_parts, [&dev_interface](const software_list::part &x)
					{
						return x.m_interface == dev_interface;
					}));
			});
			if (sw)
				return sw;
		}
	}
	return nullptr;
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	software_list::test
};
