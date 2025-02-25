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

#ifndef Q_RFC_EVALUATIONDIALOG_H
#define Q_RFC_EVALUATIONDIALOG_H

// Qt
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QIcon>
#include <QTextBrowser>

class EvaluationDialog : public QDialog
{
	Q_OBJECT

public:
	EvaluationDialog(std::string html, QWidget* parent = nullptr);

private:
	QTextBrowser* m_textBrowser;
	QPushButton* m_closeButton;
};

#endif // Q_RFC_EVALUATIONDIALOG_H
