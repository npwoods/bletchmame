/***************************************************************************

    dlginputs.cpp

    Input customization dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <wx/button.h>

#include "dlginputs.h"
#include "runmachinetask.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class InputsDialog : public wxDialog
	{
	public:
		InputsDialog(wxWindow &parent, const std::vector<Input> &inputs);

	private:
		template<typename TControl, typename... TArgs> TControl &AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args);

		void StartInputPoll(const wxString &port_tag, int mask, InputSeq::inputseq_type seq_type);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::InputsDialog(wxWindow &parent, const std::vector<Input> &inputs)
	: wxDialog(&parent, wxID_ANY, "Inputs", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
{
	int id = wxID_LAST + 1;

	// scrolled view
	std::unique_ptr<wxBoxSizer> outer_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	wxScrolledWindow &scrolled = AddControl<wxScrolledWindow>(*this, *outer_sizer, 1, wxALL | wxEXPAND);

	// buttons
	std::unique_ptr<wxBoxSizer> button_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	wxButton &restore_button	= AddControl<wxButton>(*this, *button_sizer, 0, wxALL, id++, "Restore Defaults");
	wxButton &ok_button			= AddControl<wxButton>(*this, *button_sizer, 0, wxALL, wxID_OK, "OK");
	outer_sizer->Add(button_sizer.release(), 0, wxALIGN_RIGHT);
	
	// restore button is not yet implemented
	restore_button.Disable();

	// main grid
	std::unique_ptr<wxFlexGridSizer> grid_sizer = std::make_unique<wxFlexGridSizer>(2);

	// build controls
	wxString buffer;
	for (const Input &input : inputs)
	{
		for (const InputSeq &input_seq : input.m_seqs)
		{
			const wxString *name;
			switch (input_seq.m_type)
			{
			case InputSeq::inputseq_type::STANDARD:
				name = &input.m_name;
				break;
			case InputSeq::inputseq_type::INCREMENT:
				buffer = input.m_name + " Inc";
				name = &buffer;
				break;
			case InputSeq::inputseq_type::DECREMENT:
				buffer = input.m_name + " Dec";
				name = &buffer;
				break;
			default:
				throw false;
			}

			wxButton &button			= AddControl<wxButton>(scrolled, *grid_sizer, 0, wxALL | wxEXPAND, id++, *name);
			wxStaticText &static_text	= AddControl<wxStaticText>(scrolled, *grid_sizer, 0, wxALL, id++, input_seq.m_text);

			wxString port_tag = input.m_port_tag;
			int mask = input.m_mask;
			InputSeq::inputseq_type seq_type = input_seq.m_type;
			Bind(wxEVT_BUTTON, [this, port_tag, mask, seq_type](auto &) { StartInputPoll(port_tag, mask, seq_type); }, button.GetId());

			(void)static_text;
		}
	}

	// configure scrolling
	scrolled.SetScrollRate(25, 25);
	scrolled.SetSizer(grid_sizer.release());
	scrolled.FitInside();

	// finally set the outer sizer
	SetSizer(outer_sizer.release());

	// appease compiler
	(void)ok_button;
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &InputsDialog::AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args)
{
	TControl *control = new TControl(&parent, std::forward<TArgs>(args)...);
	sizer.Add(control, proportion, flags, 4);
	return *control;
}


//-------------------------------------------------
//  StartInputPoll
//-------------------------------------------------

void InputsDialog::StartInputPoll(const wxString &port_tag, int mask, InputSeq::inputseq_type seq_type)
{
	(void)port_tag;
	(void)mask;
	(void)seq_type;
}


//-------------------------------------------------
//  show_inputs_dialog
//-------------------------------------------------

bool show_inputs_dialog(wxWindow &parent, const std::vector<Input> &inputs)
{
	InputsDialog dialog(parent, inputs);
	return dialog.ShowModal() == wxID_OK;
}
