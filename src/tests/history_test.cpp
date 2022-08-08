/***************************************************************************

	history_test.cpp

	Unit tests for history.cpp

***************************************************************************/

// bletchmame headers
#include "history.h"
#include "utility.h"
#include "test.h"


// ======================> HistoryDatabase::Test

class HistoryDatabase::Test : public QObject
{
	Q_OBJECT

private slots:
	void general();
	void toRichText_1() { return toRichText(u8"", ""); }
	void toRichText_2() { return toRichText(u8"foo bar", "foo bar"); }
	void toRichText_3() { return toRichText(u8"foo\nbar", "foo<br/>bar"); }
	void toRichText_4() { return toRichText(u8"foo < bar", "foo &lt; bar"); }
	void toRichText_5() { return toRichText(u8"foo > bar", "foo &gt; bar"); }
	void toRichText_6() { return toRichText(u8"foo & bar", "foo &amp; bar"); }
	void toRichText_7() { return toRichText(u8"httpptta httpptta", "httpptta httpptta"); }
	void toRichText_8() { return toRichText(u8"foo: https://www.contoso.com/ bar",
		"foo: <a href=\"https://www.contoso.com/\">https://www.contoso.com/</a> bar"); }
	void toRichText_9() { return toRichText(u8"https://www.contoso.com/1\nhttps://www.contoso.com/2",
		"<a href=\"https://www.contoso.com/1\">https://www.contoso.com/1</a><br/><a href=\"https://www.contoso.com/2\">https://www.contoso.com/2</a>"); }

private:
	void toRichText(std::u8string_view text, const QString &expected);
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void HistoryDatabase::Test::general()
{
	using namespace std::literals;

	HistoryDatabase hist;
	QVERIFY(hist.load(":/resources/history.xml"));

	std::u8string_view s1 = hist.get(MachineIdentifier(u8"rampage"sv));
	std::u8string_view s2 = hist.get(MachineIdentifier(u8"rampage2"sv));
	std::u8string_view s3 = hist.get(MachineIdentifier(u8"dummy"sv));
	std::u8string_view s4 = hist.get(SoftwareIdentifier(u8"nes"sv, u8"zelda"sv));
	std::u8string_view s5 = hist.get(SoftwareIdentifier(u8"nes"sv, u8"zeldau"sv));
	std::u8string_view s6 = hist.get(SoftwareIdentifier(u8"nes"sv, u8"dummy"sv));
	std::u8string_view s7 = hist.get(SoftwareIdentifier(u8"a2600"sv, u8"pacman"sv));

	QVERIFY(s1.size() == 11284);
	QVERIFY(s2.size() == 11284);
	QVERIFY(s3.size() == 0);
	QVERIFY(s4.size() == 537);
	QVERIFY(s5.size() == 537);
	QVERIFY(s6.size() == 0);
	QVERIFY(s7.size() == 4871);

	QVERIFY(s1 == util::trim(s1));
	QVERIFY(s2 == util::trim(s2));
	QVERIFY(s3 == util::trim(s3));
	QVERIFY(s4 == util::trim(s4));
	QVERIFY(s5 == util::trim(s5));
	QVERIFY(s6 == util::trim(s6));
	QVERIFY(s7 == util::trim(s7));
}


//-------------------------------------------------
//  toRichText
//-------------------------------------------------

void HistoryDatabase::Test::toRichText(std::u8string_view text, const QString &expected)
{
	QString actual = HistoryDatabase::toRichText(text);
	QVERIFY(actual == expected);
}


//-------------------------------------------------

static TestFixture<HistoryDatabase::Test> fixture;
#include "history_test.moc"
