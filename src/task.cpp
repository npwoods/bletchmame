/***************************************************************************

    task.cpp

    Abstract base class for handling tasks for MAME

***************************************************************************/

#include "task.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  dtor
//-------------------------------------------------

Task::~Task()
{
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void Task::Abort()
{
	// by default, do nothing - most tasks (like -listxml) are doing something
	// of finite duration anyways and an explicit "abort" does not have any
	// meaning
}


//-------------------------------------------------
//  OnChildProcessCompleted
//-------------------------------------------------

void Task::OnChildProcessCompleted(emu_error)
{
	// by default, do nothing
}


//-------------------------------------------------
//  OnChildProcessKilled
//-------------------------------------------------

void Task::OnChildProcessKilled()
{
	// by default, do nothing
}
