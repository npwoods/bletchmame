/***************************************************************************

	softwarelist.cpp

	Support for software lists

***************************************************************************/

// bletchmame headers
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
		const auto [nameAttr, descriptionAttr] = attributes.get("name", "description");
		m_name			= nameAttr.as<QString>().value_or("");
		m_description	= nameAttr.as<QString>().value_or("");
	});
	xml.onElementBegin({ "softwarelist", "software" }, [this](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr] = attributes.get("name");
		software &s = m_software.emplace_back(*this);
		s.m_name		= nameAttr.as<QString>().value_or("");
		s.m_parts.reserve(16);
	});
	xml.onElementEnd({ "softwarelist", "software" }, [this]()
	{
		util::last(m_software).m_parts.shrink_to_fit();
	});
	xml.onElementEnd({ "softwarelist", "software", "description" }, [this](std::u8string &&content)
	{
		util::last(m_software).m_description = util::toQString(content);
	});
	xml.onElementEnd({ "softwarelist", "software", "year" }, [this](std::u8string &&content)
	{
		util::last(m_software).m_year = util::toQString(content);
	});
	xml.onElementEnd({ "softwarelist", "software", "publisher" }, [this](std::u8string &&content)
	{
		util::last(m_software).m_publisher = util::toQString(content);
	});
	xml.onElementBegin({ "softwarelist", "software", "part" }, [this](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, interfaceAttr] = attributes.get("name", "interface");
		software &s		= util::last(m_software);
		part &p			= s.m_parts.emplace_back();
		p.m_name		= nameAttr.as<QString>().value_or("");
		p.m_interface	= interfaceAttr.as<QString>().value_or("");
	});
	xml.onElementBegin({ "softwarelist", "software", "part", "dataarea" }, [this](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr] = attributes.get("name");
		software &s		= util::last(m_software);
		part &p			= util::last(s.m_parts);
		dataarea &a		= p.m_dataareas.emplace_back();
		a.m_name		= nameAttr.as<QString>().value_or("");
	});
	xml.onElementBegin({ "softwarelist", "software", "part", "dataarea", "rom" }, [this](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, sizeAttr, crcAttr, sha1Attr] = attributes.get("name", "size", "crc", "sha1");
		software &s		= util::last(m_software);
		part &p			= util::last(s.m_parts);
		dataarea &a		= util::last(p.m_dataareas);
		rom &r			= a.m_roms.emplace_back();
		r.m_name		= nameAttr.as<QString>().value_or("");
		r.m_size		= sizeAttr.as<std::uint64_t>();
		r.m_crc32		= crcAttr.as<std::uint32_t>(16);
		r.m_sha1		= util::fixedByteArrayFromHex<20>(sha1Attr.as<std::u8string_view>());
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

software_list::ptr software_list::try_load(const QStringList &hash_paths, const QString &softlist_name)
{
	for (const QString &path : hash_paths)
	{
		QString full_path = path + "/" + softlist_name + ".xml";
		QFile file(full_path);

		if (file.open(QIODevice::ReadOnly))
		{
			software_list::ptr softlist = std::make_unique<software_list>();

			QString error_message;
			if (softlist->load(file, error_message))
				return softlist;
		}
	}

	return { };
}


//-------------------------------------------------
//  software_list::software ctor
//-------------------------------------------------

software_list::software::software(const class software_list &parent)
	: m_parent(parent)
{
}


//-------------------------------------------------
//  software_list::software dtor
//-------------------------------------------------

software_list::software::~software()
{
	// seem to need to declare this explicitly to avoid what appears to be a compiler bug
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
		software_list::ptr softlist = software_list::try_load(hash_paths, softlist_info.name());
		if (softlist)
			m_software_lists.push_back(std::move(softlist));
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
		return std::ranges::find_if(name, [](QChar ch)
		{
			return ch == '.' || ch == ':' || ch == '/' || ch == '\\';
		}) != name.cend();
	};

	if (!name.isEmpty() && !has_special_character())
	{
		for (const software_list::ptr &swlist : software_lists())
		{
			const software_list::software *sw = util::find_if_ptr(swlist->get_software(), [&name, &dev_interface](const software_list::software &sw)
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



//-------------------------------------------------
//  software_list_collection::find_software_by_name
//-------------------------------------------------

const software_list::software *software_list_collection::find_software_by_list_and_name(const QString &softwareList, const QString &software) const
{
	// find the software list
	const software_list::ptr *swlist = util::find_if_ptr(software_lists().begin(), software_lists().end(), [&softwareList](const software_list::ptr &swlistptr)
	{
		return swlistptr->name() == softwareList;
	});
	if (!swlist)
		return nullptr;

	// find the software
	const software_list::software *sw = util::find_if_ptr((*swlist)->get_software(), [&software](const software_list::software &sw)
	{
		return sw.name() == software;
	});
	return sw;
}
