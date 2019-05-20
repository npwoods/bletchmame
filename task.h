/***************************************************************************

    task.h

    Abstract base class for handling tasks for MAME

***************************************************************************/

#pragma once

#ifndef TASK_H
#define TASK_H

#include <wx/process.h>
#include <wx/event.h>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> Task

class Task
{
public:
    typedef std::unique_ptr<Task> ptr;

    virtual ~Task()
    {
    }

	virtual void Process(wxProcess &process, wxEvtHandler &handler) = 0;
	virtual void Abort() {}
	virtual std::vector<wxString> GetArguments() const = 0;
};


#endif // TASK_H
