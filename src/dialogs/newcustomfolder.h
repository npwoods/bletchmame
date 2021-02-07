/***************************************************************************

    dialogs/newcustomfolder.h

    Dialog for adding custom folders

***************************************************************************/

#ifndef DIALOGS_NEWCUSTOMFOLDER_H
#define DIALOGS_NEWCUSTOMFOLDER_H

#include <QDialog>
#include <set>

QT_BEGIN_NAMESPACE
namespace Ui { class NewCustomFolderDialog; }
QT_END_NAMESPACE

class NewCustomFolderDialog : public QDialog
{
    Q_OBJECT

public:
    // ctor/dtor
    NewCustomFolderDialog(std::function<bool(const QString &)> customFolderExistsFunc, QWidget *parent);
    ~NewCustomFolderDialog();

    // accessors
    QString newCustomFolderName() const;

private slots:
    void on_lineEdit_textChanged();

private:
    std::unique_ptr<Ui::NewCustomFolderDialog>  m_ui;
    std::function<bool(const QString &)>        m_customFolderExistsFunc;
   
    void update();
};


#endif // DIALOGS_NEWCUSTOMFOLDER_H
