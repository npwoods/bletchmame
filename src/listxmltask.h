/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include "task.h"
#include "payloadevent.h"

struct Machine
{
    wxString     m_name;
    wxString     m_sourcefile;
    wxString     m_clone_of;
    wxString     m_rom_of;
    wxString     m_description;
    wxString     m_year;
    wxString     m_manfacturer;
};

struct ListXmlResult
{
    bool                    m_success;
    wxString                m_build;
    std::vector<Machine>    m_machines;
};

wxDECLARE_EVENT(EVT_LIST_XML_RESULT, PayloadEvent<ListXmlResult>);


//**************************************************************************
//  FUNCTION PROTOTYPES
//**************************************************************************

Task::ptr create_list_xml_task();

#endif // LISTXMLTASK_H
