/***************************************************************************

    dialogs/about.h

    BletchMAME About Dialog

***************************************************************************/

#ifndef DIALOGS_ABOUT_H
#define DIALOGS_ABOUT_H

// bletchmame headers
#include "mameversion.h"

// Qt headers
#include <QDialog>

// standard headers
#include <memory>


QT_BEGIN_NAMESPACE
namespace Ui { class AboutDialog; }
class QTextStream;
QT_END_NAMESPACE

// ======================> AboutDialog

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
	class Test;

	explicit AboutDialog(QWidget *parent, const std::optional<MameVersion> &mameVersion);
	~AboutDialog();

private:
	std::unique_ptr<Ui::AboutDialog>    m_ui;

	static QDate getExeCreateDate();
	QString getAboutText(const std::optional<MameVersion> &mameVersion);
};

#endif // DIALOGS_ABOUT_H
