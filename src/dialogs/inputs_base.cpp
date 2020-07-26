/***************************************************************************

    dialogs/inputs_base.cpp

    Base class for Inputs and Switches dialog

***************************************************************************/

#include "dialogs/inputs_base.h"
#include "ui_inputs_base.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialogBase::InputsDialogBase(QWidget *parent, status::input::input_class input_class, bool restoreEnabled)
	: QDialog(parent)
	, m_input_class(input_class)
{
	// set up UI
	m_ui = std::make_unique<Ui::InputsDialogBase>();
	m_ui->setupUi(this);

	// the "Switches" dialog never had this implemented, so we have to have a provision for
	// keeping this disabled
	m_ui->restoreDefaultsButton->setEnabled(restoreEnabled);

	// input class drives the title
	QString title = getDialogTitle(m_input_class);
	setWindowTitle(title);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialogBase::~InputsDialogBase()
{
}


//-------------------------------------------------
//  on_restoreDefaultsButton_clicked
//-------------------------------------------------

void InputsDialogBase::on_restoreDefaultsButton_clicked()
{
	OnRestoreButtonPressed();
}


//-------------------------------------------------
//  getDialogTitle
//-------------------------------------------------

QString InputsDialogBase::getDialogTitle(status::input::input_class input_class)
{
	QString result;
	switch (input_class)
	{
	case status::input::input_class::CONTROLLER:
		result = "Joysticks and Controllers";
		break;
	case status::input::input_class::KEYBOARD:
		result = "Keyboard";
		break;
	case status::input::input_class::MISC:
		result = "Miscellaneous Input";
		break;
	case status::input::input_class::CONFIG:
		result = "Configuration";
		break;
	case status::input::input_class::DIPSWITCH:
		result = "DIP Switches";
		break;
	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  isRelevantInputClass
//-------------------------------------------------

bool InputsDialogBase::isRelevantInputClass(status::input::input_class inputClass) const
{
	return inputClass == m_input_class;
}


//-------------------------------------------------
//  addWidgetsToGrid
//-------------------------------------------------

void InputsDialogBase::addWidgetsToGrid(int row, std::initializer_list<std::reference_wrapper<QWidget>> widgets)
{
	int column = 0;
	for (QWidget &widget : widgets)
		m_ui->gridLayout->addWidget(&widget, row, column++);
}
