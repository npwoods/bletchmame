/***************************************************************************

    dialogs/console.cpp

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#include <QTextCursor>
#include <QTextCharFormat>

#include "dialogs/console.h"
#include "ui_console.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ConsoleDialog::ConsoleDialog(QWidget *parent, std::shared_ptr<RunMachineTask> &&task, IConsoleDialogHost &host)
	: QDialog(parent)
	, m_host(host)
	, m_task(std::move(task))
{
	// set up UI
	m_ui = std::make_unique<Ui::ConsoleDialog>();
	m_ui->setupUi(this);

	// connect events
	connect(m_ui->invokeButton, &QPushButton::clicked, [this]() { onInvoke(); });
	connect(m_ui->invokeLineEdit, &QLineEdit::textChanged, [this]() { m_ui->invokeButton->setEnabled(!m_ui->invokeLineEdit->text().isEmpty()); });

	// listen to chatter
	m_host.SetChatterListener([this](const ChatterEvent &chatter) { onChatter(chatter); });
	m_task->setChatterEnabled(true);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ConsoleDialog::~ConsoleDialog()
{
	m_task->setChatterEnabled(false);
	m_host.SetChatterListener(nullptr);
}


//-------------------------------------------------
//  onInvoke
//-------------------------------------------------

void ConsoleDialog::onInvoke()
{
	// get the command line, and ensure that it is not blank or whitespace
	QString command_line = m_ui->invokeLineEdit->text();

	// if this just white space, bail
	if (std::find_if(command_line.begin(), command_line.end(), [](auto ch) { return ch != ' '; }) == command_line.end())
		return;

	// issue the command
	m_task->issueFullCommandLine(std::move(command_line));

	// and clear
	m_ui->invokeLineEdit->clear();
}


//-------------------------------------------------
//  OnChatter
//-------------------------------------------------

void ConsoleDialog::onChatter(const ChatterEvent &evt)
{
	// ignore pings; they are noisy
	if (isChatterPing(evt) && m_ui->filterOutPingsCheckBox->checkState() == Qt::CheckState::Checked)
		return;

	// determine the text color
	QColor color;
	switch (evt.type())
	{
	case ChatterEvent::ChatterType::COMMAND_LINE:
		color = QColorConstants::Blue;
		break;
	case ChatterEvent::ChatterType::RESPONSE:
		color = evt.text().startsWith("OK") ? QColorConstants::Black : QColorConstants::Red;
		break;
	default:
		throw false;
	}

	// append the text
	QTextCursor cursor(m_ui->textView->textCursor());
	QTextCharFormat format;
	format.setForeground(QBrush(color));
	cursor.setCharFormat(format);
	cursor.insertText(evt.text());
	cursor.insertText("\r\n");
}


//-------------------------------------------------
//  isChatterPing
//-------------------------------------------------

bool ConsoleDialog::isChatterPing(const ChatterEvent &evt)
{
	// hack, but good enough for now
	return (evt.type() == ChatterEvent::ChatterType::COMMAND_LINE && evt.text() == "ping")
		|| (evt.type() == ChatterEvent::ChatterType::RESPONSE && evt.text().contains("pong"));
}
