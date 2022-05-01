/***************************************************************************

    dialogs/stopwarning.h

    BletchMAME end emulation prompt

***************************************************************************/

#ifndef DIALOGS_STOPWARNING_H
#define DIALOGS_STOPWARNING_H

// Qt headers
#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class StopWarningDialog; }
class QTextStream;
QT_END_NAMESPACE

// ======================> StopWarningDialog

class StopWarningDialog : public QDialog
{
	Q_OBJECT

public:
	enum WarningType
	{
		Stop,
		Exit
	};

	// ctor/dtor
	StopWarningDialog(QWidget *parent, WarningType warningType);
	~StopWarningDialog();

	// accessors
	bool showThisChecked() const;
	void setShowThisChecked(bool checked);

private:
	std::unique_ptr<Ui::StopWarningDialog>    m_ui;
};


#endif // DIALOGS_STOPWARNING_H
