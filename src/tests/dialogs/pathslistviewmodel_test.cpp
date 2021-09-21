/***************************************************************************

	pathslistviewmodel_test.cpp

	Test cases for pathslistviewmodel.cpp

***************************************************************************/

#include "dialogs/pathslistviewmodel.h"
#include "../test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void setPaths1();
		void setPaths2();
		void setPaths3();

	private:
		static bool myCanonicalizeAndValidateFunc(QString &path);
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  setPaths1
//-------------------------------------------------

void Test::setPaths1()
{
	PathsListViewModel model(&Test::myCanonicalizeAndValidateFunc);

	QStringList paths = { "/aLPHa/", "/BRaVo/" };
	model.setPaths(std::move(paths), true);

	QVERIFY(model.pathCount() == 2);
	QVERIFY(model.path(0) == "/alpha/");
	QVERIFY(model.path(1) == "/bravo/");
}


//-------------------------------------------------
//  setPaths2
//-------------------------------------------------

void Test::setPaths2()
{
	PathsListViewModel model(&Test::myCanonicalizeAndValidateFunc);

	QStringList paths = { "/aLPHa" };
	model.setPaths(std::move(paths), false);

	QVERIFY(model.pathCount() == 1);
	QVERIFY(model.path(0) == "/alpha");
}


//-------------------------------------------------
//  setPaths3
//-------------------------------------------------

void Test::setPaths3()
{
	PathsListViewModel model(&Test::myCanonicalizeAndValidateFunc);

	QStringList paths = { "/aLPHa", "/BRaVo" };
	model.setPaths(std::move(paths), false);

	QVERIFY(model.pathCount() == 1);
	QVERIFY(model.path(0) == "/alpha");
}


//-------------------------------------------------
//  myCanonicalizeAndValidateFunc
//-------------------------------------------------

bool Test::myCanonicalizeAndValidateFunc(QString &path)
{
	path = path.toLower();
	return true;
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "pathslistviewmodel_test.moc"
