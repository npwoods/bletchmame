/***************************************************************************

    runmachinetask.cpp

    Task for running an emulation session

***************************************************************************/

#include "wx/wxprec.h"

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <wx/sstream.h>
#include <iostream>
#include <thread>

#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "prefs.h"
#include "validity.h"


//**************************************************************************
//  VARIABLES
//**************************************************************************

wxDEFINE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDEFINE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<status::update>);
wxDEFINE_EVENT(EVT_CHATTER, PayloadEvent<Chatter>);


//**************************************************************************
//  UTILITY
//**************************************************************************

//-------------------------------------------------
//  BuildCommand
//-------------------------------------------------

wxString BuildCommand(const std::vector<wxString> &args)
{
	wxString command;
	for (const wxString &arg : args)
	{
		if (!command.IsEmpty())
			command += " ";

		// do we need quotes?
		bool needs_quotes = arg.empty() || arg.find(' ') != wxString::npos;

		// append the argument, with quotes if necessary
		if (needs_quotes)
			command += "\"";
		command += arg;
		if (needs_quotes)
			command += "\"";
	}
	command += "\r\n";
	return command;
}


//**************************************************************************
//  MAIN THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(info::machine machine, wxWindow &target_window)
    : m_machine(machine)
    , m_target_window((std::uintptr_t)target_window.GetHWND())
	, m_chatter_enabled(false)
{
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> RunMachineTask::GetArguments(const Preferences &prefs) const
{
	return
	{
		GetMachine().name(),
		"-rompath",
		prefs.GetPathWithSubstitutions(Preferences::path_type::roms),
		"-samplepath",
		prefs.GetPathWithSubstitutions(Preferences::path_type::samples),
		"-cfg_directory",
		prefs.GetPathWithSubstitutions(Preferences::path_type::config),
		"-nvram_directory",
		prefs.GetPathWithSubstitutions(Preferences::path_type::nvram),
		"-hashpath",
		prefs.GetPathWithSubstitutions(Preferences::path_type::hash),
		"-artpath",
		prefs.GetPathWithSubstitutions(Preferences::path_type::artwork),
		"-pluginspath",
		prefs.GetPathWithSubstitutions(Preferences::path_type::plugins),
		"-window",
		"-keyboardprovider",
		"dinput",
		"-mouseprovider",
		"dinput",
		"-lightgunprovider",
		"dinput",
		"-attach_window",
		std::to_string(m_target_window),
		"-skip_gameinfo",
		"-nomouse",
		"-plugin",
		WORKER_UI_PLUGIN_NAME
	};
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void RunMachineTask::Abort()
{
	Issue({ "exit" });
}


//-------------------------------------------------
//  OnChildProcessCompleted
//-------------------------------------------------

void RunMachineTask::OnChildProcessCompleted(emu_error status)
{
	InternalPost(Message::type::TERMINATED, "", status);
}


//-------------------------------------------------
//  OnChildProcessKilled
//-------------------------------------------------

void RunMachineTask::OnChildProcessKilled()
{
	InternalPost(Message::type::TERMINATED, "", emu_error::KILLED);
}


//-------------------------------------------------
//  Issue
//-------------------------------------------------

void RunMachineTask::Issue(const std::vector<wxString> &args)
{
	wxString command = BuildCommand(args);
	InternalPost(Message::type::COMMAND, std::move(command));
}


//-------------------------------------------------
//  IssueFullCommandLine
//-------------------------------------------------

void RunMachineTask::IssueFullCommandLine(const wxString &full_command)
{
	InternalPost(Message::type::COMMAND, full_command + "\r\n");
}


//-------------------------------------------------
//  InternalPost
//-------------------------------------------------

void RunMachineTask::InternalPost(Message::type type, wxString &&command, emu_error status)
{
	Message message;
	message.m_type = type;
	message.m_command = std::move(command);
	message.m_status = status;
	m_message_queue.Post(message);
}


//**************************************************************************
//  CLIENT THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  Process
//-------------------------------------------------

void RunMachineTask::Process(wxProcess &process, wxEvtHandler &handler)
{
	RunMachineResult result;

	// set up streams
	wxTextInputStream input(*process.GetInputStream());
	wxTextOutputStream output(*process.GetOutputStream());
	wxTextInputStream error(*process.GetErrorStream());

	// receive the inaugural response from MAME
	ReceiveResponse(handler, input);

	// loop until the process terminates
	bool done = false;
	emu_error status = emu_error::NONE;
	while (!done)
	{
		// await a message from the queue
		Message message;
		m_message_queue.Receive(message);

		switch (message.m_type)
		{
		case Message::type::COMMAND:
			// emit this command to MAME
			wxLogDebug("MAME <== [%s]", message.m_command);
			output.WriteString(message.m_command);

			if (m_chatter_enabled)
				PostChatter(handler, Chatter::chatter_type::COMMAND_LINE, std::move(message.m_command));

			// and receive a response from MAME
			ReceiveResponse(handler, input);
			break;

		case Message::type::TERMINATED:
			wxLogDebug("MAME --- TERMINATED (status=%d)", message.m_status);
			done = true;
			status = message.m_status;
			break;

		default:
			throw false;
		}
	}

	// was there an error?  if so capture it
	wxString error_message;
	if (status != emu_error::NONE)
	{
		// read until the end of the stream
		wxString s;
		while ((s = error.ReadLine()), !s.IsEmpty())
		{
			error_message += s;
			error_message += "\r\n";
		}

		// if we were not able to collect any info, show something
		if (error_message.IsEmpty())
			error_message = wxString::Format("Error %d running MAME", (int)status);
	}
	result.m_error_message = std::move(error_message);

	util::QueueEvent(handler, EVT_RUN_MACHINE_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  ReceiveResponse
//-------------------------------------------------

void RunMachineTask::ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &input)
{
	// MAME has a pesky habit of emitting human readable messages to standard output, therefore
	// we have a convention with the worker_ui plugin by which actual messages are preceeded with
	// an at-sign
	wxString str;
	do
	{
		str = input.ReadLine();
	} while (!input.GetInputStream().Eof() && (str.empty() || str[0] != '@'));
	wxLogDebug("MAME ==> %s", str);

	// chatter
	if (m_chatter_enabled)
		PostChatter(handler, Chatter::chatter_type::RESPONSE, wxString(str));

	// interpret the response
	util::string_truncate(str, '#');
	std::vector<wxString> args = util::string_split(str, [](wchar_t ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });

	// did we get a status reponse
	if (args.size() >= 2 && args[0] == "@OK" && args[1] == "STATUS")
	{
		status::update status_update = status::update::read(input);
		util::QueueEvent(handler, EVT_STATUS_UPDATE, wxID_ANY, std::move(status_update));
	}
}


//-------------------------------------------------
//  PostChatter
//-------------------------------------------------

void RunMachineTask::PostChatter(wxEvtHandler &handler, Chatter::chatter_type type, wxString &&text)
{
	Chatter chatter;
	chatter.m_type = type;
	chatter.m_text = std::move(text);
	util::QueueEvent(handler, EVT_CHATTER, wxID_ANY, std::move(chatter));
}


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

static void test()
{
	wxString command = BuildCommand({ "alpha", "bravo", "charlie" });
	assert(command == "alpha bravo charlie\r\n");
}


//-------------------------------------------------
//  checks
//-------------------------------------------------

static const validity_check checks[] =
{
	test
};
