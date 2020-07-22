/***************************************************************************

    dialogs/inputs.cpp

    Input customization dialog

***************************************************************************/

#include "dialogs/inputs.h"
#include "ui_inputs.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::InputsDialog(QWidget &parent, const QString &title, IInputsHost &host, status::input::input_class input_class)
{
    // set up UI
    m_ui = std::make_unique<Ui::InputsDialog>();
    m_ui->setupUi(this);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialog::~InputsDialog()
{
}

