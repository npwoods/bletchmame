/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include "utility.h"


//-------------------------------------------------
//  InternalProcessXml
//-------------------------------------------------

static void InternalProcessXml(const wxXmlNode &node, std::vector<wxString> &path, const std::function<void(const std::vector<wxString> &path, const wxXmlNode &node)> &callback)
{
    path.push_back(node.GetName());
    callback(path, node);

    for (wxXmlNode *child = node.GetChildren(); child; child = child->GetNext())
    {
        InternalProcessXml(*child, path, callback);
    }

    path.resize(path.size() - 1);
}


//-------------------------------------------------
//  ProcessXml
//-------------------------------------------------

void util::ProcessXml(wxXmlDocument &xml, const std::function<void(const std::vector<wxString> &path, const wxXmlNode &node)> &callback)
{
    wxXmlNode &root(*xml.GetRoot());
    std::vector<wxString> path;
    InternalProcessXml(root, path, callback);
}
