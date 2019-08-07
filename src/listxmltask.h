/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include "task.h"
#include "wxhelpers.h"


//**************************************************************************
//  MACROS
//**************************************************************************

// pesky macros
#ifdef ERROR
#undef ERROR
#endif // ERROR


//**************************************************************************
//  TYPES
//**************************************************************************

struct ListXmlResult
{
	enum class status
	{
		SUCCESS,		// the invocation of -listxml succeeded
		ABORTED,		// the user aborted the -listxml request midflight
		ERROR			// an error is to be reported to use user
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
