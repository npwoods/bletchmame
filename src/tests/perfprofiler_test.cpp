/***************************************************************************

	perfprofiler_test.cpp

	Unit tests for perfprofiler.cpp

***************************************************************************/

// bletchmame headers
#include "perfprofiler.h"
#include "test.h"

class ProfilerLabel::Test : public QObject
{
	Q_OBJECT

private slots:
	void ctor_1()				{ ctor("foo", "foo"); }
	void fromPrettyFunction_1() { fromPrettyFunction("alpha::bravo(Charlie&, const Delta&)::<lambda(std::u8string&&)>", "alpha::bravo(Charlie&, const Delta&)::<lambda(std::u8string&&)>"); }
	void equals();
	void hash();
	void toQString();

private:
	void ctor(const char *input, const char *expected);
	void fromPrettyFunction(const char *input, const char *expected);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ProfilerLabel::Test::ctor
//-------------------------------------------------

void ProfilerLabel::Test::ctor(const char *input, const char *expected)
{
	ProfilerLabel label(input);
	QVERIFY(!strncmp(&label.m_text[0], expected, std::size(label.m_text)));
	for (auto i = strlen(expected); i < label.m_text.size(); i++)
		QVERIFY(label.m_text[i] == '\0');
}


//-------------------------------------------------
//  ProfilerLabel::Test::fromPrettyFunction
//-------------------------------------------------

void ProfilerLabel::Test::fromPrettyFunction(const char *input, const char *expected)
{
	ProfilerLabel label = ProfilerLabel::fromPrettyFunction(input);
	std::u8string_view actual = label.toStringView();
	QVERIFY(actual == std::u8string_view((const char8_t *)expected));
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
