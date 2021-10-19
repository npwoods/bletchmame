/***************************************************************************

	perfprofiler_test.cpp

	Unit tests for perfprofiler.cpp

***************************************************************************/

#include "perfprofiler.h"
#include "test.h"

class ProfilerLabel::Test : public QObject
{
	Q_OBJECT

private slots:
	void ctor();
	void equals();
	void hash();
	void toQString();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ProfilerLabel::Test::ctor
//-------------------------------------------------

void ProfilerLabel::Test::ctor()
{
	ProfilerLabel label("foo");

	QVERIFY(label.m_text[0] == 'f');
	QVERIFY(label.m_text[1] == 'o');
	QVERIFY(label.m_text[2] == 'o');

	for (auto i = 3; i < label.m_text.size(); i++)
		QVERIFY(label.m_text[i] == '\0');
}


//-------------------------------------------------
//  ProfilerLabel::Test::equals
//-------------------------------------------------

void ProfilerLabel::Test::equals()
{
	ProfilerLabel x("foo");
	ProfilerLabel y("foo");
	ProfilerLabel z("bar");
	QVERIFY(x == y);
	QVERIFY(x != z);
}


//-------------------------------------------------
//  ProfilerLabel::Test::hash
//-------------------------------------------------

void ProfilerLabel::Test::hash()
{
	ProfilerLabel x("foo");
	ProfilerLabel y("foo");	
	std::hash<ProfilerLabel> hash;
	QVERIFY(hash(x) == hash(y));
}


//-------------------------------------------------
//  ProfilerLabel::Test::toQString
//-------------------------------------------------

void ProfilerLabel::Test::toQString()
{
	ProfilerLabel x("foo");
	QString y = x.toQString();
	QVERIFY(y == "foo");
}


//-------------------------------------------------

static TestFixture<ProfilerLabel::Test> fixture;
#include "perfprofiler_test.moc"
