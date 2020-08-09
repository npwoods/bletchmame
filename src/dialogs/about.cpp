/***************************************************************************

    dialogs/about.h

    BletchMAME About Dialog

***************************************************************************/

#include "about.h"
#include "ui_about.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
