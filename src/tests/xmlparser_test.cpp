/***************************************************************************

    xmlparser_test.cpp

    Unit tests for xmlparser_test.cpp

***************************************************************************/

// bletchmame headers
#include "xmlparser.h"
#include "test.h"


class XmlParser::Test : public QObject
{
    Q_OBJECT

private slots:
	void test();
	void unicodeStdString();
	void unicodeQString();
	void skipping();
	void multiple();
	void localeSensitivity();
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
	std::optional<QString> charlie_value = { };
	std::optional<int> foxtrot_value = { };
	std::optional<bool> golf_value = INVALID_BOOL_VALUE;
	std::optional<bool> hotel_value = INVALID_BOOL_VALUE;
	std::optional<bool> india_value = INVALID_BOOL_VALUE;
	std::optional<std::uint32_t> julliet_value = { };
	std::optional<float> kilo_value = { };
	std::optional<std::uint64_t> lima_value = 0;
	std::optional<int> mike_value = 0;
	std::optional<std::uint32_t> november_value = 0;
	std::optional<std::uint64_t> oscar_value = 0;

	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		charlie_value = attributes.get<QString>("charlie");
	});
	xml.onElementBegin({ "alpha", "echo" }, [&](const XmlParser::Attributes &attributes)
	{
		foxtrot_value = attributes.get<int>("foxtrot");
		golf_value = attributes.get<bool>("golf");
		hotel_value = attributes.get<bool>("hotel");
		india_value = attributes.get<bool>("india");
		julliet_value = attributes.get<std::uint32_t>("julliet");
		kilo_value = attributes.get<float>("kilo");
		lima_value = attributes.get<std::uint64_t>("lima");
		mike_value = attributes.get<int>("mike", 16);
		november_value = attributes.get<std::uint32_t>("november", 16);
		oscar_value = attributes.get<std::uint64_t>("oscar", 16);
	});

	const char *xml_text =
		"<alpha>"
		"<bravo charlie=\"delta\"/>"
		"<echo foxtrot=\"42\" hotel=\"on\" india=\"off\" julliet=\"2500000000\" kilo=\"3.14159\" lima=\"72455405295\" mike=\"123ABC\" november=\"DEADBEEF\" oscar=\"BAADF00DBAADF00D\" />/>"
		"</alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));

	QVERIFY(result);
	QVERIFY(charlie_value == "delta");
	QVERIFY(foxtrot_value == 42);
	QVERIFY(golf_value == std::nullopt);
	QVERIFY(hotel_value == true);
	QVERIFY(india_value == false);
	QVERIFY(julliet_value == 2500000000);
	QVERIFY(abs(kilo_value.value() - 3.14159f) < 0.000000001);
	QVERIFY(lima_value == 0x10DEADBEEF);
	QVERIFY(mike_value == 0x123ABC);
	QVERIFY(november_value == 0xDEADBEEF);
	QVERIFY(oscar_value == 0xBAADF00DBAADF00D);
}


//-------------------------------------------------
//  unicodeStdString
//-------------------------------------------------

void XmlParser::Test::unicodeStdString()
{
	XmlParser xml;
	std::u8string bravo_value;
	std::optional<std::u8string> charlie_value;
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		charlie_value = attributes.get<std::u8string_view>("charlie");
	});
	xml.onElementEnd({ "alpha", "bravo" }, [&](std::u8string &&value)
	{
		bravo_value = std::move(value);
	});

	const char *xml_text = "<alpha><bravo charlie=\"&#x6B7B;\">&#x60AA;</bravo></alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));
	QVERIFY(result);
	QVERIFY(bravo_value == u8"\u60AA");
	QVERIFY(charlie_value.has_value());
	QVERIFY(charlie_value == u8"\u6B7B");
}


//-------------------------------------------------
//  unicodeQString
//-------------------------------------------------

void XmlParser::Test::unicodeQString()
{
	XmlParser xml;
	QString bravo_value;
	std::optional<QString> charlie_value;
	xml.onElementBegin({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
	{
		charlie_value = attributes.get<QString>("charlie");
	});
	xml.onElementEnd({ "alpha", "bravo" }, [&](QString &&value)
	{
		bravo_value = std::move(value);
	});

	const char *xml_text = "<alpha><bravo charlie=\"&#x6B7B;\">&#x60AA;</bravo></alpha>";
	bool result = xml.parseBytes(xml_text, strlen(xml_text));
	QVERIFY(result);
	QVERIFY(bravo_value.toStdWString() == L"\u60AA");
	QVERIFY(charlie_value.has_value());
	QVERIFY(charlie_value->toStdWString() == L"\u6B7B");
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
		bool skip_value = attributes.get<bool>("skip").value();
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
		int value = attributes.get<int>("value").value();
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
//  localeSensitivity - checks to see if we have
//	problems due to sensitivity on the current locale
// 
//	this was bug #143
//-------------------------------------------------

void XmlParser::Test::localeSensitivity()
{
	// set the locale to a locale that does not use periods as decimal separators; this
	// is not exactly thread safe, but this is good enough for the purposes of unit testing
	TestLocaleOverride override("it-IT");

	XmlParser xml;
	std::optional<float> f = 0;
	xml.onElementBegin({ "alpha" }, [&](const XmlParser::Attributes &attributes)
	{
		f = attributes.get<float>("bravo");
	});

	const char *xmlText = "<alpha bravo=\"1.234\"/>";
	bool result = xml.parseBytes(xmlText, strlen(xmlText));
	QVERIFY(result);
	QVERIFY(f.has_value());
	QVERIFY(std::abs(*f - 1.234) < 0.001);
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
		int value = attributes.get<int>("value").value();
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
		value = attributes.get<T>("value");
	});

	QVERIFY(!xml.parseBytes(xmlText, strlen(xmlText)));
	QVERIFY(xml.m_errors.size() == 1);
	QVERIFY(!value.has_value());
}


//-------------------------------------------------

static TestFixture<XmlParser::Test> fixture;
#include "xmlparser_test.moc"
