/***************************************************************************

    dialogs/inputs_multiquick.cpp

    Multi Axis Input Dialog

***************************************************************************/

#include "dialogs/inputs_multiquick.h"
#include "ui_inputs_multiquick.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::MultipleQuickItemsDialog::MultipleQuickItemsDialog(InputsDialog &parent, std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last)
    : QDialog(&parent)
{
    // set up UI
    m_ui = std::make_unique<Ui::MultipleQuickItemsDialog>();
    m_ui->setupUi(this);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialog::MultipleQuickItemsDialog::~MultipleQuickItemsDialog()
{
}


//-------------------------------------------------
//  GetSelectedQuickItems
//-------------------------------------------------

std::vector<std::reference_wrapper<const InputsDialog::QuickItem>> InputsDialog::MultipleQuickItemsDialog::GetSelectedQuickItems() const
{
    throw false;
}
