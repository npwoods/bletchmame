/***************************************************************************

    validity.cpp

    Miscellaneous utility code

***************************************************************************/

#include "validity.h"
#include "utility.h"


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

	return true;
}
