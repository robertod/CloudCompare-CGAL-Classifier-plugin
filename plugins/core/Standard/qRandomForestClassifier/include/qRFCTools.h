//##########################################################################
//#                                                                        #
//#       CLOUDCOMPARE PLUGIN: Random Forest Classification Plugin         #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#         COPYRIGHT: UNIVERSITE EUROPEENNE DE BRETAGNE / Inria           #
//#                                                                        #
//##########################################################################

#ifndef Q_RFC_TOOLS_HEADER
#define Q_RFC_TOOLS_HEADER

//system
#include <vector>

//qCC_db
#include <ccPointCloud.h>

//Qt
#include <Qt>
#include <QVariant>
#include <QString>

class QComboBox;
class QString;

class ccGenericPointCloud;
class ccPointCloud;
class ccHObject;
class ccMainAppInterface;
class ccScalarField;

namespace CCCoreLib {
	class GenericProgressCallback;
	class DgmOctree;
}

//! RFC generic tools
class qRFCTools
{
public:
	//! Returns a long description of a given entity (name + [ID])
	static QString GetEntityName(ccHObject* obj);

	//! Returns the cloud associated to the currently selected item of a combox box
	/** Relies on the item associated data (itemData) that should be equal to the cloud's ID.
	**/
	static ccPointCloud* GetCloudFromCombo(QComboBox* comboBox, ccHObject* dbRoot);

	// Qt tools
	struct FeatureListItem {
		QString name;
		QVariant data;
		Qt::CheckState checked;
	};
	typedef std::vector<FeatureListItem> FeatureList;
	static QVariant featureListToVariant(const FeatureList&);
	static FeatureList variantToFeatureList(const QVariant&);

	// Determine whether a scalar field is a list of integers
	static bool validScalarField(CCCoreLib::ScalarField* SF);
};

#endif //Q_RFC_TOOLS_HEADER
