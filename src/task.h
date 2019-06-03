/***************************************************************************

    task.h

    Abstract base class for handling tasks for MAME

***************************************************************************/

#pragma once

#ifndef TASK_H
#define TASK_H

#include <wx/process.h>
#include <wx/event.h>
#include <memory>

class Preferences;
class MameClient;

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> Task

class Task
{
	friend class MameClient;

public:
	enum class emu_error
	{
		// standard MAME error codes, from MAME main.h
		NONE = 0,				// no error
		FAILED_VALIDITY = 1,	// failed validity checks
		MISSING_FILES = 2,		// missing files
		FATALERROR = 3,			// some other fatal error
		DEVICE = 4,				// device initialization error (MESS-specific)
		NO_SUCH_GAME = 5,		// game was specified but doesn't exist
		INVALID_CONFIG = 6,		// some sort of error in configuration 
		IDENT_NONROMS = 7,		// identified all non-ROM files
		IDENT_PARTIAL = 8,		// identified some files but not all
		IDENT_NONE = 9,			// identified no files

		// our error codes
		INVALID = 1000,			// invalid error code
		KILLED,					// the process was killed
	};

	typedef std::shared_ptr<Task> ptr;
	virtual ~Task();

protected:
	// called on the main thread to trigger a shutdown (e.g. - BletchMAME is closed)
	virtual void Abort();

	// retrieves the arguments to be used at the command line
	virtual std::vector<wxString> GetArguments(const Preferences &prefs) const = 0;

	// called on a child thread tasked with ownership of a MAME child process
	virtual void Process(wxProcess &process, wxEvtHandler &handler) = 0;

	// called on the main thread when the child process has been completed
	virtual void OnChildProcessCompleted(emu_error status);

	// called on the main thread when the child process has been killed
	virtual void OnChildProcessKilled();
};


#endif // TASK_H
