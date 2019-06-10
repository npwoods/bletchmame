/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include <fstream>
#include <functional>

#include "prefs.h"
#include "utility.h"
#include "xmlparser.h"


//**************************************************************************
//  LOCAL VARIABLES
//**************************************************************************

static std::array<const char *, static_cast<size_t>(Preferences::path_type::count)>	s_path_names =
{
	"emu",
	"roms",
	"samples",
	"config",
	"nvram"
};


static std::array<const char *, Preferences::COLUMN_COUNT> s_column_ids =
{
    "name",
    "description",
    "year",
    "manufacturer"
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ValidateDimension
//-------------------------------------------------

static bool IsValidDimension(int dimension)
{
    // arbitrary validation of dimensions
    return dimension >= 10 && dimension <= 20000;
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

Preferences::Preferences()
	: m_size(950, 600)
	, m_column_widths({85, 370, 50, 320})
	, m_menu_bar_shown(true)
{
	// Defaults
	for (int i = 0; i < COLUMN_COUNT; i++)
		SetColumnOrder(i, i);
	SetPath(path_type::config, GetConfigDirectory(true));
	SetPath(path_type::nvram, GetConfigDirectory(true));

    Load();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

Preferences::~Preferences()
{
    Save();
}


//-------------------------------------------------
//  Load
//-------------------------------------------------

bool Preferences::Load()
{
	using namespace std::placeholders;

	wxString file_name = GetFileName(false);

	// first check to see if the file exists
	if (!wxFileExists(file_name))
		return false;

	XmlParser xml;
	path_type type = path_type::count;
	std::array<int, COLUMN_COUNT> column_order;
	xml.OnElement({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		bool menu_bar_shown;
		if (attributes.Get("menu_bar_shown", menu_bar_shown))
			SetMenuBarShown(menu_bar_shown);
	});
	xml.OnElement({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		wxString type_string;
		if (attributes.Get("type", type_string))
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), type_string);
			type = iter != s_path_names.cend()
				? static_cast<path_type>(iter - s_path_names.cbegin())
				: path_type::count;
		}
	});
	xml.OnElement({ "preferences", "path" }, [&](wxString &&content)
	{
		if (type < path_type::count)
			SetPath(type, std::move(content));
		type = path_type::count;
	});
	xml.OnElement({ "preferences", "mameextraarguments" }, [&](wxString &&content)
	{
		SetMameExtraArguments(std::move(content));
	});
	xml.OnElement({ "preferences", "size" }, [&](const XmlParser::Attributes &attributes)
	{
		int width, height;
		if (attributes.Get("width", width) && attributes.Get("height", height) && IsValidDimension(width) && IsValidDimension(height))
		{
			wxSize size;
			size.SetWidth(width);
			size.SetHeight(height);
			SetSize(size);
		}
	});
	xml.OnElement({ "preferences", "selectedmachine" }, [&](wxString &&content)
	{
		SetSelectedMachine(std::move(content));
	});
	xml.OnElement({ "preferences", "column" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string id;
		if (attributes.Get("id", id))
		{
			auto iter = std::find(s_column_ids.begin(), s_column_ids.end(), id); 
			if (iter != s_column_ids.end())
			{
				int index = iter - s_column_ids.begin();
				int width, order;

				if (attributes.Get("width", width) && IsValidDimension(width))
					SetColumnWidth(index, width);
				if (attributes.Get("order", order) && order >= 0 && order < column_order.size())
					column_order[index] = order;
			}
		}
	});
	bool success = xml.Parse(file_name);

	// we're merging in the column order here because we want to make sure that
	// the columns are specified if and only if each column is present
	int column_order_mask = 0;
	for (size_t i = 0; i < column_order.size(); i++)
		column_order_mask |= 1 << i;
	if (column_order_mask == (1 << column_order.size()) - 1)
		SetColumnOrder(column_order);

	return success;
}


//-------------------------------------------------
//  Save
//-------------------------------------------------

void Preferences::Save()
{
	wxString file_name = GetFileName(true);
	std::ofstream output(file_name.ToStdString(), std::ios_base::out);
	Save(output);
}


//-------------------------------------------------
//  Save
//-------------------------------------------------

void Preferences::Save(std::ostream &output)
{
	output << "<!-- Preferences for BletchMAME -->" << std::endl;
	output << "<preferences menu_bar_shown=\"" << (m_menu_bar_shown ? "1" : "0") << "\">" << std::endl;
	output << std::endl;

	output << "\t<!-- Paths -->" << std::endl;
	for (size_t i = 0; i < m_paths.size(); i++)
		output << "\t<path type=\"" << s_path_names[i] << "\">" << GetPath(static_cast<path_type>(i)) << "</path>" << std::endl;
	output << std::endl;

	output << "\t<!-- Other -->" << std::endl;
	if (!m_mame_extra_arguments.IsEmpty())
		output << "\t<mameextraarguments>" << m_mame_extra_arguments << "</mameextraarguments>" << std::endl;
	output << "\t<size width=\"" << m_size.GetWidth() << "\" height=\"" << m_size.GetHeight() << "\"/>" << std::endl;
	if (!m_selected_machine.IsEmpty())
		output << "\t<selectedmachine>" << m_selected_machine.ToStdString() << "</selectedmachine>" << std::endl;
	for (size_t i = 0; i < m_column_widths.size(); i++)
		output << "\t<column id=\"" << s_column_ids[i] << "\" width=\"" << m_column_widths[i] << "\" order=\"" << m_column_order[i] << "\"/>" << std::endl;
	output << std::endl;

	output << "</preferences>" << std::endl;
}


//-------------------------------------------------
//  GetFileName
//-------------------------------------------------

wxString Preferences::GetFileName(bool ensure_directory_exists)
{
	wxString directory = GetConfigDirectory(ensure_directory_exists);
	wxFileName file_name(directory, "BletchMAME.xml");
	return file_name.GetFullPath();
}


//-------------------------------------------------
//  GetConfigDirectory - gets the configuration
//	directory, and optionally ensuring it exists
//-------------------------------------------------

wxString Preferences::GetConfigDirectory(bool ensure_directory_exists)
{
	// this is currently a thin wrapper on GetUserDataDir(), but hypothetically
	// we might want a command line option to override this directory
	wxString directory = wxStandardPaths::Get().GetUserDataDir();

	// if appropriate, ensure the directory exists
	if (ensure_directory_exists && !wxDir::Exists(directory))
		wxDir::Make(directory);

	return directory;
}
