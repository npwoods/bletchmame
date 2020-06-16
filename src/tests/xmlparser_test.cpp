/***************************************************************************

    xmlparser_test.cpp

    Unit tests for xmlparser_test.cpp

***************************************************************************/

#include "xmlparser.h"
#include "test.h"

class XmlParser::Test : public QObject
{
    Q_OBJECT

private slots:
	void test();
	void unicode();
	void skipping();
	void multiple();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  test
//-------------------------------------------------

void XmlParser::Test::test()
{
	const bool INVALID_BOOL_VALUE = (bool)42;

	XmlParser xml;
	QString charlie_value;
	bool charlie_value_parsed = INVALID_BOOL_VALUE;
	int foxtrot_value = 0;
	bool foxtrot_value_parsed = INVALID_BOOL_VALUE;
	bool golf_value = INVALID_BOOL_VALUE;
	bool golf_value_parsed = INVALID_BOOL_VALUE;
	bool hotel_value = INVALID_BOOL_VALUE;
	bool hotel_value_parsed = INVALID_BOOL_VALUE;
	bool india_value = INVALID_BOOL_VALUE;
	bool india_value_parsed = INVALID_BOOL_VALUE;
	std::uint32_t julliet_value = INVALID_BOOL_VALUE;
	bool julliet_value_parsed = INVALID_BOOL_VALUE;
	float kilo_value = INVALID_BOOL_VALUE;
	bool kilo_value_parsed = INVALID_BOOL_VALUE;
	xml.OnElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		charlie_value_parsed = attributes.Get("charlie", charlie_value);
	});
	xml.OnElementBegin({ "alpha", "echo" }, [&](const XmlParser::Attributes &attributes)
	{
		foxtrot_value_parsed = attributes.Get("foxtrot", foxtrot_value);
		golf_value_parsed = attributes.Get("golf", golf_value);
		hotel_value_parsed = attributes.Get("hotel", hotel_value);
		india_value_parsed = attributes.Get("india", india_value);
		julliet_value_parsed = attributes.Get("julliet", julliet_value);
		kilo_value_parsed = attributes.Get("kilo", kilo_value);
	});

	const char *xml_text =
		"<alpha>"
		"<bravo charlie=\"delta\"/>"
		"<echo foxtrot=\"42\" hotel=\"on\" india=\"off\" julliet=\"2500000000\" kilo=\"3.14159\"/>/>"
		"</alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));

	QVERIFY(result);
	QVERIFY(charlie_value == "delta");
	QVERIFY(charlie_value_parsed);
	QVERIFY(foxtrot_value == 42);
	QVERIFY(foxtrot_value_parsed);
	QVERIFY(!golf_value);
	QVERIFY(!golf_value_parsed);
	QVERIFY(hotel_value);
	QVERIFY(hotel_value_parsed);
	QVERIFY(!india_value);
	QVERIFY(india_value_parsed);
	QVERIFY(julliet_value == 2500000000);
	QVERIFY(julliet_value_parsed);
	QVERIFY(abs(kilo_value - 3.14159f) < 0.000000001);
	QVERIFY(kilo_value_parsed);
}


//-------------------------------------------------
//  unicode
//-------------------------------------------------

void XmlParser::Test::unicode()
{
	XmlParser xml;
	QString bravo_value;
	QString charlie_value;
	xml.OnElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		bool result = attributes.Get("charlie", charlie_value);
		QVERIFY(result);
	});
	xml.OnElementEnd({ "alpha", "bravo" }, [&](QString &&value)
	{
		bravo_value = std::move(value);
	});

	const char *xml_text = "<alpha><bravo charlie=\"&#x6B7B;\">&#x60AA;</bravo></alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));
	QVERIFY(result);
	QVERIFY(bravo_value.toStdWString() == L"\u60AA");
	QVERIFY(charlie_value.toStdWString() == L"\u6B7B");
}


//-------------------------------------------------
//  skipping
//-------------------------------------------------

void XmlParser::Test::skipping()
{
	XmlParser xml;
	int expected_invocations = 0;
	int unexpected_invocations = 0;
	xml.OnElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		bool skip_value;
		attributes.Get("skip", skip_value);
		return skip_value ? XmlParser::element_result::SKIP : XmlParser::element_result::OK;
	});
	xml.OnElementBegin({ "alpha", "bravo", "expected" }, [&](const XmlParser::Attributes &)
	{
		expected_invocations++;
	});
	xml.OnElementBegin({ "alpha", "bravo", "unexpected" }, [&](const XmlParser::Attributes &)
	{
		unexpected_invocations++;
	});

	const char *xml_text =
		"<alpha>"
		"<bravo skip=\"no\"><expected/></bravo>"
		"<bravo skip=\"yes\"><unexpected/></bravo>"
		"</alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));
	QVERIFY(result);
	QVERIFY(expected_invocations == 1);
	QVERIFY(unexpected_invocations == 0);
}


//-------------------------------------------------
//  multiple
//-------------------------------------------------

void XmlParser::Test::multiple()
{
	XmlParser xml;
	int total = 0;
	xml.OnElementBegin({ { "alpha", "bravo" },
						 { "alpha", "charlie" },
						 { "alpha", "delta" } }, [&](const XmlParser::Attributes &attributes)
		{
			int value;
			attributes.Get("value", value);
			total += value;
		});

	const char *xml_text =
		"<alpha>"
		"<bravo value=\"2\" />"
		"<charlie value=\"3\" />"
		"<delta value=\"5\" />"
		"<echo value=\"-666\" />"
		"</alpha>";
	bool result = xml.ParseBytes(xml_text, strlen(xml_text));
	QVERIFY(result);
	QVERIFY(total == 10);
}


static TestFixture<XmlParser::Test> fixture;
#include "xmlparser_test.moc"
