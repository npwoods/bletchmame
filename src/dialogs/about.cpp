/***************************************************************************

    dialogs/about.h

    BletchMAME About Dialog

***************************************************************************/

#include <QDate>
#include <QFileInfo>
#include <QTextStream>

#include "about.h"
#include "ui_about.h"
#include "mameversion.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AboutDialog::AboutDialog(QWidget *parent, const QString &mameVersion)
    : QDialog(parent)
{
	m_ui = std::make_unique<Ui::AboutDialog>();
	m_ui->setupUi(this);

	// get the "pretty" MAME version
	QString prettyMameVersion = getPrettyMameVersion(mameVersion);

	// set the "about..." text
	QString aboutText = getAboutText(prettyMameVersion);
	m_ui->aboutTextLabel->setText(aboutText);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

AboutDialog::~AboutDialog()
{
}


//-------------------------------------------------
//  getPrettyMameVersion
//-------------------------------------------------

QString AboutDialog::getPrettyMameVersion(const QString &mameVersion)
{
	QString result;
	if (!mameVersion.isEmpty())
	{
		auto version = MameVersion(mameVersion);
		if (!version.isDirty())
		{
			// simple MAME version (e.g. - "0.213 (mame0213)"); this should be the case when the user
			// is using an off the shelf version of MAME and we want to present a simple version string
			result = QString("MAME %1.%2").arg(
				QString::number(version.major()),
				QString::number(version.minor()));
		}
		else
		{
			// non-simple MAME version; probably an interim build
			result = QString("MAME ") + mameVersion;
		}
	}
	return result;
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

QString AboutDialog::getAboutText(const QString &prettyMameVersion)
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
		if (!prettyMameVersion.isEmpty())
			stream << prettyMameVersion << "\n";

		// Qt version
		stream << "Qt " << QT_VERSION_STR;
	}
	return result;
}
