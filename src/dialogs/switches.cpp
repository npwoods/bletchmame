/***************************************************************************

    dialogs/switches.cpp

    Configuration & DIP Switches customization dialog

***************************************************************************/

#include "dialogs/switches.h"
#include "ui_inputs.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

SwitchesDialog::SwitchesDialog(QWidget &parent, const QString &title, ISwitchesHost &host, status::input::input_class input_class)
{
    // set up UI
    m_ui = std::make_unique<Ui::InputsDialog>();
    m_ui->setupUi(this);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

SwitchesDialog::~SwitchesDialog()
{
}

