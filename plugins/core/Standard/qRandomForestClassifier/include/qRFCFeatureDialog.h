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

#ifndef Q_RFC_FEATUREDIALOG_H
#define Q_RFC_FEATUREDIALOG_H

// Qt
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QCheckBox>
#include <QIcon>

class FeatureDialog : public QDialog
{
	Q_OBJECT

public:
	FeatureDialog(QStringList items, QWidget* parent = nullptr);

	QStringList getSelections() const;

protected:
	void onCheckAllToggled(bool checked);

private:
	QListView* m_listView;
	QStandardItemModel* m_model;
	QCheckBox* m_checkAllBox;
	QPushButton* m_okButton;
	QPushButton* m_cancelButton;
};

#endif //Q_RFC_FEATUREDIALOG_H
