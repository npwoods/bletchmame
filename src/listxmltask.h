/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include "task.h"
#include "payloadevent.h"

struct ListXmlResult
{
	enum class status
	{
		SUCCESS,
		ABORTED,
		XML_PARSE_ERROR,
		IO_ERROR
	};

	status		m_status;
	wxString	m_error_message;
};

wxDECLARE_EVENT(EVT_LIST_XML_RESULT, PayloadEvent<ListXmlResult>);


//**************************************************************************
//  FUNCTION PROTOTYPES
//**************************************************************************

Task::ptr create_list_xml_task(wxString &&dest);

#endif // LISTXMLTASK_H
