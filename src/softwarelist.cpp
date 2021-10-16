/***************************************************************************

	softwarelist.cpp

	Support for software lists

***************************************************************************/

#include "softwarelist.h"
#include "prefs.h"
#include "xmlparser.h"


//**************************************************************************
//  SOFTWARE LIST
//**************************************************************************

//-------------------------------------------------
//  load
//-------------------------------------------------

bool software_list::load(QIODevice &stream, QString &error_message)
{
	XmlParser xml;
	std::string current_device_extensions;
	xml.onElementBegin({ "softwarelist" }, [this](const XmlParser::Attributes &attributes)
	{
		m_name			= attributes.get<QString>("name").value_or("");
		m_description	= attributes.get<QString>("description").value_or("");
	});
	xml.onElementBegin({ "softwarelist", "software" }, [this](const XmlParser::Attributes &attributes)
	{
		software &s = m_software.emplace_back();
		s.m_name		= attributes.get<QString>("name").value_or("");
		s.m_parts.reserve(16);
	});
	xml.onElementEnd({ "softwarelist", "software" }, [this](QString &&)
	{
		util::last(m_software).m_parts.shrink_to_fit();
	});
	xml.onElementEnd({ "softwarelist", "software", "description" }, [this](QString &&content)
	{
		util::last(m_software).m_description = std::move(content);
	});
	xml.onElementEnd({ "softwarelist", "software", "year" }, [this](QString &&content)
	{
		util::last(m_software).m_year = std::move(content);
	});
	xml.onElementEnd({ "softwarelist", "software", "publisher" }, [this](QString &&content)
	{
		util::last(m_software).m_publisher = std::move(content);
	});
	xml.onElementBegin({ "softwarelist", "software", "part" }, [this](const XmlParser::Attributes &attributes)
	{
		software &s		= util::last(m_software);
		part &p			= s.m_parts.emplace_back();
		p.m_name		= attributes.get<QString>("name").value_or("");
		p.m_interface	= attributes.get<QString>("interface").value_or("");
	});
	xml.onElementBegin({ "softwarelist", "software", "part", "dataarea" }, [this](const XmlParser::Attributes &attributes)
	{
		software &s		= util::last(m_software);
		part &p			= util::last(s.m_parts);
		dataarea &a		= p.m_dataareas.emplace_back();
		a.m_name		= attributes.get<QString>("name").value_or("");
	});
	xml.onElementBegin({ "softwarelist", "software", "part", "dataarea", "rom" }, [this](const XmlParser::Attributes &attributes)
	{
		software &s		= util::last(m_software);
		part &p			= util::last(s.m_parts);
		dataarea &a		= util::last(p.m_dataareas);
		rom &r			= a.m_roms.emplace_back();
		r.m_name		= attributes.get<QString>("name").value_or("");
		r.m_size		= attributes.get<std::uint64_t>("size");
		r.m_crc32		= attributes.get<std::uint32_t>("crc", 16);
		r.m_sha1		= util::fixedByteArrayFromHex<20>(attributes.get<std::u8string_view>("sha1"));
	});

	// parse the XML, but be bold and try to reserve lots of space
	m_software.reserve(4000);
	bool success = xml.parse(stream);
	m_software.shrink_to_fit();

	// did we succeed?
	if (!success)
	{
		error_message = xml.errorMessagesSingleString();
		return false;
	}

	error_message.clear();
	return true;
}


//-------------------------------------------------
//  try_load
//-------------------------------------------------

std::optional<software_list> software_list::try_load(const QStringList &hash_paths, const QString &softlist_name)
{
	for (const QString &path : hash_paths)
	{
		QString full_path = path + "/" + softlist_name + ".xml";
		QFile file(full_path);

		if (file.open(QIODevice::ReadOnly))
		{
			software_list softlist;
			QString error_message;

			if (softlist.load(file, error_message))
				return softlist;
		}
	}

	return { };
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
	QStringList hash_paths = prefs.getSplitPaths(Preferences::global_path_type::HASH);
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
 
const software_list::software *software_list_collection::find_software_by_name(const QString &name, const QString &dev_interface) const
{
	// local function to determine if there is a special character
	auto has_special_character = [&name]()
	{
		return std::find_if(name.cbegin(), name.cend(), [](QChar ch)
		{
			return ch == '.' || ch == ':' || ch == '/' || ch == '\\';
		}) != name.cend();
	};

	if (!name.isEmpty() && !has_special_character())
	{
		for (const software_list &swlist : software_lists())
		{
			const software_list::software *sw = util::find_if_ptr(swlist.get_software(), [&name, &dev_interface](const software_list::software &sw)
			{
				return sw.name() == name
					&& (dev_interface.isEmpty() || util::find_if_ptr(sw.parts(), [&dev_interface](const software_list::part &x)
					{
						return x.interface() == dev_interface;
					}));
			});
			if (sw)
				return sw;
		}
	}
	return nullptr;
}
