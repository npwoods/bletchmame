/***************************************************************************

    dialogs/inputs_multiaxis.cpp

    Multi Axis Input Dialog

***************************************************************************/

#include "dialogs/inputs_multiaxis.h"
#include "ui_inputs_multiaxis.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::MultiAxisInputDialog::MultiAxisInputDialog(InputsDialog &parent, const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref)
    : QDialog(&parent)
{
    // set up UI
    m_ui = std::make_unique<Ui::MultiAxisInputDialog>();
    m_ui->setupUi(this);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialog::MultiAxisInputDialog::~MultiAxisInputDialog()
{
}
