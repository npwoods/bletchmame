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
	void xmlParsingError();

	void attributeParsingError_int_1()		{ attributeParsingError<int>("<alpha><bravo value=\"NOT_AN_INTEGER\"/></alpha>"); }
	void attributeParsingError_int_2()		{ attributeParsingError<int>("<alpha><bravo value=\"42_NOT_AN_INTEGER_42\"/></alpha>"); }
	void attributeParsingError_int_3()		{ attributeParsingError<int>("<alpha><bravo value=\"99999999999999999999999999999\"/></alpha>"); }
	void attributeParsingError_uint_1()		{ attributeParsingError<unsigned int>("<alpha><bravo value=\"-999\"/></alpha>"); }
	void attributeParsingError_float_1()	{ attributeParsingError<float>("<alpha><bravo value=\"NOT_A_FLOAT\"/></alpha>"); }
	void attributeParsingError_float_2()	{ attributeParsingError<float>("<alpha><bravo value=\"42_NOT_A_FLOAT_42\"/></alpha>"); }

private:
	template<class T> void attributeParsingError(const char *xml);
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
	std::uint64_t lima_value = 0;
	bool lima_value_parsed = INVALID_BOOL_VALUE;
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		charlie_value_parsed = attributes.get("charlie", charlie_value);
	});
	xml.onElementBegin({ "alpha", "echo" }, [&](const XmlParser::Attributes &attributes)
	{
		foxtrot_value_parsed = attributes.get("foxtrot", foxtrot_value);
		golf_value_parsed = attributes.get("golf", golf_value);
		hotel_value_parsed = attributes.get("hotel", hotel_value);
		india_value_parsed = attributes.get("india", india_value);
		julliet_value_parsed = attributes.get("julliet", julliet_value);
		kilo_value_parsed = attributes.get("kilo", kilo_value);
		lima_value_parsed = attributes.get("lima", lima_value);
	});

	const char *xml_text =
		"<alpha>"
		"<bravo charlie=\"delta\"/>"
		"<echo foxtrot=\"42\" hotel=\"on\" india=\"off\" julliet=\"2500000000\" kilo=\"3.14159\" lima=\"72455405295\"/>/>"
		"</alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));

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
	QVERIFY(lima_value == 0x10DEADBEEF);
	QVERIFY(lima_value_parsed);
}


//-------------------------------------------------
//  unicode
//-------------------------------------------------

void XmlParser::Test::unicode()
{
	XmlParser xml;
	QString bravo_value;
	QString charlie_value;
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		bool result = attributes.get("charlie", charlie_value);
		QVERIFY(result);
	});
	xml.onElementEnd({ "alpha", "bravo" }, [&](QString &&value)
	{
		bravo_value = std::move(value);
	});

	const char *xml_text = "<alpha><bravo charlie=\"&#x6B7B;\">&#x60AA;</bravo></alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));
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
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		bool skip_value;
		attributes.get("skip", skip_value);
		return skip_value ? XmlParser::ElementResult::Skip : XmlParser::ElementResult::Ok;
	});
	xml.onElementBegin({ "alpha", "bravo", "expected" }, [&](const XmlParser::Attributes &)
	{
		expected_invocations++;
	});
	xml.onElementBegin({ "alpha", "bravo", "unexpected" }, [&](const XmlParser::Attributes &)
	{
		unexpected_invocations++;
	});

	const char *xml_text =
		"<alpha>"
		"<bravo skip=\"no\"><expected/></bravo>"
		"<bravo skip=\"yes\"><unexpected/></bravo>"
		"</alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));
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
	xml.onElementBegin({ { "alpha", "bravo" },
						 { "alpha", "charlie" },
						 { "alpha", "delta" } }, [&](const XmlParser::Attributes &attributes)
		{
			int value;
			attributes.get("value", value);
			total += value;
		});

	const char *xml_text =
		"<alpha>"
		"<bravo value=\"2\" />"
		"<charlie value=\"3\" />"
		"<delta value=\"5\" />"
		"<echo value=\"-666\" />"
		"</alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));
	QVERIFY(result);
	QVERIFY(total == 10);
}


//-------------------------------------------------
//  xmlParsingError
//-------------------------------------------------

void XmlParser::Test::xmlParsingError()
{
	XmlParser xml;
	std::vector<int> values;
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		int value;
		attributes.get("value", value);
		values.push_back(value);
	});

	const char *xmlText1 =
		"<alpha---blahhhrg>"
		"</alpha>";
	QVERIFY(!xml.parseBytes(xmlText1, strlen(xmlText1)));
	QVERIFY(xml.m_errors.size() == 1);
	QVERIFY(values.size() == 0);
}


//-------------------------------------------------
//  attributeParsingError
//-------------------------------------------------

template<class T>
void XmlParser::Test::attributeParsingError(const char *xmlText)
{
	XmlParser xml;
	std::optional<T> value;
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		T x;
		if (attributes.get("value", x))
			value = x;
	});

	QVERIFY(!xml.parseBytes(xmlText, strlen(xmlText)));
	QVERIFY(xml.m_errors.size() == 1);
	QVERIFY(!value.has_value());
}


//-------------------------------------------------

static TestFixture<XmlParser::Test> fixture;
#include "xmlparser_test.moc"
