/***************************************************************************

    dialogs/inputs_seqpoll.cpp

    Input Sequence (InputSeq) polling dialog

***************************************************************************/

#include "dialogs/inputs_seqpoll.h"
#include "ui_inputs_seqpoll.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

SeqPollingDialog::SeqPollingDialog(QWidget *parent, Type type, const QString &label)
{
	// set up UI
	m_ui = std::make_unique<Ui::SeqPollingDialog>();
	m_ui->setupUi(this);

	// identify proper formats
	QString titleFormat, instructionsFormat;
	switch (type)
	{
	case Type::ADD:
		titleFormat = "Add To %1";
		instructionsFormat = "Press key or button to add to %1";
		break;

	case Type::SPECIFY:
		titleFormat = "Specify %1";
		instructionsFormat = "Press key or button to specify %1";
		break;

	default:
		throw false;
	}

	// specify proper window title and text for instructions
	setWindowTitle(titleFormat.arg(label));
	m_ui->instructionsLabel->setText(instructionsFormat.arg(label));
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

SeqPollingDialog::~SeqPollingDialog()
{
}


//-------------------------------------------------
//  on_specifyMouseInputButton_clicked
//-------------------------------------------------

void SeqPollingDialog::on_specifyMouseInputButton_clicked()
{
	throw false;	// NYI
}