/***************************************************************************

    machinefoldertreemodel_test.cpp

    Unit tests for machinefoldertreemodel.cpp

***************************************************************************/

// bletchmame headers
#include "machinefoldertreemodel.h"
#include "prefs.h"
#include "test.h"


class MachineFolderTreeModel::Test : public QObject
{
    Q_OBJECT

private slots:
    void createAndRefresh();
    void allIconsLoad();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  createAndRefresh
//-------------------------------------------------

void MachineFolderTreeModel::Test::createAndRefresh()
{
    // prerequisites
    info::database db;
    QVERIFY(db.load(buildInfoDatabase()));
    Preferences prefs;

    // create the model and refresh
    MachineFolderTreeModel model(nullptr, db, prefs);
    model.refresh();
}


//-------------------------------------------------
//  allIconsLoad
//-------------------------------------------------

void MachineFolderTreeModel::Test::allIconsLoad()
{
    auto folderIconResourceNames = getFolderIconResourceNames();

	for (const char* resourceName : folderIconResourceNames)
		QVERIFY(!tryLoadImage(resourceName));
}


static TestFixture<MachineFolderTreeModel::Test> fixture;
#include "machinefoldertreemodel_test.moc"
