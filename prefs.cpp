/***************************************************************************

    prefs.cpp

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/xml/xml.h>
#include <fstream>
#include <functional>

#include "prefs.h"
#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ValidateDimension
//-------------------------------------------------

static bool IsValidDimension(long dimension)
{
    // arbitrary validation of dimensiohns
    return dimension >= 10 && dimension <= 20000;
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

Preferences::Preferences()
    : m_size(950, 600)
    , m_column_widths({85, 370, 50, 320})
{
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

    wxString file_name = GetFileName();

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

        if (component == "mamecommand")
        {
            SetMameCommand(node.GetNodeContent());
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
    wxString file_name = GetFileName();
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
    output << "\t<mamecommand>" << m_mame_command << "</mamecommand>" << std::endl;
    output << "\t<size width=\"" << m_size.GetWidth() << "\" height=\"" << m_size.GetHeight() << "\"/>" << std::endl;
    if (!m_selected_machine.IsEmpty())
        output << "\t<selectedmachine>" << m_selected_machine.ToStdString() << "</selectedmachine>" << std::endl;
    for (size_t i = 0; i < m_column_widths.size(); i++)
        output << "\t<column index=\"" << i << "\" width=\"" << m_column_widths[i] << "\"/>" << std::endl;
    output << "</preferences>" << std::endl;
}


//-------------------------------------------------
//  GetFileName
//-------------------------------------------------

wxString Preferences::GetFileName()
{
    wxStandardPaths::Get().UseAppInfo(wxStandardPaths::AppInfo_None);
    wxString path = wxStandardPaths::Get().GetUserConfigDir();
    wxFileName file_name(path, "BletchMAME.xml");
    return file_name.GetFullPath();
}
