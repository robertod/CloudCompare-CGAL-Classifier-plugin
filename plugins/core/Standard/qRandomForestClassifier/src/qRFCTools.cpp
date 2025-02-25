#include "qRFCTools.h"

//CCCoreLib
#include <DistanceComputationTools.h>
#include <Neighbourhood.h>
#include <ParallelSort.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

//qCC_plugins
#include <ccMainAppInterface.h>
#include <ccQtHelpers.h>

//Qt
#include <QApplication>
#include <QComboBox>
#include <QMainWindow>
#include <QtConcurrentMap>

QString qRFCTools::GetEntityName(ccHObject* obj)
{
	if (!obj)
	{
		assert(false);
		return QString();
	}

	QString name = obj->getName();
	if (name.isEmpty())
		name = "unnamed";
	name += QString(" [ID %1]").arg(obj->getUniqueID());

	return name;
}
ccPointCloud* qRFCTools::GetCloudFromCombo(QComboBox* comboBox, ccHObject* dbRoot)
{
	assert(comboBox && dbRoot);
	if (!comboBox || !dbRoot)
	{
		assert(false);
		return nullptr;
	}

	//return the cloud currently selected in the combox box
	int index = comboBox->currentIndex();
	if (index < 0)
	{
		assert(false);
		return nullptr;
	}
	unsigned uniqueID = comboBox->itemData(index).toUInt();
	ccHObject* item = dbRoot->find(uniqueID);
	if (!item || !item->isA(CC_TYPES::POINT_CLOUD))
	{
		assert(false);
		return nullptr;
	}
	return static_cast<ccPointCloud*>(item);
}
QVariant qRFCTools::featureListToVariant(const FeatureList& features)
{
	QVariantList variantList;
	for (const auto& feature : features)
	{
		QVariantMap map;
		map["name"] = feature.name;
		map["data"] = feature.data;
		map["checked"] = feature.checked;
		variantList.append(map);
	}
	return variantList;
}
qRFCTools::FeatureList qRFCTools::variantToFeatureList(const QVariant& variant)
{
	FeatureList features;
	QVariantList variantList = variant.toList();
	for (const auto& item : variantList)
	{
		QVariantMap map = item.toMap();
		QString name = map["name"].toString();
		QVariant data = map["data"];
		Qt::CheckState checked = Qt::CheckState(map["checked"].toInt());
		features.push_back({ name,data,checked });
	}
	return features;
}
bool qRFCTools::validScalarField(CCCoreLib::ScalarField* SF)
{
	std::set<float> unique_values;
	for (size_t i = 0; i < SF->size(); ++i)
	{
		unique_values.insert(SF->getValue(i));
	}

	for (float value : unique_values)
	{
		if (value != int(value))
			return false;
	}

	return true;
}
