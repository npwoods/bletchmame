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
		assert(result[0] == "Alpha");
		assert(result[1] == "Bravo");
		assert(result[2] == "Charlie");
	}

	return true;
}
