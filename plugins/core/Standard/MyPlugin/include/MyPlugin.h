//##########################################################################
//#                                                                        #
//#                   CLOUDCOMPARE PLUGIN: MyPlugin                        #
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
//#                             COPYRIGHT: XXX                             #
//#                                                                        #
//##########################################################################

#pragma once

#include "ccStdPluginInterface.h"

//! Example qCC plugin
class MyPlugin : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES( ccPluginInterface ccStdPluginInterface )

	// Replace "Example" by your plugin name (IID should be unique - let's hope your plugin name is unique ;)
	// The info.json file provides information about the plugin to the loading system and
	// it is displayed in the plugin information dialog.
	Q_PLUGIN_METADATA( IID "cccorp.cloudcompare.plugin.MyPlugin" FILE "../info.json" )

public:
	explicit MyPlugin( QObject *parent = nullptr );
	~MyPlugin() override = default;

	// Inherited from ccStdPluginInterface
	void onNewSelection( const ccHObject::Container &selectedEntities ) override;
	QList<QAction *> getActions() override;

private:
	//! Default action
	/** You can add as many actions as you want in a plugin.
		Each action will correspond to an icon in the dedicated
		toolbar and an entry in the plugin menu.
	**/
	QAction* m_action;
};
