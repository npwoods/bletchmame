/***************************************************************************

    versiontask.h

    Task for invoking '-version'

***************************************************************************/

#pragma once

#ifndef VERSIONTASK_H
#define VERSIONTASK_H

#include "task.h"
#include "payloadevent.h"


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

struct VersionResult
{
	wxString	m_version;
};

wxDECLARE_EVENT(EVT_VERSION_RESULT, PayloadEvent<VersionResult>);


//**************************************************************************
//  FUNCTION PROTOTYPES
//**************************************************************************

Task::ptr create_version_task();

#endif // VERSIONTASK_H
