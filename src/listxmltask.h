/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include "task.h"
#include "payloadevent.h"

struct ConfigurationCondition
{
	wxString							m_tag;
	int									m_mask;
	wxString							m_relation;
	int									m_value;
};

struct ConfigurationSetting
{
	wxString							m_name;
	int									m_value;
	std::vector<ConfigurationCondition>	m_conditions;
};

struct Configuration
{
	wxString							m_name;
	wxString							m_tag;
	std::vector<ConfigurationSetting>	m_settings;
};

struct Device
{
	wxString							m_type;
	wxString							m_tag;
	wxString							m_instance_name;
	std::vector<wxString>				m_extensions;
};

struct Machine
{
	bool								m_light;
	wxString							m_name;
	wxString							m_sourcefile;
	wxString							m_clone_of;
	wxString							m_rom_of;
	wxString							m_description;
	wxString							m_year;
	wxString							m_manfacturer;
	std::vector<Configuration>			m_configurations;
	std::vector<Device>					m_devices;
};

struct ListXmlResult
{
	bool                    m_success;
	wxString				m_target;
	wxString                m_build;
	std::vector<Machine>    m_machines;
};

wxDECLARE_EVENT(EVT_LIST_XML_RESULT, PayloadEvent<ListXmlResult>);


//**************************************************************************
//  FUNCTION PROTOTYPES
//**************************************************************************

Task::ptr create_list_xml_task(bool light = true, const wxString &target = "");

#endif // LISTXMLTASK_H
