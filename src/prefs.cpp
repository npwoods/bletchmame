/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/dir.h>
#include <wx/utils.h> 

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
	"nvram",
	"hash",
	"artwork",
	"plugins"
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

static wxString GetDefaultPluginsDirectory();


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
	SetPath(path_type::plugins, GetDefaultPluginsDirectory());

    Load();
}


//-------------------------------------------------
//  GetMachineInfo
//-------------------------------------------------

const Preferences::MachineInfo *Preferences::GetMachineInfo(const wxString &machine_name) const
{
	const auto iter = m_machine_info.find(machine_name);
	return iter != m_machine_info.end()
		? &iter->second
		: nullptr;
}


//-------------------------------------------------
//  GetMachinePath
//-------------------------------------------------

const wxString &Preferences::GetMachinePath(const wxString &machine_name, machine_path_type path_type) const
{
	// find the machine path entry
	const MachineInfo *info = GetMachineInfo(machine_name);
	if (!info)
		return util::g_empty_string;

	switch (path_type)
	{
	case machine_path_type::working_directory:
		return info->m_working_directory;
	case machine_path_type::last_save_state:
		return info->m_last_save_state;
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
		m_machine_info[machine_name].m_working_directory = std::move(path);
		break;
	case machine_path_type::last_save_state:
		m_machine_info[machine_name].m_last_save_state = std::move(path);
		break;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  GetRecentDeviceFiles
//-------------------------------------------------

std::vector<wxString> &Preferences::GetRecentDeviceFiles(const wxString &machine_name, const wxString &device_type)
{
	return m_machine_info[machine_name].m_recent_device_files[device_type];
}


const std::vector<wxString> &Preferences::GetRecentDeviceFiles(const wxString &machine_name, const wxString &device_type) const
{
	static const std::vector<wxString> empty_vector;
	const MachineInfo *info = GetMachineInfo(machine_name);
	if (!info)
		return empty_vector;

	auto iter = info->m_recent_device_files.find(device_type);
	if (iter == info->m_recent_device_files.end())
		return empty_vector;

	return iter->second;
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
	wxString current_machine_name;
	wxString current_device_type;

	// clear out state
	m_machine_info.clear();

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
	xml.OnElementEnd({ "preferences", "searchboxtext" }, [&](wxString &&content)
	{
		SetSearchBoxText(std::move(content));
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
		if (!attributes.Get("name", current_machine_name))
			return XmlParser::element_result::SKIP;

		wxString path;
		if (attributes.Get("working_directory", path))
			SetMachinePath(current_machine_name, machine_path_type::working_directory, std::move(path));
		if (attributes.Get("last_save_state", path))
			SetMachinePath(current_machine_name, machine_path_type::last_save_state, std::move(path));
		return XmlParser::element_result::OK;
	});
	xml.OnElementBegin({ "preferences", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		return attributes.Get("type", current_device_type)
			? XmlParser::element_result::OK
			: XmlParser::element_result::SKIP;
	});
	xml.OnElementEnd({ "preferences", "machine", "device", "recentfile" }, [&](wxString &&content)
	{
		GetRecentDeviceFiles(current_machine_name, current_device_type).push_back(std::move(content));
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
	if (!m_search_box_text.IsEmpty())
		output << "\t<searchboxtext>" << m_search_box_text.ToStdString() << "</searchboxtext>" << std::endl;
	for (size_t i = 0; i < m_column_widths.size(); i++)
		output << "\t<column id=\"" << s_column_ids[i] << "\" width=\"" << m_column_widths[i] << "\" order=\"" << m_column_order[i] << "\"/>" << std::endl;
	output << std::endl;

	output << "\t<!-- Machines -->" << std::endl;
	for (const auto &[machine_name, info] : m_machine_info)
	{
		if (!machine_name.empty() && (!info.m_working_directory.empty() || !info.m_last_save_state.empty() || !info.m_recent_device_files.empty()))
		{
			output << "\t<machine name=\"" << XmlParser::Escape(machine_name) << "\"";
			
			if (!info.m_working_directory.empty())
				output << " working_directory=\"" << XmlParser::Escape(info.m_working_directory) << "\"";
			if (!info.m_last_save_state.empty())
				output << " last_save_state=\"" << XmlParser::Escape(info.m_last_save_state) << "\"";

			if (info.m_recent_device_files.empty())
			{
				output << "/>" << std::endl;
			}
			else
			{
				output << ">" << std::endl;
				for (const auto &[device_type, recents] : info.m_recent_device_files)
				{
					output << "\t\t<device type=\"" << XmlParser::Escape(device_type) << "\">" << std::endl;
					for (const wxString &recent : recents)
						output << "\t\t\t<recentfile>" << recent.ToStdWstring() << "</recentfile>" << std::endl;
					output << "\t\t</device>" << std::endl;
				}
				output << "\t</machine>" << std::endl;
			}
		}
	}
	output << std::endl;

	output << "</preferences>" << std::endl;
}


//-------------------------------------------------
//  InternalApplySubstitutions
//-------------------------------------------------

template<typename TStr, typename TFunc>
static TStr InternalApplySubstitutions(const TStr &src, TFunc func)
{
	wxString result;
	result.reserve(src.size() + 100);

	enum class parse_state
	{
		NORMAL,
		AFTER_DOLLAR_SIGN,
		IN_VARIABLE_NAME
	};
	parse_state state = parse_state::NORMAL;
	typename TStr::const_iterator var_begin_iter = src.cbegin();

	for (typename TStr::const_iterator iter = src.cbegin(); iter < src.cend(); iter++)
	{
		wchar_t ch = *iter;
		switch (ch)
		{
		case '$':
			if (state == parse_state::NORMAL)
				state = parse_state::AFTER_DOLLAR_SIGN;
			break;

		case '(':
			if (state == parse_state::AFTER_DOLLAR_SIGN)
			{
				state = parse_state::IN_VARIABLE_NAME;
				var_begin_iter = iter + 1;
			}
			break;

		case ')':
			{
				wxString var_name(var_begin_iter, iter);
				TStr var_value = func(var_name);
				result += var_value;
				state = parse_state::NORMAL;
			}
			break;

		default:
			if (state == parse_state::NORMAL)
				result += *iter;
		}
	}
	return result;
}


//-------------------------------------------------
//  ApplySubstitutions
//-------------------------------------------------

wxString Preferences::ApplySubstitutions(const wxString &path) const
{

	return InternalApplySubstitutions(path, [this](const wxString &var_name)
	{
		wxString result;
		if (var_name == wxT("MAMEPATH"))
		{
			const wxString &path = GetPath(Preferences::path_type::emu_exectuable);
			wxFileName::SplitPath(path, &result, nullptr, nullptr);
		}
		else if (var_name == wxT("BLETCHMAMEPATH"))
		{
			wxFileName filename(wxStandardPaths::Get().GetExecutablePath());
			result = filename.GetPath();
		}
		return result;
	});
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


//-------------------------------------------------
//  GetDefaultPluginsDirectory
//-------------------------------------------------

static wxString GetDefaultPluginsDirectory()
{
	wxString path_separator(wxFileName::GetPathSeparator());
	wxString plugins(wxT("plugins"));

	return wxString("$(BLETCHMAMEPATH)") + path_separator + plugins
		+ wxString(";") + wxString("$(MAMEPATH)") + path_separator + plugins;
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

static void general()
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
//  path_names
//-------------------------------------------------

static void path_names()
{
	auto iter = std::find(s_path_names.begin(), s_path_names.end(), nullptr);
	assert(iter == s_path_names.end());
	(void)iter;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	general,
	path_names
};
