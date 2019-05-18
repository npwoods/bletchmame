/***************************************************************************

    prefs.h

    Wrapper for preferences specific to BletchMAME

***************************************************************************/

#pragma once

#ifndef PREFS_H
#define PREFS_H

#include <wx/string.h>
#include <wx/gdicmn.h>
#include <array>
#include <ostream>

class Preferences
{
public:
    Preferences();
    ~Preferences();

    const wxString &GetMameCommand() const                  { return m_mame_command; }
    void SetMameCommand(wxString &&mame_command)            { m_mame_command = std::move(mame_command); }

    const wxSize &GetSize() const                           { return m_size; }
    void SetSize(const wxSize &size)                        { m_size = size; }

    int GetColumnWidth(int column_index) const              { return m_column_widths[column_index]; }
    void SetColumnWidth(int column_index, int width)        { m_column_widths[column_index] = width; }

    const wxString &GetSelectedMachine() const              { return m_selected_machine; }
    void SetSelectedMachine(const wxString &machine_name)   { m_selected_machine = machine_name; }
    void SetSelectedMachine(wxString &&machine_name)        { m_selected_machine = std::move(machine_name); }

private:
    wxString                        m_mame_command;
    wxSize                          m_size;
    std::array<int, 4>              m_column_widths;
    wxString                        m_selected_machine;

    bool Load();
    void Save();
    void Save(std::ostream &output);
    void ProcessXmlCallback(const std::vector<wxString> &path, const wxXmlNode &node);
    wxString GetFileName();
};

#endif // PREFS_H
