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

static std::array<const char *, static_cast<size_t>(Preferences::global_path_type::COUNT)>	s_path_names =
{
	"emu",
	"roms",
	"samples",
	"config",
	"nvram",
	"hash",
	"artwork",
	"icons",
	"plugins",
	"profiles"
};


static const util::enum_parser_bidirectional<ColumnPrefs::sort_type> s_column_sort_type_parser =
{
	{ "ascending", ColumnPrefs::sort_type::ASCENDING, },
	{ "descending", ColumnPrefs::sort_type::DESCENDING }
};


static const util::enum_parser_bidirectional<Preferences::list_view_type> s_list_view_type_parser =
{
	{ "machine", Preferences::list_view_type::MACHINE, },
	{ "softwarelist", Preferences::list_view_type::SOFTWARELIST, },
	{ "profile", Preferences::list_view_type::PROFILE }
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
//  GetListViewSelectionKey
//-------------------------------------------------

static wxString GetListViewSelectionKey(const char *view_type, const wxString &softlist)
{
	return wxString(view_type) + wxString(1, '\0') + softlist;
}


//-------------------------------------------------
//  SplitListViewSelectionKey
//-------------------------------------------------

static std::tuple<const wxChar *, const wxChar *> SplitListViewSelectionKey(const wxString &key)
{
	// get the view type
	const wxChar *view_type = key.c_str();

	// get the machine key, if present
	std::size_t null_pos = key.find('\0');
	const wxChar *machine = key.size() > null_pos + 1
		? view_type + null_pos + 1
		: nullptr;

	// and return them as a tuple
	return std::make_tuple(view_type, machine);
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

Preferences::Preferences()
	: m_size(950, 600)
	, m_menu_bar_shown(true)
	, m_selected_tab(list_view_type::MACHINE)
{
	// default paths
	SetGlobalPath(global_path_type::CONFIG, GetConfigDirectory(true));
	SetGlobalPath(global_path_type::NVRAM, GetConfigDirectory(true));
	SetGlobalPath(global_path_type::PLUGINS, GetDefaultPluginsDirectory());
	SetGlobalPath(global_path_type::PROFILES, GetConfigDirectory(true) + "\\profiles");

    Load();
}


//-------------------------------------------------
//  GetPathCategory
//-------------------------------------------------

Preferences::path_category Preferences::GetPathCategory(global_path_type path_type)
{
	path_category result;
	switch (path_type)
	{
	case Preferences::global_path_type::EMU_EXECUTABLE:
		result = path_category::FILE;
		break;

	case Preferences::global_path_type::CONFIG:
	case Preferences::global_path_type::NVRAM:
		result = path_category::SINGLE_DIRECTORY;
		break;

	case Preferences::global_path_type::ROMS:
	case Preferences::global_path_type::SAMPLES:
	case Preferences::global_path_type::HASH:
	case Preferences::global_path_type::ARTWORK:
	case Preferences::global_path_type::PLUGINS:
	case Preferences::global_path_type::PROFILES:
	case Preferences::global_path_type::ICONS:
		result = path_category::MULTIPLE_DIRECTORIES;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  GetPathCategory
//-------------------------------------------------

Preferences::path_category Preferences::GetPathCategory(machine_path_type path_type)
{
	path_category result;
	switch (path_type)
	{
	case Preferences::machine_path_type::LAST_SAVE_STATE:
		result = path_category::FILE;
		break;

	case Preferences::machine_path_type::WORKING_DIRECTORY:
		result = path_category::SINGLE_DIRECTORY;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  EnsureDirectoryPathsHaveFinalPathSeparator
//-------------------------------------------------

void Preferences::EnsureDirectoryPathsHaveFinalPathSeparator(path_category category, wxString &path)
{
	if (category != path_category::FILE && !path.empty() && !wxFileName::IsPathSeparator(path[path.size() - 1]))
	{
		path += wxFileName::GetPathSeparator();
	}
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
//  SetGlobalPath
//-------------------------------------------------

void Preferences::SetGlobalPath(global_path_type type, wxString &&path)
{
	EnsureDirectoryPathsHaveFinalPathSeparator(GetPathCategory(type), path);
	m_paths[static_cast<size_t>(type)] = std::move(path);
}


//-------------------------------------------------
//  GetSplitPaths
//-------------------------------------------------

std::vector<wxString> Preferences::GetSplitPaths(global_path_type type) const
{
	const wxString &paths_string = GetGlobalPath(type);
	return util::string_split(paths_string, [](const wchar_t ch) { return ch == ';'; });
}


//-------------------------------------------------
//  GetGlobalPathWithSubstitutions
//-------------------------------------------------

wxString Preferences::GetGlobalPathWithSubstitutions(global_path_type type) const
{
	assert(GetPathCategory(type) != path_category::FILE);
	return ApplySubstitutions(GetGlobalPath(type));
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
	case machine_path_type::WORKING_DIRECTORY:
		return info->m_working_directory;
	case machine_path_type::LAST_SAVE_STATE:
		return info->m_last_save_state;
	default:
		throw false;
	}
}


//-------------------------------------------------
//  GetListViewSelection
//-------------------------------------------------

const wxString &Preferences::GetListViewSelection(const char *view_type, const wxString &machine_name) const
{
	wxString key = GetListViewSelectionKey(view_type, machine_name);
	auto iter = m_list_view_selection.find(key);
	return iter != m_list_view_selection.end()
		? iter->second
		: util::g_empty_string;
}


//-------------------------------------------------
//  SetListViewSelection
//-------------------------------------------------

void Preferences::SetListViewSelection(const char *view_type, const wxString &machine_name, wxString &&selection)
{
	wxString key = GetListViewSelectionKey(view_type, machine_name);
	m_list_view_selection[key] = std::move(selection);
}


//-------------------------------------------------
//  SetMachinePath
//-------------------------------------------------

void Preferences::SetMachinePath(const wxString &machine_name, machine_path_type path_type, wxString &&path)
{
	// ensure that if we have a path, it has a path separator at the end
	EnsureDirectoryPathsHaveFinalPathSeparator(GetPathCategory(path_type), path);

	switch (path_type)
	{
	case machine_path_type::WORKING_DIRECTORY:
		m_machine_info[machine_name].m_working_directory = std::move(path);
		break;
	case machine_path_type::LAST_SAVE_STATE:
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
	global_path_type type = global_path_type::COUNT;
	wxString current_machine_name;
	wxString current_device_type;
	wxString *current_list_view_parameter = nullptr;

	// clear out state
	m_machine_info.clear();

	xml.OnElementBegin({ "preferences" }, [&](const XmlParser::Attributes &attributes)
	{
		bool menu_bar_shown;
		if (attributes.Get("menu_bar_shown", menu_bar_shown))
			SetMenuBarShown(menu_bar_shown);

		list_view_type selected_tab;
		if (attributes.Get("selected_tab", selected_tab, s_list_view_type_parser))
			SetSelectedTab(selected_tab);

	});
	xml.OnElementBegin({ "preferences", "path" }, [&](const XmlParser::Attributes &attributes)
	{
		wxString type_string;
		if (attributes.Get("type", type_string))
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), type_string);
			type = iter != s_path_names.cend()
				? static_cast<global_path_type>(iter - s_path_names.cbegin())
				: global_path_type::COUNT;
		}
	});
	xml.OnElementEnd({ "preferences", "path" }, [&](wxString &&content)
	{
		if (type < global_path_type::COUNT)
			SetGlobalPath(type, std::move(content));
		type = global_path_type::COUNT;
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
	xml.OnElementBegin({ "preferences", "selection" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string list_view;
		std::string softlist;
		if (attributes.Get("view", list_view))
		{
			attributes.Get("softlist", softlist);
			wxString key = GetListViewSelectionKey(list_view.c_str(), softlist);
			current_list_view_parameter = &m_list_view_selection[key];
		}
	});
	xml.OnElementBegin({ "preferences", "searchboxtext" }, [&](const XmlParser::Attributes &attributes)
	{
		wxString list_view;
		if (!attributes.Get("view", list_view))
			list_view = "machine";
		current_list_view_parameter = &m_list_view_filter[list_view];
	});
	xml.OnElementEnd({{ "preferences", "selection" },
					  { "preferences", "searchboxtext" }}, [&](wxString &&content)
	{
		assert(current_list_view_parameter);
		*current_list_view_parameter = std::move(content);
		current_list_view_parameter = nullptr;
	});
	xml.OnElementBegin({ "preferences", "column" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string view_type, id;
		if (attributes.Get("type", view_type) && attributes.Get("id", id))
		{
			ColumnPrefs &col_prefs = m_column_prefs[view_type][id];
			attributes.Get("width", col_prefs.m_width);
			attributes.Get("order", col_prefs.m_order);
			attributes.Get("sort", col_prefs.m_sort, s_column_sort_type_parser);
		}
	});
	xml.OnElementBegin({ "preferences", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		if (!attributes.Get("name", current_machine_name))
			return XmlParser::element_result::SKIP;

		wxString path;
		if (attributes.Get("working_directory", path))
			SetMachinePath(current_machine_name, machine_path_type::WORKING_DIRECTORY, std::move(path));
		if (attributes.Get("last_save_state", path))
			SetMachinePath(current_machine_name, machine_path_type::LAST_SAVE_STATE, std::move(path));
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
	output << "<preferences menu_bar_shown=\"" << (m_menu_bar_shown ? "1" : "0") << "\" selected_tab=\"" << s_list_view_type_parser[GetSelectedTab()] << "\">" << std::endl;
	output << std::endl;

	output << "\t<!-- Paths -->" << std::endl;
	for (size_t i = 0; i < m_paths.size(); i++)
		output << "\t<path type=\"" << s_path_names[i] << "\">" << GetGlobalPath(static_cast<global_path_type>(i)) << "</path>" << std::endl;
	output << std::endl;

	output << "\t<!-- Miscellaneous -->" << std::endl;
	if (!m_mame_extra_arguments.IsEmpty())
		output << "\t<mameextraarguments>" << m_mame_extra_arguments << "</mameextraarguments>" << std::endl;
	output << "\t<size width=\"" << m_size.GetWidth() << "\" height=\"" << m_size.GetHeight() << "\"/>" << std::endl;

	for (const auto &pair : m_list_view_selection)
	{
		if (!pair.second.empty())
		{
			auto [view_type, softlist] = SplitListViewSelectionKey(pair.first);
			output << "\t<selection view=\"" << wxString(view_type);
			if (softlist)
				output << "\" softlist=\"" + wxString(softlist);
			output << "\">" << XmlParser::Escape(pair.second) << "</selection>" << std::endl;
		}
	}

	for (const auto &[view_type, text] : m_list_view_filter)
	{
		if (!text.empty())
			output << "\t<searchboxtext view=\"" << view_type << "\">" << text << "</searchboxtext>" << std::endl;
	}

	// column width/order
	for (const auto &view_prefs : m_column_prefs)
	{
		for (const auto &col_prefs : view_prefs.second)
		{
			output << "\t<column type=\"" << view_prefs.first << "\" id=\"" << col_prefs.first
				<< "\" width=\"" << col_prefs.second.m_width
				<< "\" order=\"" << col_prefs.second.m_order;

			if (col_prefs.second.m_sort.has_value())
				output << "\" sort=\"" << s_column_sort_type_parser[col_prefs.second.m_sort.value()];

			output << "\"/>" << std::endl;
		}
	}
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
		bool handled = false;
		wchar_t ch = *iter;
		switch (ch)
		{
		case '$':
			if (state == parse_state::NORMAL)
			{
				state = parse_state::AFTER_DOLLAR_SIGN;
				handled = true;
			}
			break;

		case '(':
			if (state == parse_state::AFTER_DOLLAR_SIGN)
			{
				state = parse_state::IN_VARIABLE_NAME;
				var_begin_iter = iter + 1;
				handled = true;
			}
			break;

		case ')':
			if (state == parse_state::IN_VARIABLE_NAME)
			{
				wxString var_name(var_begin_iter, iter);
				TStr var_value = func(var_name);
				result += var_value;
				state = parse_state::NORMAL;
				handled = true;
			}
			break;
		}

		// if it wasn't handled, append the character
		if (!handled && state == parse_state::NORMAL)
			result += *iter;
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
			const wxString &path = GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
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
	const wxString &mame_path = GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);
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

	assert(prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE)	== "C:\\mame64.exe");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::ROMS)				== "C:\\roms\\");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::SAMPLES)			== "C:\\samples\\");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::CONFIG)			== "C:\\cfg\\");
	assert(prefs.GetGlobalPath(Preferences::global_path_type::NVRAM)			== "C:\\nvram\\");

	assert(prefs.GetMachinePath("echo", Preferences::machine_path_type::WORKING_DIRECTORY)		== "C:\\MyEchoGames\\");
	assert(prefs.GetMachinePath("echo", Preferences::machine_path_type::LAST_SAVE_STATE)		== "C:\\MyLastState.sta");
	assert(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::WORKING_DIRECTORY)	== "");
	assert(prefs.GetMachinePath("foxtrot", Preferences::machine_path_type::LAST_SAVE_STATE)		== "");
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
//  global_get_path_category
//-------------------------------------------------

static void global_get_path_category()
{
	for (Preferences::global_path_type type : util::all_enums<Preferences::global_path_type>())
		Preferences::GetPathCategory(type);
}


//-------------------------------------------------
//  machine_get_path_category
//-------------------------------------------------

static void machine_get_path_category()
{
	for (Preferences::machine_path_type type : util::all_enums<Preferences::machine_path_type>())
		Preferences::GetPathCategory(type);
}


//-------------------------------------------------
//  substitutions
//-------------------------------------------------

static void substitutions(const wxChar *input, const wxChar *expected)
{
	auto func = [](const wxString &var_name)
	{
		return var_name == wxT("VARNAME")
			? wxT("vardata")
			: wxString();
	};

	wxString actual = InternalApplySubstitutions(wxString(input), func);
	assert(actual == expected);
	(void)actual;
	(void)expected;
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	general,
	path_names,
	global_get_path_category,
	machine_get_path_category,
	[]() { substitutions(wxT("C:\\foo"),				wxT("C:\\foo")); },
	[]() { substitutions(wxT("C:\\foo (with parens)"),	wxT("C:\\foo (with parens)")); },
	[]() { substitutions(wxT("C:\\$(VARNAME)\\foo"),	wxT("C:\\vardata\\foo")); }
};
