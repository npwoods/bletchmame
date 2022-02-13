/***************************************************************************

    dialogs/newcustomfolder.cpp

    Dialog for adding custom folders

***************************************************************************/

// bletchmame headers
#include "dialogs/newcustomfolder.h"
#include "ui_newcustomfolder.h"

// Qt headers
#include <QPushButton>


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

NewCustomFolderDialog::NewCustomFolderDialog(std::function<bool(const QString &)> customFolderExistsFunc, QWidget *parent)
    : QDialog(parent)
    , m_customFolderExistsFunc(std::move(customFolderExistsFunc))
{
    // set up Qt form
    m_ui = std::make_unique<Ui::NewCustomFolderDialog>();
    m_ui->setupUi(this);

    // initial update
    update();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

NewCustomFolderDialog::~NewCustomFolderDialog()
{
}


//-------------------------------------------------
//  update
//-------------------------------------------------

void NewCustomFolderDialog::update()
{
    QString text = newCustomFolderName();
    bool okEnabled = !text.isEmpty() && !m_customFolderExistsFunc(text);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(okEnabled);
}


//-------------------------------------------------
//  newCustomFolderName
//-------------------------------------------------

QString NewCustomFolderDialog::newCustomFolderName() const
{
    return m_ui->lineEdit->text();
}


//-------------------------------------------------
//  on_lineEdit_textChanged
//-------------------------------------------------

void NewCustomFolderDialog::on_lineEdit_textChanged()
{
    update();
}
