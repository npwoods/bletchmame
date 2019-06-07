/***************************************************************************

    validity.cpp

    Miscellaneous utility code

***************************************************************************/

#include "validity.h"
#include "utility.h"
#include "xmlparser.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

bool validity_checks()
{
	{
		auto result = util::string_split(std::string("Alpha,Bravo,Charlie"), [](char ch) { return ch == ','; });
		assert(result[0] == "Alpha");
		assert(result[1] == "Bravo");
		assert(result[2] == "Charlie");
	}

	{
		auto result = util::string_split(std::wstring(L"Alpha,Bravo,Charlie"), [](wchar_t ch) { return ch == ','; });
		assert(result[0] == L"Alpha");
		assert(result[1] == L"Bravo");
		assert(result[2] == L"Charlie");
	}

	{
		std::vector<std::string> vec = { "Alpha", "Bravo", "Charlie" };
		std::string result = util::string_join(std::string(","), vec);
		assert(result == "Alpha,Bravo,Charlie");
	}

	{
		std::vector<std::wstring> vec = { L"Alpha", L"Bravo", L"Charlie" };
		std::wstring result = util::string_join(std::wstring(L","), vec);
		assert(result == L"Alpha,Bravo,Charlie");
	}

	{
		XmlParser xml;
		wxString charlie_value;
		int foxtrot_value = 0;
		xml.OnElement({ "alpha", "bravo" }, [&](const XmlParser::Attributes &attributes)
		{
			assert(attributes.Get("charlie", charlie_value));
		});
		xml.OnElement({ "alpha", "echo" }, [&](const XmlParser::Attributes &attributes)
		{
			assert(attributes.Get("foxtrot", foxtrot_value));
		});

		bool result = xml.ParseXml(
			"<alpha>"
				"<bravo charlie=\"delta\"/>"
				"<echo foxtrot=\"42\"/>"
			"</alpha>");
		assert(result);
		assert(charlie_value == "delta");
		assert(foxtrot_value == 42);
	}

	return true;
}
