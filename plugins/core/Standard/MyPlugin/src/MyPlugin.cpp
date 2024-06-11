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

#include <QtGui>

#include "MyPlugin.h"

#include "ActionA.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_2.h>
#include <vector>
typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2 Point_2;
typedef std::vector<Point_2> Points;

// Default constructor:
//	- pass the Qt resource path to the info.json file (from <yourPluginName>.qrc file) 
//  - constructor should mainly be used to initialize actions and other members
MyPlugin::MyPlugin( QObject *parent )
	: QObject( parent )
	, ccStdPluginInterface( ":/CC/plugin/MyPlugin/info.json" )
	, m_action( nullptr )
{
}

// This method should enable or disable your plugin actions
// depending on the currently selected entities ('selectedEntities').
void MyPlugin::onNewSelection( const ccHObject::Container &selectedEntities )
{
	if ( m_action == nullptr )
	{
		return;
	}
	
	// If you need to check for a specific type of object, you can use the methods
	// in ccHObjectCaster.h or loop and check the objects' classIDs like this:
	//
	//	for ( ccHObject *object : selectedEntities )
	//	{
	//		if ( object->getClassID() == CC_TYPES::VIEWPORT_2D_OBJECT )
	//		{
	//			// ... do something with the viewports
	//		}
	//	}
	
	// For example - only enable our action if something is selected.
	m_action->setEnabled( !selectedEntities.empty() );
}

// This method returns all the 'actions' your plugin can perform.
// getActions() will be called only once, when plugin is loaded.
QList<QAction *> MyPlugin::getActions()
{
	// default action (if it has not been already created, this is the moment to do it)
	if ( !m_action )
	{
		// Here we use the default plugin name, description, and icon,
		// but each action should have its own.
		m_action = new QAction( getName(), this );
		m_action->setToolTip( getDescription() );
		m_action->setIcon( getIcon() );
		
		// Connect appropriate signal
		connect( m_action, &QAction::triggered, this, [this]()
		{
			Example::performActionA( m_app );

			Points points, result;
			points.push_back(Point_2(0,0));
			points.push_back(Point_2(10,0));
			points.push_back(Point_2(10,10));
			points.push_back(Point_2(6,5));
			points.push_back(Point_2(4,1));
			CGAL::convex_hull_2( points.begin(), points.end(), std::back_inserter(result) );
			std::cout << result.size() << " points on the convex hull" << std::endl;
		});
	}

	return { m_action };
}
