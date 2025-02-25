//##########################################################################
//#                                                                        #
//#       CLOUDCOMPARE PLUGIN: Random Forest Classification Plugin         #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                           COPYRIGHT: Inria                             #
//#                                                                        #
//##########################################################################

#ifndef Q_RFC_NOTICE_DIALOG_HEADER
#define Q_RFC_NOTICE_DIALOG_HEADER

#include <ui_qRFCNoticeDialog.h>

//qCC_plugins
#include <ccMainAppInterface.h>

//Qt
#include <QMainWindow>

class qRFCNoticeDialog : public QDialog, public Ui::RFCNoticeDialog
{
public:
	//! Default constructor
	qRFCNoticeDialog(QWidget* parent = nullptr)
		: QDialog(parent)
		, Ui::RFCNoticeDialog()
	{
		setupUi(this);
	}
};

//whether disclaimer has already been displayed (and accepted) or not
static bool s_noticeAccepted = false;

static bool ShowNoticeDialog(ccMainAppInterface* app)
{
	if (!s_noticeAccepted)
	{
		s_noticeAccepted = qRFCNoticeDialog(app ? app->getMainWindow() : 0).exec();
	}

	return s_noticeAccepted;
}

#endif //Q_RFC_NOTICE_DIALOG_HEADER
