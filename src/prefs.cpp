/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/dir.h>
#include <fstream>
#include <functional>

#include "prefs.h"
#include "utility.h"
#include "xmlparser.h"
#include "validity.h"


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
//  GetMachinePath
//-------------------------------------------------

const wxString &Preferences::GetMachinePath(const wxString &machine_name, machine_path_type path_type) const
{
	// find the machine path entry
	static const wxString empty_string;
	auto iter = m_machine_paths.find(machine_name);
	if (iter == m_machine_paths.end())
		return empty_string;

	switch (path_type)
	{
	case machine_path_type::working_directory:
		return iter->second.m_working_directory;
	case machine_path_type::last_save_state:
		return iter->second.m_last_save_state;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  SetMachinePath
//-------------------------------------------------

void Preferences::SetMachinePath(const wxString &machine_name, machine_path_type path_type, wxString &&path)
{
	switch (path_type)
	{
	case machine_path_type::working_directory:
		m_machine_paths[machine_name].m_working_directory = std::move(path);
		break;
	case machine_path_type::last_save_state:
		m_machine_paths[machine_name].m_last_save_state = std::move(path);
		break;
	default:
		throw false;
	}
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

	wxFileInputStream input(file_name);
	return Load(input);
}


//-------------------------------------------------
//  Load
//-------------------------------------------------

bool Preferences::Load(wxInputStream &input)
{
	XmlParser xml;
	path_type type = path_type::count;
	std::array<int, COLUMN_COUNT> column_order = { 0, };

	xml.OnElementBegin({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		bool menu_bar_shown;
		if (attributes.Get("menu_bar_shown", menu_bar_shown))
			SetMenuBarShown(menu_bar_shown);
	});
	xml.OnElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
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
	xml.OnElementEnd({ "preferences", "path" }, [&](wxString &&content)
	{
		if (type < path_type::count)
			SetPath(type, std::move(content));
		type = path_type::count;
	});
	xml.OnElementEnd({ "preferences", "mameextraarguments" }, [&](wxString &&content)
	{
		SetMameExtraArguments(std::move(content));
	});
	xml.OnElementBegin({ "preferences", "size" }, [&](const XmlParser::Attributes &attributes)
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
	xml.OnElementEnd({ "preferences", "selectedmachine" }, [&](wxString &&content)
	{
		SetSelectedMachine(std::move(content));
	});
	xml.OnElementBegin({ "preferences", "column" }, [&](const XmlParser::Attributes &attributes)
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
				if (attributes.Get("order", order) && order >= 0 && order < (int)column_order.size())
					column_order[index] = order;
			}
		}
	});
	xml.OnElementBegin({ "preferences", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		wxString name;
		if (attributes.Get("name", name))
		{
			wxString path;
			if (attributes.Get("working_directory", path))
				SetMachinePath(name, machine_path_type::working_directory, std::move(path));
			if (attributes.Get("last_save_state", path))
				SetMachinePath(name, machine_path_type::last_save_state, std::move(path));
		}
	});
	bool success = xml.Parse(input);

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

	output << "\t<!-- Miscellaneous -->" << std::endl;
	if (!m_mame_extra_arguments.IsEmpty())
		output << "\t<mameextraarguments>" << m_mame_extra_arguments << "</mameextraarguments>" << std::endl;
	output << "\t<size width=\"" << m_size.GetWidth() << "\" height=\"" << m_size.GetHeight() << "\"/>" << std::endl;
	if (!m_selected_machine.IsEmpty())
		output << "\t<selectedmachine>" << m_selected_machine.ToStdString() << "</selectedmachine>" << std::endl;
	for (size_t i = 0; i < m_column_widths.size(); i++)
		output << "\t<column id=\"" << s_column_ids[i] << "\" width=\"" << m_column_widths[i] << "\" order=\"" << m_column_order[i] << "\"/>" << std::endl;
	output << std::endl;

	output << "\t<!-- Machines -->" << std::endl;
	for (const auto &pair : m_machine_paths)
	{
		if (!pair.first.IsEmpty() && (!pair.second.m_working_directory.IsEmpty() || !pair.second.m_last_save_state.IsEmpty()))
		{
			output << "\t<machine name=\"" << XmlParser::Escape(pair.first) << "\"";
			
			if (!pair.second.m_working_directory.IsEmpty())
				output << " working_directory=\"" << XmlParser::Escape(pair.second.m_working_directory) << "\"";
			if (!pair.second.m_last_save_state.IsEmpty())
				output << " last_save_state=\"" << XmlParser::Escape(pair.second.m_last_save_state) << "\"";
			output << "/>" << std::endl;
		}
	}
	output << std::endl;

	output << "</preferences>" << std::endl;
}


//-------------------------------------------------
//  GetMameXmlDatabasePath
//-------------------------------------------------

wxString Preferences::GetMameXmlDatabasePath(bool ensure_directory_exists) const
{
	// get the configuration directory
	wxString config_dir = GetConfigDirectory(ensure_directory_exists);
	if (config_dir.IsEmpty())
		return "";

	// get the MAME path
	const wxString &mame_path = GetPath(Preferences::path_type::emu_exectuable);
	if (mame_path.IsEmpty())
		return "";

	// parse out the MAME filename
	wxString mame_filename;
	wxFileName::SplitPath(mame_path, nullptr, &mame_filename, nullptr);

	// get the full name
	wxFileName db_filename(config_dir, mame_filename, "infodb");
	return db_filename.GetFullPath();
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


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

static void test()
{
	const char *xml =
		"<preferences menu_bar_shown=\"1\">"
			"<path type=\"emu\">C:\\mame64.exe</path>"
			"<path type=\"roms\">C:\\roms</path>"
			"<path type=\"samples\">C:\\samples</path>"
			"<path type=\"config\">C:\\cfg</path>"
			"<path type=\"nvram\">C:\\nvram</path>"

			"<size width=\"1230\" height=\"765\"/>"
			"<selectedmachine>nes</selectedmachine>"
			"<column id=\"name\" width=\"84\" order=\"0\" />"
			"<column id=\"description\" width=\"165\" order=\"1\" />"
			"<column id=\"year\" width=\"50\" order=\"2\" />"
			"<column id=\"manufacturer\" width=\"320\" order=\"3\" />"

			"<machine name=\"echo\" working_directory=\"C:\\MyEchoGames\" last_save_state=\"C:\\MyLastState.sta\" />"
		"</preferences>";

	wxMemoryInputStream input(xml, strlen(xml));
	Preferences prefs;
	prefs.Load(input);

	assert(prefs.GetPath(Preferences::path_type::emu_exectuable)	== "C:\\mame64.exe");
	assert(prefs.GetPath(Preferences::path_type::roms)				== "C:\\roms");
	assert(prefs.GetPath(Preferences::path_type::samples)			== "C:\\samples");
	assert(prefs.GetPath(Preferences::path_type::config)			== "C:\\cfg");
	assert(prefs.GetPath(Preferences::path_type::nvram)				== "C:\\nvram");

	assert(prefs.GetMachinePath("echo", Preferences::machine_path_type::working_directory)		== "C:\\MyEchoGames");
	assert(prefs.GetMachinePath("echo", Preferences::machine_path_type::last_save_state)		== "C:\\MyLastState.sta");
	assert(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::working_directory)	== "");
	assert(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::last_save_state)		== "");
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test
};
