/***************************************************************************

    dlginputs.cpp

    Input customization dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
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
		template<typename TControl, typename... TArgs> TControl &AddControl(wxSizer &sizer, int flags, TArgs&&... args);
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
	wxString buffer;
	int id = wxID_LAST + 1;

	// main grid
	std::unique_ptr<wxFlexGridSizer> grid_sizer = std::make_unique<wxFlexGridSizer>(2);
	grid_sizer->AddGrowableCol(1);

	// build controls
	for (const Input &input : inputs)
	{
		for (const InputSeq &input_seq : input.m_seqs)
		{
			const wxString *name;
			switch (input_seq.m_type)
			{
			case InputSeq::inputseq_type::STANDARD:
				name = &input_seq.m_text;
				break;
			case InputSeq::inputseq_type::INCREMENT:
				buffer = input_seq.m_text + " Inc";
				name = &buffer;
				break;
			case InputSeq::inputseq_type::DECREMENT:
				buffer = input_seq.m_text + " Dec";
				name = &buffer;
				break;
			default:
				throw false;
			}

			AddControl<wxButton>(*grid_sizer, wxALL, id++, *name);
			AddControl<wxStaticText>(*grid_sizer, wxALL, id++, input_seq.m_text);
		}
	}

	SetSizerAndFit(grid_sizer.release());
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &InputsDialog::AddControl(wxSizer &sizer, int flags, TArgs&&... args)
{
	TControl *control = new TControl(this, std::forward<TArgs>(args)...);
	sizer.Add(control, 0, flags, 4);
	return *control;
}


//-------------------------------------------------
//  show_inputs_dialog
//-------------------------------------------------

bool show_inputs_dialog(wxWindow &parent, const std::vector<Input> &inputs)
{
	InputsDialog dialog(parent, inputs);
	return dialog.ShowModal() == wxID_OK;
}
