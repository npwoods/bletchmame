/***************************************************************************

    softwarelist_test.cpp

    Unit tests for softwarelist_test.cpp

***************************************************************************/

#include <QDataStream>

#include "softwarelist.h"
#include "test.h"

class software_list::test : public QObject
{
    Q_OBJECT

private slots:
	void general();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void software_list::test::general()
{
	// get the test asset
	std::optional<std::string_view> asset = load_test_asset("softlist");
	if (!asset.has_value())
		return;

	// try to load it
	QByteArray byte_array(asset.value().data(), util::safe_static_cast<int>(asset.value().size()));
	QDataStream stream(byte_array);
	software_list softlist;
	QString error_message;
	bool success = softlist.load(stream, error_message);

	// did we succeed?
	if (!success || !error_message.isEmpty())
		throw false;

	for (const software_list::software &sw : softlist.get_software())
	{
		for (const software_list::part &part : sw.m_parts)
		{
			if (part.m_interface.isEmpty() || part.m_name.isEmpty())
				throw false;
		}
	}
}


static TestFixture<software_list::test> fixture;
#include "softwarelist_test.moc"
