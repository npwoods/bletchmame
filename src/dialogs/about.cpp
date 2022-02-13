/***************************************************************************

    dialogs/about.h

    BletchMAME About Dialog

***************************************************************************/

// bletchmame headers
#include "about.h"
#include "ui_about.h"
#include "mameversion.h"

// Qt headers
#include <QDate>
#include <QFileInfo>
#include <QTextStream>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AboutDialog::AboutDialog(QWidget *parent, const std::optional<MameVersion> &mameVersion)
    : QDialog(parent)
{
	m_ui = std::make_unique<Ui::AboutDialog>();
	m_ui->setupUi(this);

	// set the "about..." text
	QString aboutText = getAboutText(mameVersion);
	m_ui->aboutTextLabel->setText(aboutText);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

AboutDialog::~AboutDialog()
{
}


//-------------------------------------------------
//  getExeCreateDate
//-------------------------------------------------

QDate AboutDialog::getExeCreateDate()
{
	QString applicationFilePath = QCoreApplication::applicationFilePath();
	if (applicationFilePath.isEmpty())
		return QDate();

	QFileInfo fi(applicationFilePath);
	if (!fi.isFile())
		return QDate();

	return fi.birthTime().date();
}


//-------------------------------------------------
//  getAboutText
//-------------------------------------------------

QString AboutDialog::getAboutText(const std::optional<MameVersion> &mameVersion)
{
	QString result;
	{
		QTextStream stream(&result);

		// core application info
		stream << QCoreApplication::applicationName() << " " << QCoreApplication::applicationVersion();
		QDate exeCreateDate = getExeCreateDate();
		if (exeCreateDate.isValid())
		{
			QString exeCreateDateString = QLocale::system().toString(exeCreateDate, QLocale::FormatType::ShortFormat);
			stream << QString(" (%1)").arg(exeCreateDateString);
		}
		stream << "\n\n";

		// MAME version
		if (mameVersion.has_value())
			stream << mameVersion->toPrettyString() << "\n";

		// Qt version
		stream << "Qt " << QT_VERSION_STR;
	}
	return result;
}
