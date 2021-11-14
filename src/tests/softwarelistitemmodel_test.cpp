/***************************************************************************

	softwarelistitemmodel_test.cpp

	Unit tests for softwarelistitemmodel.cpp

***************************************************************************/

#include "softwarelistitemmodel.h"
#include "test.h"

namespace
{
	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void general();
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  general
//-------------------------------------------------

void Test::general()
{
	// create a SoftwareListItemModel
	SoftwareListItemModel model(nullptr, [](const software_list::software &) { });

	// ensure that we can handle this even if we are empty
	model.allAuditStatusesChanged();
}


static TestFixture<Test> fixture;
#include "softwarelistitemmodel_test.moc"
