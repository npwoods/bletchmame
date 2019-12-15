/***************************************************************************

    runmachinetask.h

    Task for running an emulation session

***************************************************************************/

#pragma once

#ifndef RUNMACHINETASK_H
#define RUNMACHINETASK_H

#include <wx/msgqueue.h>
#include <optional>

#include "task.h"
#include "info.h"
#include "wxhelpers.h"
#include "status.h"


//**************************************************************************
//  MACROS
//**************************************************************************

// workaround for GCC bug fixed in 7.4
#ifdef __GNUC__
#if __GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 4))
#define SHOULD_BE_DELETE	default
#endif	// __GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 4))
#endif // __GNUC__

#ifndef SHOULD_BE_DELETE
#define SHOULD_BE_DELETE	delete
#endif // !SHOULD_BE_DELETE

// the name of the worker_ui plugin
#define WORKER_UI_PLUGIN_NAME	wxT("worker_ui")


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

struct RunMachineResult
{
	RunMachineResult() = default;
	RunMachineResult(const RunMachineResult &that) = delete;
	RunMachineResult(RunMachineResult &&that) = default;

	bool        m_success;
    wxString    m_error_message;
};


struct Chatter
{
	Chatter() = default;
	Chatter(const Chatter &that) = delete;
	Chatter(Chatter &&that) = default;

	enum class chatter_type
	{
		COMMAND_LINE,
		RESPONSE
	};

	chatter_type		m_type;
	wxString			m_text;
};

wxDECLARE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDECLARE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<status::update>);
wxDECLARE_EVENT(EVT_CHATTER, PayloadEvent<Chatter>);

struct Machine;

class RunMachineTask : public Task
{
public:
	RunMachineTask(info::machine machine, wxString &&software, wxWindow &target_window);

	void Issue(const std::vector<wxString> &args);
	void IssueFullCommandLine(const wxString &full_command);
	const info::machine &GetMachine() const { return m_machine; }
	void SetChatterEnabled(bool enabled) { m_chatter_enabled = enabled; }

protected:
	virtual std::vector<wxString> GetArguments(const Preferences &prefs) const override;
	virtual void Process(wxProcess &process, wxEvtHandler &handler) override;
	virtual void Abort() override;
	virtual void OnChildProcessCompleted(emu_error status) override;
	virtual void OnChildProcessKilled() override;

private:
    struct Message
    {
		enum class type
		{
			INVALID,
			COMMAND,
			TERMINATED
		};

		type						m_type;
		wxString	                m_command;
		emu_error					m_status;
    };

	struct Response
	{
		enum class type
		{
			UNKNOWN,
			OK,
			END_OF_FILE,
			ERR
		};

		type						m_type;
		wxString	                m_text;
	};

	info::machine					m_machine;
	wxString						m_software;
	std::uintptr_t					m_target_window;
	wxMessageQueue<Message>         m_message_queue;
	volatile bool					m_chatter_enabled;

	void InternalPost(Message::type type, wxString &&command, emu_error status = emu_error::INVALID);
	Response ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &emu_output_stream);
	void PostChatter(wxEvtHandler &handler, Chatter::chatter_type type, wxString &&text);
};


#endif // RUNMACHINETASK_H
