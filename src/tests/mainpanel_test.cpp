/***************************************************************************

	mainpanel_test.cpp

	Unit tests for mainpanel.cpp

***************************************************************************/

#include "mainpanel.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void auditThisActionText_1()	{ auditThisActionText("",			"Audit This"); }
		void auditThisActionText_2()	{ auditThisActionText("Alpha",		"Audit \"Alpha\""); }
		void auditThisActionText_3()	{ auditThisActionText("Foo & Bar",	"Audit \"Foo && Bar\""); }

	private:
		void auditThisActionText(const QString &text, const QString &expected);
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  empty
//-------------------------------------------------

void Test::auditThisActionText(const QString &text, const QString &expected)
{
	QString result = MainPanel::auditThisActionText(text);
	QVERIFY(result == expected);
}


static TestFixture<Test> fixture;
#include "mainpanel_test.moc"
