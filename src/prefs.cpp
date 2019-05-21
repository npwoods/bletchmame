/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/xml/xml.h>
#include <wx/dir.h>
#include <fstream>
#include <functional>

#include "prefs.h"
#include "utility.h"


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


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ValidateDimension
//-------------------------------------------------

static bool IsValidDimension(long dimension)
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
{
	// Defaults
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

	// if it does, try to load it
	wxXmlDocument xml;
	if (!xml.Load(file_name))
		return false;

	util::ProcessXml(xml, std::bind(&Preferences::ProcessXmlCallback, this, _1, _2));
	return false;
}


//-------------------------------------------------
//  ProcessXmlCallback
//-------------------------------------------------

void Preferences::ProcessXmlCallback(const std::vector<wxString> &path, const wxXmlNode &node)
{
	if (path.size() == 2 && path[0] == "preferences")
	{
		const wxString &component(path[1]);

		if (component == "path")
		{
			auto iter = std::find(s_path_names.cbegin(), s_path_names.cend(), node.GetAttribute("type"));
			if (iter != s_path_names.cend())
			{
				path_type type = static_cast<path_type>(iter - s_path_names.cbegin());
				SetPath(type, node.GetNodeContent());
			}
		}
		else if (component == "mameextraarguments")
		{
			SetMameExtraArguments(node.GetNodeContent());
		}
		else if (component == "size")
		{
			long width = -1, height = -1;
			if (node.GetAttribute("width").ToLong(&width) &&
				node.GetAttribute("height").ToLong(&height) &&
				IsValidDimension(width) &&
				IsValidDimension(height))
			{
				wxSize size;
				size.SetWidth((int)width);
				size.SetHeight((int)height);
				SetSize(size);
			}
		}
		else if (component == "selectedmachine")
		{
			SetSelectedMachine(node.GetNodeContent());
		}
		else if (component == "column")
		{
			long index = -1, width = -1;
			if (node.GetAttribute("index").ToLong(&index) &&
				node.GetAttribute("width").ToLong(&width) &&
				index >= 0 && index <= (long)m_column_widths.size() &&
				IsValidDimension(width))
			{
				SetColumnWidth((int)index, (int)width);
			}
		}
	}
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
	output << "<preferences>" << std::endl;
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
		output << "\t<column index=\"" << i << "\" width=\"" << m_column_widths[i] << "\"/>" << std::endl;
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
