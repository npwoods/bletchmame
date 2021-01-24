/***************************************************************************

    machinefoldertreemodel_test.cpp

    Unit tests for machinefoldertreemodel.cpp

***************************************************************************/

#include "machinefoldertreemodel.h"
#include "test.h"


class MachineFolderTreeModel::Test : public QObject
{
    Q_OBJECT

private slots:
    void allIconsLoad();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

void MachineFolderTreeModel::Test::allIconsLoad()
{
    auto folderIconResourceNames = getFolderIconResourceNames();

    for (const char *resourceName : folderIconResourceNames)
    {
        QVERIFY(resourceName);
        QFile file(resourceName);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.size() > 0);
    }
}


static TestFixture<MachineFolderTreeModel::Test> fixture;
#include "machinefoldertreemodel_test.moc"
