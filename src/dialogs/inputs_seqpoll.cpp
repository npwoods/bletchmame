/***************************************************************************

    dialogs/inputs_seqpoll.cpp

    Input Sequence (InputSeq) polling dialog

***************************************************************************/

// bletchmame headers
#include "dialogs/inputs.h"
#include "dialogs/inputs_seqpoll.h"
#include "ui_inputs_seqpoll.h"
#include "status.h"

// Qt headers
#include <QMenu>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::SeqPollingDialog::SeqPollingDialog(InputsDialog &host, Type type, const QString &label)
	: QDialog(&host)
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

InputsDialog::SeqPollingDialog::~SeqPollingDialog()
{
}


//-------------------------------------------------
//  on_specifyMouseInputButton_clicked
//-------------------------------------------------

void InputsDialog::SeqPollingDialog::on_specifyMouseInputButton_clicked()
{
	// identify pertinent mouse inputs
	std::vector<const status::input_device_item *> items;
	for (const status::input_class &devclass : GetInputClasses())
	{
		if (devclass.m_name == "mouse")
		{
			for (const status::input_device &dev : devclass.m_devices)
			{
				for (const status::input_device_item &item : dev.m_items)
				{
					if (AxisType(item) == axis_type::NONE)
						items.emplace_back(&item);
				}
			}
		}
	}

	// sort the mouse inputs
	std::sort(items.begin(), items.end(), [](const status::input_device_item *x, const status::input_device_item *y)
	{
		return x->m_code < y->m_code;
	});

	// build the popup menu
	QMenu popupMenu;
	for (const auto &item : items)
	{
		popupMenu.addAction(item->m_name, [this, item]()
		{
			m_dialog_selected_result = item->m_code;
			close();
		});
	}

	// and display it
	QPoint popupPos = globalPositionBelowWidget(*m_ui->specifyMouseInputButton);
	popupMenu.exec(popupPos);
}


//-------------------------------------------------
//  GetInputClasses
//-------------------------------------------------

const std::vector<status::input_class> &InputsDialog::SeqPollingDialog::GetInputClasses()
{
	InputsDialog &host = *dynamic_cast<InputsDialog *>(parent());
	return host.m_host.GetInputClasses();
}
