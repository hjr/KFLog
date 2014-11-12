/***********************************************************************
 **
 **   TaskEditor.cpp
 **
 **   This file is part of KFLog.
 **
 ************************************************************************
 **
 **   Copyright (c):  2002 by Harald Maier
 **                   2011-2014 by Axel Pauli
 **
 **   This file is distributed under the terms of the General Public
 **   License. See the file COPYING for more information.
 **
 ***********************************************************************/

#ifdef QT_5
    #include <QtWidgets>
#else
    #include <QtGui>
#endif

#include "mapcalc.h"
#include "mapcontents.h"
#include "mainwindow.h"
#include "MetaTypes.h"
#include "TaskEditor.h"

extern MainWindow*    _mainWindow;
extern MapConfig*     _globalMapConfig;
extern MapContents*   _globalMapContents;
extern MapMatrix*     _globalMapMatrix;
extern QSettings       _settings;

TaskEditor::TaskEditor( QWidget *parent ) :
  QDialog(parent),
  pTask(0),
  _task(QString("task"))
{
  setWindowTitle(tr("Task Editor") );
  setModal( true );
  createDialog();
  setMinimumWidth(500);
  setMinimumHeight(300);
  restoreGeometry( _settings.value("/TaskEditor/Geometry").toByteArray() );
  show();
}

TaskEditor::~TaskEditor()
{
  _settings.setValue( "/TaskEditor/Geometry", saveGeometry() );
}

/** No descriptions */
void TaskEditor::createDialog()
{
  QLabel *l;
  QPushButton *b;

  errorFai    = new QErrorMessage( this );
  errorRoute  = new QErrorMessage( this );

  errorFai->setWindowTitle( tr("Task selection") );
  errorFai->resize(500, 250);
  errorRoute->setWindowTitle( tr("Task selection") );
  errorRoute->resize(500, 250);

  QVBoxLayout *topLayout = new QVBoxLayout( this );
  topLayout->setSpacing( 10 );

  QHBoxLayout *header = new QHBoxLayout;
  QHBoxLayout *type = new QHBoxLayout;
  QHBoxLayout *buttons = new QHBoxLayout;
  QVBoxLayout *leftLayout = new QVBoxLayout;
  QVBoxLayout *middleLayout = new QVBoxLayout;
  QVBoxLayout *rightLayout = new QVBoxLayout;
  QHBoxLayout *topGroup = new QHBoxLayout;
  QHBoxLayout *smallButtons = new QHBoxLayout;

  buttons->addStretch();
  b = new QPushButton(tr("&Ok"), this);
  b->setDefault(true);
  connect(b, SIGNAL(clicked()), SLOT(slotAccept()));
  buttons->addWidget(b);
  b = new QPushButton(tr("&Cancel"), this);
  connect(b, SIGNAL(clicked()), SLOT(reject()));
  buttons->addWidget(b);

  //----------------------------------------------------------------------------
  // Row 1
  name = new QLineEdit;
  name->setReadOnly(false);
  l = new QLabel(tr("Name") + ":");
  header->addWidget(l);
  header->addWidget(name);

  //----------------------------------------------------------------------------
  // Row 2
  // Create a combo box for task types
  planningTypes = new QComboBox;
  connect( planningTypes, SIGNAL(activated(const QString&)),
           SLOT(slotSetPlanningType(const QString&)) );

  l = new QLabel( tr( "Task Type" ) + ":" );
  l->setBuddy( planningTypes );

  // Load task types into combo box
  planningTypes->addItems( FlightTask::ttGetSortedTranslationList() );

  // Create an non-exclusive button group
  QButtonGroup *bgrp2 = new QButtonGroup( this );
  connect(bgrp2, SIGNAL(buttonClicked(int)), SLOT(slotSetPlanningDirection(int)));
  bgrp2->setExclusive(false);

  // insert 2 check buttons
  left = new QCheckBox(tr("left"));
  left->setChecked(true);
  bgrp2->addButton( left, 0 );
  right = new QCheckBox(tr("right"));
  left->setChecked(false);
  bgrp2->addButton( right, 1 );

  // Align check boxes into a group box.
  QGroupBox *faiGroupBox = new QGroupBox(tr("Side of FAI area"));

  QHBoxLayout *hbBox = new QHBoxLayout;
  hbBox->addWidget(left);
  hbBox->addWidget(right);
  faiGroupBox->setLayout(hbBox);

  taskType = new QLabel;
  taskType->setMinimumWidth(100);

  type->addWidget(l);
  type->addWidget(planningTypes);
  type->addWidget(faiGroupBox);
  type->addStretch(1);
  type->addWidget(taskType);

  //----------------------------------------------------------------------------
  // Row 3
  route =  new KFLogTreeWidget("TaskEditor-Route");

  route->setSortingEnabled( false );
  route->setAllColumnsShowFocus( true );
  route->setFocusPolicy( Qt::StrongFocus );
  route->setRootIsDecorated( false );
  route->setItemsExpandable( true );
  route->setSelectionMode( QAbstractItemView::SingleSelection );
  route->setSelectionBehavior( QAbstractItemView::SelectRows );
  route->setAlternatingRowColors( true );
  route->addRowSpacing( 5 );
  route->setColumnCount( 5 );

  route->setDragEnabled(true);
  route->viewport()->setAcceptDrops(true);
  route->setDropIndicatorShown(true);
  route->setDragDropMode(QAbstractItemView::InternalMove);

  QStringList headerLabels;

  headerLabels  << tr("Type")
                << tr("Taskpoint")
                << tr("Length")
                << tr("Course")
                << tr("");

  route->setHeaderLabels( headerLabels );

  QTreeWidgetItem* headerItem = route->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );
  headerItem->setTextAlignment( 1, Qt::AlignCenter );
  headerItem->setTextAlignment( 2, Qt::AlignCenter );
  headerItem->setTextAlignment( 3, Qt::AlignCenter );

  connect( route, SIGNAL(itemClicked( QTreeWidgetItem*, int )),
           this, SLOT( slotItemClicked( QTreeWidgetItem*, int )) );

  colRouteType     = 0;
  colRouteWaypoint = 1;
  colRouteDist     = 2;
  colRouteCourse   = 3;
  colRouteDummy    = 4;

  route->loadConfig();

  l = new QLabel(tr("Task"));
  l->setBuddy( route );

  smallButtons->addStretch(1);
  upCmd = new QPushButton(this);
  upCmd->setIcon(_mainWindow->getPixmap("kde_up_16.png"));
  upCmd->setIconSize(QSize(16, 16));
  upCmd->setToolTip( tr("Moves selected task point up"));
  connect(upCmd, SIGNAL(clicked()), SLOT(slotMoveUp()));
  smallButtons->addWidget(upCmd);

  downCmd = new QPushButton(this);
  downCmd->setIcon(_mainWindow->getPixmap("kde_down_16.png"));
  downCmd->setIconSize(QSize(16, 16));
  downCmd->setToolTip( tr("Moves selected task point down"));
  connect(downCmd, SIGNAL(clicked()), SLOT(slotMoveDown()));
  smallButtons->addWidget(downCmd);
  smallButtons->addStretch(1);

  leftLayout->addWidget(l);
  leftLayout->addWidget(route);
  leftLayout->addLayout(smallButtons);

  middleLayout->addStretch(1);
  addCmd = new QPushButton;
  addCmd->setIcon(_mainWindow->getPixmap("kde_back_16.png"));
  addCmd->setIconSize(QSize(16, 16));
  addCmd->setToolTip( tr("Adds the selected waypoint to the task list"));
  connect(addCmd, SIGNAL(clicked()), SLOT(slotAddWaypoint()));
  middleLayout->addWidget(addCmd);

  invertCmd = new QPushButton(this);
  invertCmd->setIcon(_mainWindow->getPixmap("kde_reload_16.png"));
  invertCmd->setIconSize(QSize(16, 16));
  invertCmd->setToolTip( tr("Inverts the task. Last point becomes the first point, a.s.o."));
  connect(invertCmd, SIGNAL(clicked()), SLOT(slotInvertWaypoints()));
  middleLayout->addWidget(invertCmd);

  removeCmd = new QPushButton;
  removeCmd->setIcon(_mainWindow->getPixmap("kde_forward_16.png"));
  removeCmd->setIconSize(QSize(16, 16));
  removeCmd->setToolTip( tr("Removes the selected task point from the task list"));
  connect(removeCmd, SIGNAL(clicked()), SLOT(slotRemoveWaypoint()));
  middleLayout->addWidget(removeCmd);
  middleLayout->addStretch(1);

  waypoints = new KFLogTreeWidget("TaskEditor-Waypoints");
  waypoints->setSortingEnabled( true );
  waypoints->setAllColumnsShowFocus( true );
  waypoints->setFocusPolicy( Qt::StrongFocus );
  waypoints->setRootIsDecorated( false );
  waypoints->setItemsExpandable( true );
  waypoints->setSelectionMode( QAbstractItemView::SingleSelection );
  waypoints->setSelectionBehavior( QAbstractItemView::SelectRows );
  waypoints->setAlternatingRowColors( true );
  waypoints->addRowSpacing( 5 );
  waypoints->setColumnCount( 5 );

  headerLabels.clear();

  headerLabels  << tr("Name")
                << tr("Description")
                << tr("Country")
                << tr("ICAO")
                << tr("");

  waypoints->setHeaderLabels( headerLabels );

  headerItem = waypoints->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );
  headerItem->setTextAlignment( 1, Qt::AlignCenter );
  headerItem->setTextAlignment( 2, Qt::AlignCenter );
  headerItem->setTextAlignment( 3, Qt::AlignCenter );

  colWpName        = 0;
  colWpDescription = 1;
  colWpCountry     = 2;
  colWpIcao        = 3;
  colWpDummy       = 4;

  waypoints->loadConfig();

  QHBoxLayout* hbox = new QHBoxLayout;
  hbox->setMargin(0);
  hbox->addWidget(new QLabel(tr("Point Source:")));

  m_pointSourceBox = new QComboBox;
  hbox->addWidget( m_pointSourceBox );
  hbox->addStretch( 10 );

  rightLayout->addLayout( hbox );
  rightLayout->addWidget(waypoints);

  topGroup->addLayout(leftLayout);
  topGroup->addLayout(middleLayout);
  topGroup->addLayout(rightLayout);

  topLayout->addLayout(header);
  topLayout->addLayout(type);
  topLayout->addLayout(topGroup);
  topLayout->addLayout(buttons);

  setEntriesInPointSourceBox();
  enableCommandButtons();
}

void TaskEditor::setEntriesInPointSourceBox()
{
  if( m_pointSourceBox == 0 )
    {
      return;
    }

  // Gets all available lists from MapContents
  QList<Waypoint*> &wpList = _globalMapContents->getWaypointList();

  QList<Airfield> &airfieldList = _globalMapContents->getAirfieldList();

  QList<Airfield> &gliderfieldList = _globalMapContents->getGliderfieldList();

  QList<Airfield> &outLandingList = _globalMapContents->getOutLandingList();

  QList<RadioPoint> &navaidList = _globalMapContents->getNavaidList();

  QList<SinglePoint> &hotspotList = _globalMapContents->getHotspotList();

  m_pointSourceBox->clear();

  if( wpList.size() > 0 )
    {
      m_pointSourceBox->addItem( tr("Waypoints"), Waypoints );
    }

  if( airfieldList.size() > 0 || gliderfieldList.size() > 0 )
    {
      m_pointSourceBox->addItem( tr("Airfields"), Airfields );
    }

  if( outLandingList.size() > 0 )
    {
      m_pointSourceBox->addItem( tr("Outlandings"), Outlandings );
    }

  if( hotspotList.size() > 0 )
    {
      m_pointSourceBox->addItem( tr("Hotspots"), Hotspots );
    }

  if( navaidList.size() > 0 )
    {
      m_pointSourceBox->addItem( tr("Navaids"), Navaids );
    }

  if( m_pointSourceBox->count() == 0 )
    {
      m_pointSourceBox->addItem( tr("No point data found"), None );
    }
  else
    {
      // Activate point list loading, if combo box index is changed.
      connect( m_pointSourceBox, SIGNAL(currentIndexChanged(int)),
	       SLOT(slotLoadSelectableWaypoints(int)) );

      if( m_pointSourceBox->currentIndex() != 0 )
	{
	  m_pointSourceBox->setCurrentIndex( 0 );
	}
      else
	{
	  slotLoadSelectableWaypoints( 0 );
	}
    }
}

void TaskEditor::slotLoadSelectableWaypoints( int index )
{
  if( m_pointSourceBox == 0 || m_pointSourceBox->count() == 0 )
    {
      return;
    }

  int selectedItem = m_pointSourceBox->itemData(index).toInt();

  // Check, which point source is selected.
  if( selectedItem == None )
    {
      return;
    }

  // Clear the current content in the list.
  waypoints->clear();

  if( selectedItem == Waypoints )
    {
      // Gets current waypoint list from MapContents
      QList<Waypoint*> &wpList = _globalMapContents->getWaypointList();

      for( int i = 0; i < wpList.size(); i++ )
	{
	  Waypoint* wp = wpList.at(i);

	  QTreeWidgetItem *item = new QTreeWidgetItem;

	  item->setText(colWpName, wp->name);
	  item->setIcon(colWpName, _globalMapConfig->getPixmap(wp->type, false, true));
	  item->setText(colWpDescription, wp->description);
	  item->setText(colWpCountry, wp->country);
	  item->setText(colWpIcao, wp->icao);

	  item->setTextAlignment( colWpName, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpDescription, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpCountry, Qt::AlignCenter );
	  item->setTextAlignment( colWpIcao, Qt::AlignCenter );

	  QVariant v;
	  v.setValue(wp);
	  item->setData( 0, Qt::UserRole, v );
	  waypoints->insertTopLevelItem( i, item );
	}
    }
  else if( selectedItem == Airfields )
    {
      QList<Airfield>* lists[2];

      // Gets current airfield list from MapContents
      lists[0] = &_globalMapContents->getAirfieldList();

      // Gets current gliderfield list from MapContents
      lists[1] = &_globalMapContents->getGliderfieldList();

      for( int i = 0; i < 2; i++ )
	{
	  for( int k = 0; k < lists[i]->size(); k++ )
	    {
	      Airfield& af = const_cast<Airfield &>(lists[i]->at(k));

	      QTreeWidgetItem *item = new QTreeWidgetItem;

	      item->setText(colWpName, af.getShortName() );
	      item->setIcon(colWpName, _globalMapConfig->getPixmap(af.getTypeID(), false, true) );
	      item->setText(colWpDescription, af.getName() );
	      item->setText(colWpCountry, af.getCountry() );
	      item->setText(colWpIcao, af.getICAO() );

	      item->setTextAlignment( colWpName, Qt::AlignLeft|Qt::AlignVCenter );
	      item->setTextAlignment( colWpDescription, Qt::AlignLeft|Qt::AlignVCenter );
	      item->setTextAlignment( colWpCountry, Qt::AlignCenter );
	      item->setTextAlignment( colWpIcao, Qt::AlignCenter );

	      QVariant v;
	      v.setValue(&af);
	      item->setData( 0, Qt::UserRole, v );
	      waypoints->insertTopLevelItem( i, item );
	    }
	}
    }
  else if( selectedItem == Outlandings )
    {
      // Gets current outlanding list from MapContents
      QList<Airfield>& list = _globalMapContents->getOutLandingList();

      for( int i = 0; i < list.size(); i++ )
	{
	  Airfield &af = list[i];

	  QTreeWidgetItem *item = new QTreeWidgetItem;

	  item->setText(colWpName, af.getShortName() );
	  item->setIcon(colWpName, _globalMapConfig->getPixmap(af.getTypeID(), false, true) );
	  item->setText(colWpDescription, af.getName() );
	  item->setText(colWpCountry, af.getCountry() );
	  item->setText(colWpIcao, af.getICAO() );

	  item->setTextAlignment( colWpName, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpDescription, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpCountry, Qt::AlignCenter );
	  item->setTextAlignment( colWpIcao, Qt::AlignCenter );

	  QVariant v;
	  v.setValue(&af);
	  item->setData( 0, Qt::UserRole, v );
	  waypoints->insertTopLevelItem( i, item );
	}
    }
  else if( selectedItem == Hotspots )
    {
      // Gets current hotspot list from MapContents
      QList<SinglePoint> &list = _globalMapContents->getHotspotList();

      for( int i = 0; i < list.size(); i++ )
	{
	  SinglePoint& sp = list[i];

	  QTreeWidgetItem *item = new QTreeWidgetItem;

	  item->setText(colWpName, sp.getShortName() );
	  item->setIcon(colWpName, _globalMapConfig->getPixmap(sp.getTypeID(), false, true) );
	  item->setText(colWpDescription, sp.getName() );
	  item->setText(colWpCountry, sp.getCountry() );
	  item->setText(colWpIcao, "" );

	  item->setTextAlignment( colWpName, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpDescription, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpCountry, Qt::AlignCenter );
	  item->setTextAlignment( colWpIcao, Qt::AlignCenter );

	  QVariant v;
	  v.setValue(&sp);
	  item->setData( 0, Qt::UserRole, v );
	  waypoints->insertTopLevelItem( i, item );
	}
    }
  else if( selectedItem == Navaids )
    {
      // Gets current navaid list from MapContents
      QList<RadioPoint> &list = _globalMapContents->getNavaidList();

      for( int i = 0; i < list.size(); i++ )
	{
	  RadioPoint& rp = list[i];

	  QTreeWidgetItem *item = new QTreeWidgetItem;

	  item->setText(colWpName, rp.getShortName() );
	  item->setIcon(colWpName, _globalMapConfig->getPixmap(rp.getTypeID(), false, true) );
	  item->setText(colWpDescription, rp.getName() );
	  item->setText(colWpCountry, rp.getCountry() );
	  item->setText(colWpIcao, rp.getICAO() );

	  item->setTextAlignment( colWpName, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpDescription, Qt::AlignLeft|Qt::AlignVCenter );
	  item->setTextAlignment( colWpCountry, Qt::AlignCenter );
	  item->setTextAlignment( colWpIcao, Qt::AlignCenter );

	  QVariant v;
	  v.setValue(&rp);
	  item->setData( 0, Qt::UserRole, v );
	  waypoints->insertTopLevelItem( i, item );
	}
    }
  else
    {
      // Unknown category
      qWarning() << "TaskEditor::slotLoadSelectableWaypoints(): Unknown list item"
	         << selectedItem;
      return;
    }

  waypoints->sortItems(0, Qt::AscendingOrder);
  waypoints->slotResizeColumns2Content();
}

void TaskEditor::slotSetPlanningType( const QString& text )
{
  int id = FlightTask::ttText2Item( text );

  switch (id)
  {
  case FlightTask::FAIArea:
    {
      QString msg = tr("<b>Task Type FAI Area:</b><br><br>"
          "You can define a FAI task with either Takeoff, Start, End and Landing or "
          "Takeoff, Start, End, Landing and <b>one</b> additional Route point.<br>"
          "The points <i>Takeoff</i>, <i>Start</i>, <i>End</i> and <i>Landing</i> "
          "are <b>mandatory!</b><br><br>"
          "The FAI area calculation will be made with Start and End point or Start "
          "and Route point, depending weather the route point is defined or not.<br><br>"
          "New points are inserted always after the selected one." );

      if( wpList.size() > 5 )
        {
          msg += tr( "<br><br><b>Your FAI task contains too much route points!"
                     "<br>Please remove all not necessary route points.</b>" );
        }

      errorFai->showMessage( "<html>" + msg + "</html>" );

      left->setEnabled(true);
      right->setEnabled(true);
      left->setChecked(pTask->getPlanningDirection() & FlightTask::leftOfRoute);
      right->setChecked(pTask->getPlanningDirection() & FlightTask::rightOfRoute);
    }

    break;

  case FlightTask::Route:

    errorRoute->showMessage(  tr("<html><b>Task Type Traditional Route:</b><br><br>"
      "You can define a task with Takeoff, Start, End, Landing and Route points. "
      "The points <i>Takeoff</i>, <i>Start</i>, <i>End</i> and <i>Landing</i> "
      "are <b>mandatory!</b> "
      "Additional route points can be added.<br><br>"
      "New points are inserted always after the selected one.</html>") );

    left->setEnabled(false);
    right->setEnabled(false);
    left->setChecked(false);
    right->setChecked(false);
    break;
  }

  pTask->setPlanningType(id);
  loadRouteWaypoints();
  route->setCurrentItem( route->topLevelItem(0) );
  enableCommandButtons();
}

void TaskEditor::slotSetPlanningDirection(int)
{
  int dir = 0;

  if( left->isChecked() )
    {
      dir |= FlightTask::leftOfRoute;
    }

  if( right->isChecked() )
    {
      dir |= FlightTask::rightOfRoute;
    }

  pTask->setPlanningDirection(dir);
}

void TaskEditor::loadRouteWaypoints()
{
  Waypoint *wpPrev = 0;

  route->clear();

  for( int i = 0; i < wpList.size(); i++ )
    {
      QString txt;

      Waypoint *wp = wpList.at(i);

      switch( wp->tpType )
        {
          case FlightTask::TakeOff:
            txt = tr("Take Off");
            break;
          case FlightTask::Begin:
            txt = tr("Begin of Task");
            break;
          case FlightTask::RouteP:
            txt = tr("Route Point");
            break;
          case FlightTask::End:
            txt = tr("End of Task");
            break;
          case FlightTask::FreeP:
            txt = tr("Free Point");
            break;
          case FlightTask::Landing:
            txt = tr("Landing");
            break;
          default:
            txt = tr("Unkown");
            break;
        }

      QTreeWidgetItem *item = new QTreeWidgetItem;

      item->setText(colRouteType, txt);
      item->setText(colRouteWaypoint, wp->name);
      item->setIcon(colRouteWaypoint, _globalMapConfig->getPixmap(wp->type, false, true));

      if( i > 0 )
        {
          item->setText(colRouteDist, QString("%1 Km").arg(wp->distance, 0, 'f', 3));
          item->setText(colRouteCourse, QString("%1%2")
                       .arg(getTrueCourse(wp->origP, wpPrev->origP), 3, 'f', 0, QChar('0') )
                       .arg(QChar(Qt::Key_degree)) );
        }

      item->setTextAlignment( colRouteType, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( colRouteWaypoint, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( colRouteDist, Qt::AlignRight|Qt::AlignVCenter );
      item->setTextAlignment( colRouteCourse, Qt::AlignCenter );

      route->insertTopLevelItem( i, item );

      wpPrev = wp;
    }

  route->slotResizeColumns2Content();

  taskType->setText( pTask->getTaskTypeString() );
}

int TaskEditor::getCurrentPosition()
{
  QTreeWidgetItem* item = route->currentItem();

  if( item )
    {
      return route->indexOfTopLevelItem( item );
    }

  return -1;
}

void TaskEditor::setSelected( int position )
{
  if( position >= 0 && route->topLevelItemCount() > 0 )
    {
      QTreeWidgetItem* item = route->topLevelItem( position );

      if( item )
        {
          route->setCurrentItem( item );
        }
    }

  enableCommandButtons();
}

void TaskEditor::slotMoveUp()
{
  int curPos = getCurrentPosition();

  if( curPos < 1 || route->topLevelItemCount() < 2 )
    {
      return;
    }

  Waypoint *wp = wpList.takeAt( curPos );
  wpList.insert( curPos - 1, wp );

  pTask->setWaypointList( wpList );
  loadRouteWaypoints();
  setSelected( curPos - 1 );
}

void TaskEditor::slotMoveDown()
{
  int curPos = getCurrentPosition();

  if( curPos < 0 ||
      route->topLevelItemCount() < 2 ||
      curPos >= route->topLevelItemCount() - 1 ||
      curPos >= wpList.size() - 1 )
    {
      return;
    }

  Waypoint *wp = wpList.takeAt( curPos );
  wpList.insert( curPos + 1, wp );

  pTask->setWaypointList( wpList );
  loadRouteWaypoints();
  setSelected( curPos + 1 );
}

void TaskEditor::slotInvertWaypoints()
{
  if ( wpList.count() < 2 )
    {
      // not possible to invert order, if elements are less 2
      return;
    }

  // invert list order
  for( int i= wpList.count()-2; i >= 0; i-- )
    {
      Waypoint *wp = wpList.at(i);
      wpList.removeAt(i);
      wpList.append( wp );
    }

  pTask->setWaypointList( wpList );
  loadRouteWaypoints();

  // After an invert the first waypoint item is always selected.
  setSelected( 0 );
}

void TaskEditor::slotAddWaypoint()
{
  int pos = getCurrentPosition();

  if( pos < 0 )
    {
      pos = 0;
    }

  // Gets the selected item from the waypoint list.
  QTreeWidgetItem* item = waypoints->currentItem();

  if( item == 0 )
    {
      return;
    }

  Waypoint *newWp = 0;
  Waypoint *wp = 0;
  Airfield *af = 0;
  RadioPoint *rp = 0;
  SinglePoint *sp = 0;

  // Retrieve the point data from the selected  item. At first we have to find
  // the item type.
  if( item->data(0, Qt::UserRole).canConvert<WaypointPtr>() )
    {
      wp = item->data(0, Qt::UserRole).value<WaypointPtr>();

      // Make a deep copy of waypoint data.
      newWp = new Waypoint( wp );
    }
  else if( item->data(0, Qt::UserRole).canConvert<AirfieldPtr>() )
    {
      af = item->data(0, Qt::UserRole).value<AirfieldPtr>();

      qDebug() << "AF=" << af << af->getName();

      newWp = new Waypoint;
      newWp->icao = af->getICAO();
      newWp->frequency = af->getFrequency();
      newWp->rwyList = af->getRunwayList();
      sp = af;
    }
  else if( item->data(0, Qt::UserRole).canConvert<RadioPointPtr>() )
    {
      rp = item->data(0, Qt::UserRole).value<RadioPointPtr>();
      newWp = new Waypoint;
      newWp->icao = rp->getICAO();
      newWp->frequency = rp->getFrequency();
      newWp->comment = rp->getAdditionalText();
      sp = rp;
    }
  else if( item->data(0, Qt::UserRole).canConvert<SinglePointPtr>() )
    {
      sp = item->data(0, Qt::UserRole).value<SinglePointPtr>();
      newWp = new Waypoint;
    }
  else
    {
      qWarning() << "TaskEditor::slotAddWaypoint(): Item data not handled!";
      return;
    }

  if( sp != 0 && newWp != 0 )
    {
      newWp->name = sp->getShortName();
      newWp->description = sp->getName();
      newWp->country = sp->getCountry();
      newWp->type = sp->getTypeID();
      newWp->origP = sp->getWGSPosition();
      newWp->elevation = sp->getElevation();

      if( newWp->comment.isEmpty() )
	{
	  newWp->comment = sp->getComment();
	}
      else
	{
	  newWp->comment += "\n\n" + sp->getComment();
	}
    }

  // Set projected coordinates
  newWp->projP = _globalMapMatrix->wgsToMap(newWp->origP);

  if( pos >= wpList.size() || pos < 0 )
    {
      wpList.append( newWp );
      pos = wpList.size() - 1;
    }
  else
    {
      wpList.insert(pos + 1, newWp);
      pos++;
    }

  pTask->setWaypointList( wpList );
  loadRouteWaypoints();
  setSelected( pos );
}

void TaskEditor::slotRemoveWaypoint()
{
  int curPos = getCurrentPosition();

  if ( curPos < 0 || wpList.count() == 0 )
    {
      return;
    }

  delete route->takeTopLevelItem( curPos );
  delete wpList.takeAt( curPos );

  pTask->setWaypointList( wpList );
  loadRouteWaypoints();

  // Remember last position.
  if( curPos >= wpList.size() )
    {
      setSelected( wpList.size() - 1);
    }
  else
    {
      setSelected( curPos );
    }
}

void TaskEditor::setTask(FlightTask *task)
{
  if( task == static_cast<FlightTask *>(0) )
    {
      // As fall back an internal empty task object is setup
      _task = FlightTask( MapContents::instance()->genTaskName() );
      pTask = &_task;

      qWarning() << "TaskEditor::setTask(): Null object passed as task!";
    }
  else
    {
      pTask = task;
    }

  // get waypoint list of task
  wpList = pTask->getWPList();

  name->setText( pTask->getFileName() );

  // Save initial name of task. Is checked during accept for change.
  startName = name->text();

  planningTypes->setCurrentIndex( planningTypes->findText( FlightTask::ttItem2Text(pTask->getPlanningType())) );

  left->setChecked(pTask->getPlanningDirection() & FlightTask::leftOfRoute);
  right->setChecked(pTask->getPlanningDirection() & FlightTask::rightOfRoute);

  slotSetPlanningType( FlightTask::ttItem2Text(pTask->getPlanningType()) );
}

void TaskEditor::slotItemClicked( QTreeWidgetItem* item, int column )
{
  Q_UNUSED( item )
  Q_UNUSED( column )

  enableCommandButtons();
}

void TaskEditor::enableCommandButtons()
{
  if( wpList.count() == 0 )
    {
      addCmd->setEnabled( true );
      removeCmd->setEnabled( false );
      upCmd->setEnabled( false );
      downCmd->setEnabled( false );
      invertCmd->setEnabled( false );
    }
  else if( wpList.count() == 1 )
    {
      addCmd->setEnabled( true );
      removeCmd->setEnabled( true );
      upCmd->setEnabled( false );
      downCmd->setEnabled( false );
      invertCmd->setEnabled( false );
    }
  else
    {
      addCmd->setEnabled( true );
      removeCmd->setEnabled( true );
      invertCmd->setEnabled( true );

      if( route->topLevelItemCount() && route->currentItem() == 0 )
        {
          // If no item is selected we select the first one.
          route->setCurrentItem(route->topLevelItem(route->indexOfTopLevelItem(0)));
        }

      if( route->indexOfTopLevelItem(route->currentItem()) > 0 )
        {
          upCmd->setEnabled( true );
        }
      else
        {
          upCmd->setEnabled( false );
        }

      if( route->indexOfTopLevelItem(route->currentItem()) < route->topLevelItemCount() - 1 )
        {
          downCmd->setEnabled( true );
        }
      else
        {
          downCmd->setEnabled( false );
        }
    }
}

void TaskEditor::slotAccept()
{
  // Here we check the task constrains.
  if( startName != name->text() )
    {
      // User has changed the task name. We must check, if the new name
      // is already in use.
      if( _globalMapContents->taskNameInUse( name->text()) )
        {
          QMessageBox::warning( this,
                                 tr("Task name in use"),
                                 tr("<html>The chosen task name is already in use!<br><br>"
                                    "Please enter another one.</html>"),
                                 QMessageBox::Ok );
          return;
        }
    }

  pTask->setTaskName( name->text() );

  if( wpList.size() < 4 )
    {
      QMessageBox::warning( this,
                             tr("Task is incomplete"),
                             tr("<html>A task consist of at least four waypoints!<br><br>"
                                "Please add the missing points.</html>"),
                             QMessageBox::Ok );
      return;
    }

  if( wpList.size() > 5 && pTask->getPlanningType() == FlightTask::FAIArea )
    {
      QMessageBox::warning( this,
                             tr("FAI area task violation"),
                             tr("<html>A FAI area task can have one additional "
                                 "route point only!<br><br>"
                                 "Please remove all not needed other points.</html>"),
                             QMessageBox::Ok );
      return;
    }

  accept();
}