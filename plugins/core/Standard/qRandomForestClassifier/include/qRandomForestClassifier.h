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

#pragma once

#include "ccStdPluginInterface.h"

//! Random Forest Classification Plugin
class qRFC : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES( ccPluginInterface ccStdPluginInterface )
	Q_PLUGIN_METADATA( IID "cccorp.cloudcompare.plugin.qRandomForestClassifier.classification" FILE "../info.json" )

public:
	explicit qRFC( QObject *parent = nullptr );
	~qRFC() override = default;

	// Inherited from ccStdPluginInterface
	void onNewSelection( const ccHObject::Container &selectedEntities ) override;
	QList<QAction *> getActions() override;

protected:
	//! Perform point cloud classification given a trained classifier
	void doClassifyAction();
	//! Train point cloud classifier
	void doTrainAction();

	QAction* m_classifyAction;
	QAction* m_trainAction;

	//! Currently selected entities;
	ccHObject::Container m_selectedEntities;
};
