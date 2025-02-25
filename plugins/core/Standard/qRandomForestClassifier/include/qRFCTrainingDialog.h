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

#ifndef Q_RFC_TRAINING_DIALOG_HEADER
#define Q_RFC_TRAINING_DIALOG_HEADER

#include <ui_qRFCTrainingDialog.h>

//local
#include "Classifier.h"
#include "qRFCTools.h"

//qCC_db
#include <ccPointCloud.h>

//Qt
#include <QStandardItemModel>

class ccMainAppInterface;
class ccPointCloud;

//! RFC plugin's training dialog
class qRFCTrainingDialog : public QDialog, public Ui::RFCTrainingDialog
{
	Q_OBJECT

public:

	//! Default constructor
	qRFCTrainingDialog(ccMainAppInterface* app);

	//! Get class #1 point cloud
	ccPointCloud* getClass1Cloud();
	//! Get class #2 point cloud
	ccPointCloud* getClass2Cloud();
	//! Get evaluation point cloud
	ccPointCloud* getEvaluationCloud();
	//! Get training point cloud when using scalar field
	std::pair<ccPointCloud*,ccScalarField*> getPointCloudAndScalarField();

	//! Loads parameters from persistent settings
	void loadParamsFromPersistentSettings();
	//! Saves parameters to persistent settings
	void saveParamsToPersistentSettings();

	//! Returns the input type (true: point clouds or false: scalar field)
	bool getInputType() const;
	//! Returns the minimum scale
	double getMinScale() const;
	//! Returns the number of scales
	int getNScale() const;
	//! Returns the number of scales
	int getNClasses() const;
	std::vector<int> getClassIndices() const;
	//! Returns the max number of core points to use
	int getMaxCorePoints() const;
	//! Returns whether the features should be outputted for the user
	bool getEvaluateParams() const;
	//! Returns the number of trees
	int getNTrees() const;
	//! Returns the maximum tree depth
	int getMaxTreeDepth() const;

	//! Returns the selected features
	qRFCTools::FeatureList getFeatureList() const;
	void setFeatureList(qRFCTools::FeatureList);
	//QVariant featureListToVariant(const qRFCTools::FeatureList&);
	//qRFCTools::FeatureList variantToFeatureList(const QVariant&);

	std::vector<std::pair<Features::Source, CCCoreLib::ScalarField*> > qRFCTrainingDialog::getFeatures(ccPointCloud*) const;

protected:

	void onClassChanged(int);
	void onCloudChanged(int);
	void onPointCloudsEnabled();
	void onScalarFieldsEnabled();
	void onPointCloudChanged(int);
	void onScalarFieldChanged(int);
	void onClassIndicesChanged();
	void onFeatureCheckStateChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&);
	void onAddFeatureClicked();
	void onRemoveFeatureClicked();

protected:

	//! Gives access to the application (data-base, UI, etc.)
	ccMainAppInterface* m_app;
	//List of features available
	QStandardItemModel* m_featureModel;
	//List of available clouds
	std::vector<ccHObject*> m_clouds;

	//Verifies whether the current parameters are valid or not
	bool validParameters(const bool showAdvice=false) const;
	bool validScalarField() const;
	bool validClassIndices() const;
};

#endif //Q_RFC_TRAINING_DIALOG_HEADER
