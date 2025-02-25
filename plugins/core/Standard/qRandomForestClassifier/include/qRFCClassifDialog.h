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

#ifndef Q_RFC_CLASSIF_DIALOG_HEADER
#define Q_RFC_CLASSIF_DIALOG_HEADER

#include <ui_qRFCClassifDialog.h>

//local
#include "Classifier.h"
#include "qRFCTools.h"

//Qt
#include <QStandardItemModel>

class ccMainAppInterface;
class ccPointCloud;

//! RFC plugin's classification dialog
class qRFCClassifDialog : public QDialog, public Ui::RFCClassifDialog
{
	Q_OBJECT

public:

	//! Default constructor
	qRFCClassifDialog(ccPointCloud* cloud, ccMainAppInterface* app);

	//! Returns Classifier file name
	QString getClassifFilePath() const;

	//! Returns the regularization type
	int getRegType() const;
	//! Returns the minimum scale
	double getMinScale() const;
	//! Returns the number of scales
	int getNScale() const;
	//! Returns the number of classes
	int getNClasses() const;
	std::vector<int> getClassIndices() const;
	//! Returns whether the features should be outputted for the user
	bool getExportFeatures() const;
	//! Returns the labels for evaluating the given point cloud
	CCCoreLib::ScalarField* getEvaluationLabels() const;

	//! Loads parameters from persistent settings
	void loadParamsFromPersistentSettings();
	//! Saves parameters to persistent settings
	void saveParamsToPersistentSettings();

	//! Returns the selected features
	qRFCTools::FeatureList getFeatureList() const;
	void setFeatureList(qRFCTools::FeatureList);

	std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > getFeatures() const;

protected:

	bool validParameters(const bool showAdvice = false) const;
	bool validClassIndices() const;
	void browseClassifierFile();
	void loadClassifierFile(QString);
	void onClassIndicesChanged();
	void onFeatureCheckStateChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&);
	void onAddFeatureClicked();
	void onRemoveFeatureClicked();
	void onScalarFieldChanged(int);
	void onEvaluateCheckStateChanged(bool);

protected:

	//! Gives access to the application (data-base, UI, etc.)
	ccMainAppInterface* m_app;

	//! Associated cloud
	ccPointCloud* m_cloud;
	//List of features available
	QStandardItemModel* m_featureModel;
};

#endif //Q_RFC_CLASSIF_DIALOG_HEADER
