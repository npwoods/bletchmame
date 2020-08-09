/***************************************************************************

    dialogs/about.h

    BletchMAME About Dialog

***************************************************************************/

#ifndef DIALOGS_ABOUT_H
#define DIALOGS_ABOUT_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class AboutDialog; }
QT_END_NAMESPACE

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();

private:
    Ui::AboutDialog *ui;
};

#endif // DIALOGS_ABOUT_H
