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


//-------------------------------------------------
//  ReadUntilEnd
//-------------------------------------------------

static wxString ReadUntilEnd(wxTextInputStream &stream)
{
	wxString s, result;

	while ((s = stream.ReadLine()), !s.IsEmpty())
	{
		result += s;
		result += "\r\n";
	}
	return result;
}


//**************************************************************************
//  MAIN THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(info::machine machine, wxString &&software, wxWindow &target_window)
    : m_machine(machine)
	, m_software(software)
    , m_target_window((std::uintptr_t)target_window.GetHWND())
	, m_chatter_enabled(false)
{
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> RunMachineTask::GetArguments(const Preferences &prefs) const
{
	std::vector<wxString> results = { GetMachine().name() };
	if (!m_software.empty())
		results.push_back(m_software);

	std::vector<wxString> args =
	{
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

	results.reserve(results.size() + args.size());
	for (wxString &arg : args)
		results.push_back(std::move(arg));
	return results;
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
	Response response;

	// set up streams (note that from our perspective, MAME's output streams are input
	// for us, and this we use wxTextInputStream)
	wxTextOutputStream emu_input_stream(*process.GetOutputStream());
	wxTextInputStream emu_output_stream(*process.GetInputStream());
	wxTextInputStream emu_error_stream(*process.GetErrorStream());

	// receive the inaugural response from MAME; we want to call it quicks if this doesn't work
	response = ReceiveResponse(handler, emu_output_stream);
	if (response.m_type != Response::type::OK)
	{
		// alas, we have an error starting MAME
		result.m_success = false;

		// try various strategies to get the best error message possible
		if (!response.m_text.empty())
		{
			// we got an error message from the LUA worker_ui plug-in; display it
			result.m_error_message = std::move(response.m_text);
		}
		else
		{
			// we did not get an error message from the LUA worker_ui plug-in; capture
			// MAME's standard output and present it (not ideal, but better than nothing)
			wxString error_output = ReadUntilEnd(emu_error_stream);
			result.m_error_message = !error_output.empty()
				? wxT("Error starting MAME:\r\n\r\n") + error_output
				: wxT("Error starting MAME");	// when all else fails...
		}
		util::QueueEvent(handler, EVT_RUN_MACHINE_RESULT, wxID_ANY, std::move(result));
		return;
	}

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
			emu_input_stream.WriteString(message.m_command);

			if (m_chatter_enabled)
				PostChatter(handler, Chatter::chatter_type::COMMAND_LINE, std::move(message.m_command));

			// and receive a response from MAME
			response = ReceiveResponse(handler, emu_output_stream);
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

	// was there an error?
	wxString error_message;
	if (status != emu_error::NONE)
	{
		// if so, capture what was emitted by MAME's standard output stream
		error_message = ReadUntilEnd(emu_error_stream);

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

RunMachineTask::Response RunMachineTask::ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &emu_output_stream)
{
	static const util::enum_parser<Response::type> s_response_type_parser =
	{
		{ "@OK", Response::type::OK, },
		{ "@ERROR", Response::type::ERR }
	};
	Response response;

	// MAME has a pesky habit of emitting human readable messages to standard output, therefore
	// we have a convention with the worker_ui plugin by which actual messages are preceeded with
	// an at-sign
	wxString str;
	do
	{
		str = emu_output_stream.ReadLine();
	} while ((!str.empty() && str[0] != '@') || (str.empty() && !emu_output_stream.GetInputStream().Eof()));
	wxLogDebug("MAME ==> %s", str);

	// special case; check for EOF
	if (str.empty())
	{
		response.m_type = Response::type::END_OF_FILE;
		return response;
	}

	// chatter
	if (m_chatter_enabled)
		PostChatter(handler, Chatter::chatter_type::RESPONSE, wxString(str));

	// start interpreting the response; first get the text
	size_t index = str.find('#');
	if (index != std::string::npos)
	{
		// get the response text
		size_t response_text_position = index + 1;
		while (response_text_position < str.size() && str[response_text_position] == '#')
			response_text_position++;
		while (response_text_position < str.size() && str[response_text_position] == ' ')
			response_text_position++;
		response.m_text = str.substr(response_text_position);

		// and resize the original string
		str.resize(index);
	}

	// now get the arguments; there should be at least one
	std::vector<wxString> args = util::string_split(str, [](wchar_t ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });
	assert(!args.empty());

	// interpret the main message
	s_response_type_parser(args[0].ToStdString(), response.m_type);

	// did we get a status reponse
	if (response.m_type == Response::type::OK && args.size() >= 2 && args[1] == "STATUS")
	{
		status::update status_update = status::update::read(emu_output_stream);
		util::QueueEvent(handler, EVT_STATUS_UPDATE, wxID_ANY, std::move(status_update));
	}
	return response;
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
