/***********************************************************************
**
**   recorderdialog.cpp
**
**   This file is part of KFLog4.
**
************************************************************************
**
**   Copyright (c):  2002 by Heiner Lamprecht
**                   2011 by Axel Pauli
**
**   This file is distributed under the terms of the General Public
**   License. See the file COPYING for more information.
**
**   $Id$
**
***********************************************************************/

#include <dlfcn.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QtGui>
#include <Qt3Support>

#include "gliders.h"
#include "mainwindow.h"
#include "mapcalc.h"
#include "mapcontents.h"
#include "recorderdialog.h"
#include "rowdelegate.h"
#include "wgspoint.h"

extern MainWindow  *_mainWindow;
extern MapConfig   *_globalMapConfig;
extern MapContents *_globalMapContents;
extern MapMatrix   *_globalMapMatrix;
extern QSettings    _settings;

RecorderDialog::RecorderDialog( QWidget *parent ) :
  QDialog(parent),
  isOpen(false),
  isConnected(false),
  activeRecorder(0)
{
  setObjectName( "RecorderDialog" );
  setWindowTitle( tr("KFLog Flight Recorder") );
  setAttribute( Qt::WA_DeleteOnClose );
  setModal( true );
  setSizeGripEnabled( true );

  waypoints = _globalMapContents->getWaypointList();

  qSort(waypoints.begin(), waypoints.end());

  QList<BaseFlightElement *> *tList = _globalMapContents->getFlightList();

  for( int i = 0; i < tList->size(); i++ )
    {
      BaseFlightElement* element = tList->at(i);

      if( element->getObjectType() == BaseMapElement::Task )
        {
          tasks.append( dynamic_cast<FlightTask *> (element) );
        }
    }

  configLayout = new QGridLayout(this);

  setupTree = new QTreeWidget(this);
  setupTree->setRootIsDecorated( false );
  setupTree->setItemsExpandable( false );
  setupTree->setSortingEnabled( true );
  setupTree->setSelectionMode( QAbstractItemView::SingleSelection );
  setupTree->setSelectionBehavior( QAbstractItemView::SelectRows );
  setupTree->setColumnCount( 1 );
  setupTree->setFocusPolicy( Qt::StrongFocus );
  setupTree->setHeaderLabel( tr( "Menu" ) );

  // Set additional space per row
  RowDelegate* rowDelegate = new RowDelegate( setupTree, 10 );
  setupTree->setItemDelegate( rowDelegate );

  QTreeWidgetItem* headerItem = setupTree->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );

  configLayout->addWidget( setupTree, 0, 0 );

  connect( setupTree, SIGNAL(itemClicked( QTreeWidgetItem*, int )),
           this, SLOT( slotPageClicked( QTreeWidgetItem*, int )) );

  QPushButton *closeButton = new QPushButton(tr("&Close"), this);
  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

  statusBar = new QLabel;
  statusBar->setMargin( 5 );

  QGridLayout* gridBox = new QGridLayout;
  gridBox->setSpacing( 0 );
  gridBox->addWidget( statusBar, 0, 0 );
  gridBox->setColumnStretch( 0, 10 );
  gridBox->addWidget( closeButton, 0, 1, Qt::AlignRight );

  configLayout->addLayout( gridBox, 1, 0, 1, 2 );

  __addRecorderPage();
  __addFlightPage();
  __addDeclarationPage();
  __addTaskPage();
  __addWaypointPage();
  __addConfigPage();

  setupTree->sortByColumn ( 0, Qt::AscendingOrder );
  setupTree->resizeColumnToContents( 0 );
  setupTree->setFixedWidth( 170 );

  activePage = recorderPage;
  recorderPage->setVisible( true );

  // activePage->setFixedWidth(685);
  // setFixedWidth(830);
  // setMinimumHeight(350);

  slotEnablePages();
  restoreGeometry( _settings.value("/RecorderDialog/Geometry").toByteArray() );
}

RecorderDialog::~RecorderDialog()
{
  _settings.setValue( "/RecorderDialog/Name", selectType->currentText() );
  _settings.setValue( "/RecorderDialog/Port", selectPort->currentItem() );
  _settings.setValue( "/RecorderDialog/Baud", selectSpeed->currentItem() );
  _settings.setValue( "/RecorderDialog/URL", selectURL->text() );
  _settings.setValue( "/RecorderDialog/Geometry", saveGeometry() );

  slotCloseRecorder();
}

QString RecorderDialog::getLoggerPath()
{
  QString _installRoot = _settings.value( "/Path/InstallRoot", ".." ).toString();

  return QString( _installRoot + "/logger" );
}

QString RecorderDialog::getLibraryPath()
{
  QString _installRoot = _settings.value( "/Path/InstallRoot", ".." ).toString();

  return QString( _installRoot + "/bin" );
}

void RecorderDialog::__addRecorderPage()
{
  int typeLoop = 0;

  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText( 0, tr("Recorder") );
  item->setData( 0, Qt::UserRole, "Recorder" );
  item->setIcon( 0, _mainWindow->getPixmap("kde_media-tape_48.png") );
  setupTree->addTopLevelItem( item );
  setupTree->setCurrentItem( item );

  recorderPage = new QWidget(this);
  recorderPage->setObjectName( "RecorderPage" );
  recorderPage->setVisible( false );

  configLayout->addWidget( recorderPage, 0, 1 );

  //----------------------------------------------------------------------------

  QGroupBox* sGroup = new QGroupBox( tr("Settings") );

  selectType = new QComboBox;

  connect( selectType, SIGNAL(activated(const QString &)), this,
           SLOT(slotRecorderTypeChanged(const QString &)) );

  selectPortLabel = new QLabel(tr("Port:"));

  selectPort = new QComboBox;
  selectPort->addItem("ttyS0");
  selectPort->addItem("ttyS1");
  selectPort->addItem("ttyS2");
  selectPort->addItem("ttyS3");
  // the following devices are used for usb adapters
  selectPort->addItem("ttyUSB0");   // classical device
  selectPort->addItem("tts/USB0");  // devfs
  selectPort->addItem("usb/tts/0"); // udev
  // we never know if the device name will change again; let the user have a chance
  selectPort->setEditable(true);

  selectSpeedLabel = new QLabel(tr("Transfer speed:"));
  selectSpeed = new QComboBox;

  selectURLLabel = new QLabel(tr("URL:"));
  selectURL = new QLineEdit;

  cmdConnect = new QPushButton(tr("Connect to recorder"));
  cmdConnect->setMaximumWidth(cmdConnect->sizeHint().width() + 5);

  QGridLayout* sGridLayout = new QGridLayout;
  sGridLayout->setSpacing(10);

  sGridLayout->addWidget( new QLabel( tr("Type:")), 0, 0 );
  sGridLayout->addWidget( selectType, 0, 1 );
  sGridLayout->addWidget( selectPortLabel, 1, 0 );
  sGridLayout->addWidget( selectPort, 1, 1 );
  sGridLayout->addWidget( selectSpeedLabel, 2, 0 );
  sGridLayout->addWidget( selectSpeed, 2, 1 );
  sGridLayout->addWidget( selectURLLabel, 2, 0 );
  sGridLayout->addWidget( selectURL, 2, 1 );
  sGridLayout->setRowMinimumHeight( 3, 5 );
  sGridLayout->addWidget( cmdConnect, 4, 0, 1, 4, Qt::AlignLeft );

  sGroup->setLayout( sGridLayout );

  //----------------------------------------------------------------------------
  QGroupBox* iGroup = new QGroupBox( tr("Info") );

  lblApiID = new QLabel(tr("API-Version:"));
  apiID = new QLabel;
  apiID->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  apiID->setBackgroundRole( QPalette::Light );
  apiID->setAutoFillBackground( true );
  apiID->setEnabled(false);

  lblSerID = new QLabel(tr("Serial-No.:"));
  serID = new QLabel(tr("No recorder connected"));
  serID->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  serID->setBackgroundRole( QPalette::Light );
  serID->setAutoFillBackground( true );
  serID->setEnabled(false);

  lblRecType = new QLabel(tr("Recorder Type:"));
  recType = new QLabel;
  recType->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  recType->setBackgroundRole( QPalette::Light );
  recType->setAutoFillBackground( true );
  recType->setEnabled(false);

  lblPltName = new QLabel(tr("Pilot Name:"));
  pltName = new QLineEdit;
  pltName->setEnabled(false);

  lblGldType = new QLabel(tr("Glider Type:"));
  gldType = new QLineEdit;
  gldType->setEnabled(false);

  lblGldID = new QLabel(tr("Glider Sign:"));
  gldID = new QLineEdit;
  gldID->setEnabled(false);

  lblCompID = new QLabel(tr("Competition Id:"));
  compID = new QLineEdit;
  compID->setEnabled(false);

  cmdUploadBasicConfig = new QPushButton(tr("Write data to recorder"));
  cmdUploadBasicConfig->setMaximumWidth(cmdUploadBasicConfig->sizeHint().width() + 5);

  // disable this button until we read the information from the flight recorder:
  cmdUploadBasicConfig->setEnabled(false);
  connect( cmdUploadBasicConfig, SIGNAL(clicked()), SLOT(slotWriteConfig()) );

  QGridLayout* iGridLayout = new QGridLayout;
  iGridLayout->setSpacing(10);
  iGridLayout->addWidget( lblApiID, 0, 0 );
  iGridLayout->addWidget( apiID, 0, 1 );
  iGridLayout->addWidget( lblPltName, 0, 2 );
  iGridLayout->addWidget( pltName, 0, 3 );
  iGridLayout->addWidget( lblSerID, 1, 0 );
  iGridLayout->addWidget( serID, 1, 1 );
  iGridLayout->addWidget( lblGldType, 1, 2 );
  iGridLayout->addWidget( gldType, 1, 3 );
  iGridLayout->addWidget( lblRecType, 2, 0 );
  iGridLayout->addWidget( recType, 2, 1 );
  iGridLayout->addWidget( lblGldID, 2, 2 );
  iGridLayout->addWidget( gldID, 2, 3 );
  iGridLayout->addWidget( lblCompID, 3, 2 );
  iGridLayout->addWidget( compID, 3, 3 );
  iGridLayout->setRowMinimumHeight( 4, 5 );
  iGridLayout->addWidget( cmdUploadBasicConfig, 5, 0, 1, 4, Qt::AlignLeft );
  iGridLayout->setColStretch( 1, 5 );
  iGridLayout->setColStretch( 3, 5 );

  iGroup->setLayout( iGridLayout );

  QVBoxLayout* recorderLayout = new QVBoxLayout;
  recorderLayout->setContentsMargins( 0, 0, 0, 0 );
  recorderLayout->addWidget( sGroup );
  recorderLayout->addSpacing( 20 );
  recorderLayout->addWidget( iGroup );
  recorderLayout->addStretch( 10 );

  recorderPage->setLayout( recorderLayout );

  __setRecorderConnectionType( FlightRecorderPluginBase::none );

  QDir path = QDir( getLoggerPath() );
  QStringList configRec = path.entryList( "*.desktop" );

  if( configRec.count() == 0 )
    {
      QMessageBox::critical( this,
                             tr("No recorders installed!"),
                             tr("There are no recorder-libraries installed."),
                             QMessageBox::Ok );
    }

  libNameList.clear();

  selectPort->setCurrentIndex( _settings.value("/RecorderDialog/Port", 0).toInt() );
  selectSpeed->setCurrentIndex( _settings.value("/RecorderDialog/Baud", 0).toInt() );

  QString name( _settings.value("/RecorderDialog/Name", "").toString() );

  selectURL->setText(_settings.value("/RecorderDialog/URL", "").toString() );

  for( int i = 0; i < configRec.size(); i++ )
    {
      QString pluginName = "";
      QString currentLibName = "";

      QFile settingFile( getLoggerPath() + "/" + configRec[i] );

      if( !settingFile.exists() )
        {
          continue;
        }

      if( !settingFile.open( QIODevice::ReadOnly ) )
        {
          continue;
        }

      QTextStream stream( &settingFile );

      while( ! stream.atEnd() )
        {
          QString lineStream = stream.readLine();

          if( lineStream.mid( 0, 5 ) == "Name=" )
            {
              pluginName = lineStream.remove( 0, 5 );
            }
          else if( lineStream.mid( 0, 8 ) == "LibName=" )
            {
              currentLibName = lineStream.remove( 0, 8 );
            }
        }

      settingFile.close();

      if( pluginName != "" && currentLibName != "" )
        {
          selectType->insertItem( pluginName );
          libNameList.insert( pluginName, currentLibName );
          typeLoop++;

          if( name == "" )
            {
              name = pluginName;
            }
        }
    }

  // sort if this style uses a listbox for the combobox
  if( selectType->model() )
    {
      selectType->model()->sort( 0 );
    }

  selectType->setCurrentIndex( selectType->findText(name) );

  slotRecorderTypeChanged( selectType->currentText() );

  connect( cmdConnect, SIGNAL(clicked()), SLOT(slotConnectRecorder()) );
}

void RecorderDialog::__setRecorderConnectionType(FlightRecorderPluginBase::TransferMode mode)
{
  selectPort->hide();
  selectPortLabel->hide();
  selectSpeed->hide();
  selectSpeedLabel->hide();
  selectURL->hide();
  selectURLLabel->hide();
  cmdConnect->setEnabled(false);

  switch(mode)
    {
      case FlightRecorderPluginBase::serial:
        selectPort->show();
        selectPortLabel->show();
        selectSpeed->show();
        selectSpeedLabel->show();
        cmdConnect->setEnabled(true);
        break;
      case FlightRecorderPluginBase::URL:
        selectURL->show();
        selectURLLabel->show();
        cmdConnect->setEnabled(true);
        break;
      default:
        break; //nothing to be done.
    }
}

void RecorderDialog::__setRecorderCapabilities()
{
  FlightRecorderPluginBase::FR_Capabilities cap = activeRecorder->capabilities();

  serID->setVisible(cap.supDspSerialNumber);
  lblSerID->setVisible(cap.supDspSerialNumber);

  recType->setVisible(cap.supDspRecorderType);
  lblRecType->setVisible(cap.supDspRecorderType);

  pltName->setVisible(cap.supDspPilotName);
  lblPltName->setVisible(cap.supDspPilotName);

  gldType->setVisible(cap.supDspGliderType);
  lblGldType->setVisible(cap.supDspGliderType);

  gldID->setVisible(cap.supDspGliderID);
  lblGldID->setVisible(cap.supDspGliderID);

  compID->setVisible(cap.supDspGliderID);
  lblCompID->setVisible(cap.supDspGliderID);

  bool edit = cap.supEditGliderID | cap.supEditGliderType | cap.supEditPilotName;

  pltName->setEnabled( edit );
  gldType->setEnabled( edit );
  gldID->setEnabled( edit );
  compID->setEnabled( edit );
  cmdUploadBasicConfig->setEnabled( edit );
  cmdUploadBasicConfig->setVisible( edit );

  selectSpeed->clear();
  selectSpeed->setEnabled( true );

  if( cap.supAutoSpeed )
    {
      selectSpeed->addItem( tr("Auto") );
      selectSpeed->setCurrentIndex( selectSpeed->count() - 1 );
    }

    {
      // insert highest speed first
      for (int i = FlightRecorderPluginBase::transferDataMax-1; i >= 0; i--)
        {
          if ((FlightRecorderPluginBase::transferData[i]._bps & cap.transferSpeeds) ||
              (cap.transferSpeeds == FlightRecorderPluginBase::bps00000))
            {
              selectSpeed->addItem(QString("%1").arg(FlightRecorderPluginBase::transferData[i]._speed));
            }
        }
    }
}

void RecorderDialog::__addFlightPage()
{
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText( 0, tr("Flights") );
  item->setData( 0, Qt::UserRole, "Flights" );
  item->setIcon( 0, _mainWindow->getPixmap("igc_48.png") );
  setupTree->addTopLevelItem( item );

  flightPage = new QWidget(this);
  flightPage->setObjectName( "FlightPage" );
  flightPage->setVisible( false );

  configLayout->addWidget( flightPage, 0, 1 );

  //----------------------------------------------------------------------------

  flightList = new KFLogTreeWidget( "RecorderDialog-FlightList", this );

  flightList->setSortingEnabled( true );
  flightList->setAllColumnsShowFocus( true );
  flightList->setFocusPolicy( Qt::StrongFocus );
  flightList->setRootIsDecorated( false );
  flightList->setItemsExpandable( true );
  flightList->setSelectionMode( QAbstractItemView::NoSelection );
  flightList->setAlternatingRowColors( true );
  flightList->addRowSpacing( 5 );
  flightList->setColumnCount( 8 );

  QStringList headerLabels;

  headerLabels  << tr("No.")
                << tr("Date")
                << tr("Pilot")
                << tr("Glider")
                << tr("First Point")
                << tr("Last Point")
                << tr("Duration")
                << "";

  flightList->setHeaderLabels( headerLabels );

  QTreeWidgetItem* headerItem = flightList->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );
  headerItem->setTextAlignment( 1, Qt::AlignCenter );
  headerItem->setTextAlignment( 2, Qt::AlignCenter );
  headerItem->setTextAlignment( 3, Qt::AlignCenter );
  headerItem->setTextAlignment( 4, Qt::AlignCenter );
  headerItem->setTextAlignment( 5, Qt::AlignCenter );
  headerItem->setTextAlignment( 6, Qt::AlignCenter );

  colNo         = 0;
  colDate       = 1;
  colPilot      = 2;
  colGlider     = 3;
  colFirstPoint = 4;
  colLastPoint  = 5;
  colDuration   = 6;
  colDummy      = 7;

  flightList->loadConfig();

  QPushButton* loadB = new QPushButton( tr( "Load list" ) );
  connect( loadB, SIGNAL(clicked()), SLOT(slotReadFlightList()) );

  QPushButton* saveB = new QPushButton( tr( "Save flight" ) );
  connect( saveB, SIGNAL(clicked()), SLOT(slotDownloadFlight()) );

  useLongNames = new QCheckBox( tr( "Long filenames" ) );

  // let's prefer short filenames. These are needed for OLC
  useLongNames->setChecked( false );
  useLongNames->setToolTip( tr("If checked, long filenames are used.") );

  useFastDownload = new QCheckBox( tr( "Fast download" ) );
  useFastDownload->setChecked( true );

  useFastDownload->setToolTip(
                  tr("<html>If checked, the IGC-file will not be signed.<BR>"
                     "<b>Note!</b> Do not use fast download<BR>"
                     " when using the file for competitions.</html>"));

  QVBoxLayout *flightPageLayout = new QVBoxLayout;
  flightPageLayout->setSpacing(10);
  flightPageLayout->setContentsMargins( 0, 0, 0, 0 );
  flightPageLayout->addWidget( flightList );

  QHBoxLayout *buttonBox = new QHBoxLayout;
  buttonBox->setSpacing( 10 );
  buttonBox->addWidget(loadB);
  buttonBox->addStretch( 10 );
  buttonBox->addWidget( saveB );
  buttonBox->addStretch( 10 );
  buttonBox->addWidget(useLongNames);
  buttonBox->addStretch( 10 );
  buttonBox->addWidget(useFastDownload);

  flightPageLayout->addLayout( buttonBox );
  flightPage->setLayout( flightPageLayout );
}

void RecorderDialog::__addDeclarationPage()
{
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText( 0, tr("Declaration") );
  item->setData( 0, Qt::UserRole, "Declaration" );
  item->setIcon( 0, _mainWindow->getPixmap("declaration_48.png") );
  setupTree->addTopLevelItem( item );

  declarationPage = new QWidget(this);
  declarationPage->setObjectName( "DeclarationPage" );
  declarationPage->setVisible( false );

  configLayout->addWidget( declarationPage, 0, 1 );

  //----------------------------------------------------------------------------

  declarationList = new KFLogTreeWidget( "RecorderDialog-DeclarationList", this );

  declarationList->setSortingEnabled( true );
  declarationList->setAllColumnsShowFocus( true );
  declarationList->setFocusPolicy( Qt::StrongFocus );
  declarationList->setRootIsDecorated( false );
  declarationList->setItemsExpandable( true );
  declarationList->setSelectionMode( QAbstractItemView::NoSelection );
  declarationList->setAlternatingRowColors( true );
  declarationList->addRowSpacing( 5 );
  declarationList->setColumnCount( 5 );

  QStringList headerLabels;

  headerLabels  << tr("No.")
                << tr("Name")
                << tr("Latitude")
                << tr("Longitude")
                << "";

  declarationList->setHeaderLabels( headerLabels );

  QTreeWidgetItem* headerItem = declarationList->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );
  headerItem->setTextAlignment( 1, Qt::AlignCenter );
  headerItem->setTextAlignment( 2, Qt::AlignCenter );
  headerItem->setTextAlignment( 3, Qt::AlignCenter );
  headerItem->setTextAlignment( 4, Qt::AlignCenter );

  declarationColNo    = 0;
  declarationColName  = 1;
  declarationColLat   = 2;
  declarationColLon   = 3;
  declarationColDummy = 4;

  declarationList->loadConfig();

  taskSelection = new QComboBox;

  QHBoxLayout* taskBox = new QHBoxLayout;
  taskBox->addWidget( new QLabel(tr("Task:")) );
  taskBox->addWidget( taskSelection );
  taskBox->addStretch( 10 );

  pilotName = new QLineEdit;
  copilotName = new QLineEdit;
  gliderID = new QLineEdit;
  gliderType = new QComboBox;
  gliderType->setEditable(true);
  editCompID = new QLineEdit;
  compClass = new QLineEdit;

  QGridLayout* gliderGLayout = new QGridLayout;
  gliderGLayout->setMargin( 0 );

  QFormLayout *formLayout = new QFormLayout;
  formLayout->addRow( tr("Pilot:"), pilotName );
  formLayout->addRow( tr("Glider Id:"), gliderID );
  formLayout->addRow( tr("Comp-Id:"), editCompID );
  gliderGLayout->addLayout( formLayout, 0, 0 );

  formLayout = new QFormLayout;
  formLayout->addRow( tr("Copilot:"), copilotName );
  formLayout->addRow( tr("Glider Type:"), gliderType );
  formLayout->addRow( tr("Comp-Class:"), compClass );
  gliderGLayout->addLayout( formLayout, 0, 1 );

  QPushButton* writeButton = new QPushButton( tr("Write declaration to recorder") );
  writeButton->setMaximumWidth(writeButton->sizeHint().width() + 15);

  QHBoxLayout *buttonBox = new QHBoxLayout;
  buttonBox->setSpacing( 0 );
  buttonBox->addWidget(writeButton);
  buttonBox->addStretch( 10 );

  QVBoxLayout *decPageLayout = new QVBoxLayout;
  decPageLayout->setContentsMargins( 0, 0, 0, 0 );
  decPageLayout->setSpacing(10);
  decPageLayout->addLayout( taskBox );
  decPageLayout->addWidget( declarationList );
  decPageLayout->addLayout( gliderGLayout );
  decPageLayout->addLayout( buttonBox );

  declarationPage->setLayout( decPageLayout );

  int idx = 0;

  while( gliderList[idx].index != -1 )
    {
      gliderType->addItem( QString( gliderList[idx++].name ) );
    }

  pilotName->setText( _settings.value("/PersonalData/PilotName", "").toString() );

  for( int i = 0; i < tasks.size(); i++ )
    {
      FlightTask *task = tasks.at(i);
      taskSelection->addItem(task->getFileName() + " " + task->getTaskTypeString());
    }

  if( tasks.count() )
    {
      slotSwitchTask( 0 );
    }
  else
    {
      qWarning( "No tasks planned ..." );

      // Isn't it possible to write an declaration without a task?
      writeButton->setEnabled( false );
    }

  connect(taskSelection, SIGNAL(activated(int)), SLOT(slotSwitchTask(int)));
  connect(writeButton, SIGNAL(clicked()), SLOT(slotWriteDeclaration()));
}

void RecorderDialog::__addTaskPage()
{
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText( 0, tr("Tasks") );
  item->setData( 0, Qt::UserRole, "Tasks" );
  item->setIcon( 0, _mainWindow->getPixmap("task_48.png") );
  setupTree->addTopLevelItem( item );

  taskPage = new QWidget;
  taskPage->setObjectName( "TaskPage" );
  taskPage->setVisible( false );

  configLayout->addWidget( taskPage, 0, 1 );

  //----------------------------------------------------------------------------

  taskList = new KFLogTreeWidget( "RecorderDialog-TaskList", this );

  taskList->setSortingEnabled( true );
  taskList->setAllColumnsShowFocus( true );
  taskList->setFocusPolicy( Qt::StrongFocus );
  taskList->setRootIsDecorated( false );
  taskList->setItemsExpandable( true );
  taskList->setSelectionMode( QAbstractItemView::NoSelection );
  taskList->setAlternatingRowColors( true );
  taskList->addRowSpacing( 5 );
  taskList->setColumnCount( 6 );

  QStringList headerLabels;

  headerLabels  << tr("No.")
                << tr("Name")
                << tr("Description")
                << tr("Distance")
                << tr("Total distance")
                << "";

  taskList->setHeaderLabels( headerLabels );

  QTreeWidgetItem* headerItem = taskList->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );
  headerItem->setTextAlignment( 1, Qt::AlignCenter );
  headerItem->setTextAlignment( 2, Qt::AlignCenter );
  headerItem->setTextAlignment( 3, Qt::AlignCenter );
  headerItem->setTextAlignment( 4, Qt::AlignCenter );

  taskColNo    = 0;
  taskColName  = 1;
  taskColDesc  = 2;
  taskColTask  = 3;
  taskColTotal = 4;
  taskColDummy = 5;

  taskList->loadConfig();

  cmdUploadTasks = new QPushButton(tr("Write tasks to recorder"));
  connect(cmdUploadTasks, SIGNAL(clicked()), SLOT(slotWriteTasks()));

  cmdDownloadTasks = new QPushButton(tr("Read tasks from recorder"));
  connect(cmdDownloadTasks, SIGNAL(clicked()), SLOT(slotReadTasks()));

  lblTaskList = new QLabel;

  QVBoxLayout *taskPageLayout = new QVBoxLayout;
  taskPageLayout->setContentsMargins( 0, 0, 0, 0 );
  taskPageLayout->setSpacing(10);
  taskPageLayout->addWidget( taskList );

  QHBoxLayout *buttonBox = new QHBoxLayout;
  buttonBox->setSpacing( 10 );
  buttonBox->addWidget(cmdUploadTasks);
  buttonBox->addStretch( 10 );
  buttonBox->addWidget( lblTaskList );
  buttonBox->addStretch( 10 );
  buttonBox->addWidget(cmdDownloadTasks);

  taskPageLayout->addLayout( buttonBox );
  taskPage->setLayout( taskPageLayout );

  fillTaskList( tasks );
}

void RecorderDialog::fillTaskList( QList<FlightTask *>& ftList )
{
  taskList->clear();

  for( int i = 0; i < ftList.size(); i++ )
    {
      FlightTask* task = ftList.at(i);

      QTreeWidgetItem *item = new QTreeWidgetItem;

      item->setText(taskColNo, QString("%1").arg( i + 1));
      item->setText(taskColName, task->getFileName());
      item->setText(taskColDesc, task->getTaskTypeString());
      item->setText(taskColTask, task->getTaskDistanceString() );
      item->setText(taskColTotal, task->getTotalDistanceString() );
      item->setText(taskColDummy, "");

      item->setTextAlignment( taskColNo, Qt::AlignRight|Qt::AlignVCenter );
      item->setTextAlignment( taskColName, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( taskColDesc, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( taskColTask, Qt::AlignCenter );
      item->setTextAlignment( taskColTotal, Qt::AlignRight|Qt::AlignVCenter );

      taskList->insertTopLevelItem( i, item );
    }

  taskList->slotResizeColumns2Content();
}

void RecorderDialog::__addWaypointPage()
{
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText( 0, tr("Waypoints") );
  item->setData( 0, Qt::UserRole, "Waypoints" );
  item->setIcon( 0, _mainWindow->getPixmap("waypoint_48.png") );
  setupTree->addTopLevelItem( item );

  waypointPage = new QWidget(this);
  waypointPage->setObjectName( "WaypointsPage" );
  waypointPage->setVisible( false );

  configLayout->addWidget( waypointPage, 0, 1 );

  //----------------------------------------------------------------------------

  waypointList = new KFLogTreeWidget( "RecorderDialog-WaypointList", this );

  waypointList->setSortingEnabled( true );
  waypointList->setAllColumnsShowFocus( true );
  waypointList->setFocusPolicy( Qt::StrongFocus );
  waypointList->setRootIsDecorated( false );
  waypointList->setItemsExpandable( true );
  waypointList->setSelectionMode( QAbstractItemView::NoSelection );
  waypointList->setAlternatingRowColors( true );
  waypointList->addRowSpacing( 5 );
  waypointList->setColumnCount( 5 );

  QStringList headerLabels;

  headerLabels  << tr("No.")
                << tr("Name")
                << tr("Latitude")
                << tr("Longitude")
                << "";

  waypointList->setHeaderLabels( headerLabels );

  QTreeWidgetItem* headerItem = waypointList->headerItem();
  headerItem->setTextAlignment( 0, Qt::AlignCenter );
  headerItem->setTextAlignment( 1, Qt::AlignCenter );
  headerItem->setTextAlignment( 2, Qt::AlignCenter );
  headerItem->setTextAlignment( 3, Qt::AlignCenter );

  waypointColNo    = 0;
  waypointColName  = 1;
  waypointColLat   = 2;
  waypointColLon   = 3;
  waypointColDummy = 4;

  waypointList->loadConfig();

  cmdUploadWaypoints = new QPushButton(tr("Write waypoints to recorder"));
  connect(cmdUploadWaypoints, SIGNAL(clicked()), SLOT(slotWriteWaypoints()));

  cmdDownloadWaypoints = new QPushButton(tr("Read waypoints from recorder"));
  connect(cmdDownloadWaypoints, SIGNAL(clicked()), SLOT(slotReadWaypoints()));

  lblWpList = new QLabel;

  QVBoxLayout *wpPageLayout = new QVBoxLayout;
  wpPageLayout->setContentsMargins( 0, 0, 0, 0 );
  wpPageLayout->setSpacing(10);
  wpPageLayout->addWidget( waypointList );

  QHBoxLayout *buttonBox = new QHBoxLayout;
  buttonBox->setSpacing( 0 );
  buttonBox->addWidget(cmdUploadWaypoints);
  buttonBox->addStretch( 10 );
  buttonBox->addWidget( lblWpList );
  buttonBox->addStretch( 10 );
  buttonBox->addWidget(cmdDownloadWaypoints);

  wpPageLayout->addLayout( buttonBox );
  waypointPage->setLayout( wpPageLayout );

  fillWaypointList( waypoints );
}

void RecorderDialog::fillWaypointList( QList<Waypoint *>& wpList )
{
  waypointList->clear();

  for( int i = 0; i < wpList.size(); i++ )
    {
      Waypoint *wp = wpList.at(i);

      QTreeWidgetItem *item = new QTreeWidgetItem;

      item->setIcon(waypointColName, _globalMapConfig->getPixmap(wp->type, false, true) );

      item->setText(waypointColNo, QString("%1").arg( i + 1));
      item->setText(waypointColName, wp->name);
      item->setText(waypointColLat, WGSPoint::printPos(wp->origP.lat(), true));
      item->setText(waypointColLon, WGSPoint::printPos(wp->origP.lon(), false));
      item->setText(waypointColDummy, "");

      item->setTextAlignment( waypointColNo, Qt::AlignRight|Qt::AlignVCenter );
      item->setTextAlignment( waypointColName, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( waypointColLat, Qt::AlignCenter );
      item->setTextAlignment( waypointColLon, Qt::AlignCenter );

      waypointList->insertTopLevelItem( i, item );
    }

  waypointList->slotResizeColumns2Content();
  // waypointList->sortByColumn(waypointColName, Qt::AscendingOrder);
}

void RecorderDialog::slotConnectRecorder()
{
  if( !activeRecorder )
    {
      return;
    }

  portName = "/dev/" + selectPort->currentText();

  QString name= libNameList[selectType->currentText()];

  int speed = 0;

  speed = selectSpeed->currentText().toInt();

  if( ! __openLib( name ) )
    {
      return;
    }

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Connecting to recorder") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  // check if we have valid parameters, is that true, try to connect!
  switch (activeRecorder->getTransferMode() )
  {

    case FlightRecorderPluginBase::serial:

      if( portName.isEmpty() )
          {
            qWarning() << "slotConnectRecorder(): Missing port!";
            isConnected = false;
            break;
          }

      isConnected = (activeRecorder->openRecorder( portName.toLatin1().data(), speed ) >= FR_OK);
      break;

  case FlightRecorderPluginBase::URL:
    {
      selectURL->setText( selectURL->text().trimmed() );

      QString URL = selectURL->text();

      if( URL.isEmpty() )
          {
            qWarning() <<  "slotConnectRecorder(): Missing URL!";
            isConnected = false;
            break;
          };

      isConnected=(activeRecorder->openRecorder(URL)>=FR_OK);
      break;
    }

  default:

    isConnected=false;
    QApplication::restoreOverrideCursor();
    statusBar->setText( "" );
    return;
  }

  if( isConnected )
    {
      connect (activeRecorder, SIGNAL(newSpeed(int)),this,SLOT(slotNewSpeed(int)));
      slotEnablePages();
      slotReadDatabase();
      QApplication::restoreOverrideCursor();
    }
  else
    {
      QApplication::restoreOverrideCursor();

      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Sorry, could not connect to recorder.\n"
                              "Please check connections and settings.\n");

      if( ! errorDetails.isEmpty() )
        {
          errorText += errorDetails;
        }

      QMessageBox::warning( this,
                            tr("Recorder Connection"),
                            errorText,
                            QMessageBox::Ok );
    }

  statusBar->setText( "" );
}

void RecorderDialog::slotCloseRecorder()
{
  if( activeRecorder )
    {
      QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

      statusBar->setText( tr("Closing connection to recorder") );
      QCoreApplication::processEvents();
      QCoreApplication::flush();

      qDebug( "A recorder is active. Checking connection." );

      if( activeRecorder->isConnected() )
        {
          qDebug( "Recorder is connected. Closing..." );

          if( activeRecorder )
            {
              activeRecorder->closeRecorder();
            }
        }

      qDebug( "Going to close recorder object..." );
      activeRecorder = 0;
      qDebug( "Done." );

      QApplication::restoreOverrideCursor();
      statusBar->setText( "" );
    }
}

void RecorderDialog::slotPageClicked( QTreeWidgetItem * item, int column )
{
  Q_UNUSED( column );

  QString itemText = item->data( 0, Qt::UserRole ).toString();

  qDebug() << "RecorderDialog::slotPageClicked(): Page=" << itemText;

  activePage->setVisible( false );

  if( itemText == "Configuration" )
    {
      configPage->setVisible( true );
      activePage = configPage;
    }
  else if( itemText == "Declaration" )
    {
      declarationPage->setVisible( true );
      activePage = declarationPage;
    }
  else if( itemText == "Flights" )
    {
      flightPage->setVisible( true );
      activePage = flightPage;
    }
  else if( itemText == "Recorder" )
    {
      recorderPage->setVisible( true );
      activePage = recorderPage;
    }
  else if( itemText == "Tasks" )
    {
      taskPage->setVisible( true );
      activePage = taskPage;
    }
  else if( itemText == "Waypoints" )
    {
      waypointPage->setVisible( true );
      activePage = waypointPage;
    }
  else
    {
      activePage->setVisible( true );

      qWarning() << "RecorderDialog::slotPageClicked: Unknown item"
                 << itemText
                 << "received!";
    }
}

void RecorderDialog::slotReadFlightList()
{
  if( !activeRecorder )
    {
      return;
    }

  if( !activeRecorder->capabilities().supDlFlight )
    {
      QMessageBox::warning( this,
                            tr("Flight download"),
                            tr("Function not implemented"),
                            QMessageBox::Ok );
      return;
    }

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Reading flights from recorder") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  // FIXME: Who do clear and free the dirList?
  int ret = activeRecorder->getFlightDir( &dirList );

  flightList->clear();

  int error = 0;

  if( ret < FR_OK )
    {
      QApplication::restoreOverrideCursor();

      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Cannot read flights from recorder." );

      if( ! errorDetails.isEmpty() )
        {
          errorText += "\n" + errorDetails;
        }

      QMessageBox::critical( this,
                             tr( "Library Error" ),
                             errorText,
                             QMessageBox::Ok );
      error++;
    }

  if( dirList.count() == 0 )
    {
      QApplication::restoreOverrideCursor();

      QMessageBox::information( this,
                                tr( "Download result" ),
                                tr( "No flights are stored in the recorder." ),
                                QMessageBox::Ok );
      error++;
    }

  if( error )
    {
      QApplication::restoreOverrideCursor();
      statusBar->setText("");
      return;
    }

  for( int i = 0; i < dirList.size(); i++ )
    {
      QString day;
      FRDirEntry* dirListItem = dirList.at(i);

      QTreeWidgetItem *item = new QTreeWidgetItem;

      item->setText(colNo, QString("%1").arg( i + 1));

      day.sprintf( "%d-%.2d-%.2d",
                   dirListItem->firstTime.tm_year + 1900,
                   dirListItem->firstTime.tm_mon + 1,
                   dirListItem->firstTime.tm_mday );
      item->setText(colDate, day);

      item->setText(colPilot, dirListItem->pilotName);
      item->setText(colGlider, dirListItem->gliderID);

      QTime time( dirListItem->firstTime.tm_hour,
                  dirListItem->firstTime.tm_min,
                  dirListItem->firstTime.tm_sec );

      item->setText(colFirstPoint, time.toString("hh:mm"));

      time = QTime( dirListItem->lastTime.tm_hour,
                    dirListItem->lastTime.tm_min,
                    dirListItem->lastTime.tm_sec );

      item->setText(colLastPoint, time.toString("hh:mm"));

      time = QTime().addSecs (dirListItem->duration);
      item->setText(colDuration, time.toString("hh:mm"));

      item->setTextAlignment( colNo, Qt::AlignRight|Qt::AlignVCenter );
      item->setTextAlignment( colDate, Qt::AlignCenter );
      item->setTextAlignment( colPilot, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( colGlider, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( colFirstPoint, Qt::AlignCenter );
      item->setTextAlignment( colLastPoint, Qt::AlignCenter );
      item->setTextAlignment( colDuration, Qt::AlignCenter );

      flightList->insertTopLevelItem( i, item );
    }

  flightList->slotResizeColumns2Content();

  QApplication::restoreOverrideCursor();
  statusBar->setText("");
}

void RecorderDialog::slotDownloadFlight()
{
  QTreeWidgetItem *item = flightList->currentItem();

  if( item == 0 )
    {
      return;
    }

  QString errorDetails;

  // If no DefaultFlightDirectory is configured, we must use $HOME instead of the root-directory
  QString flightDir = _settings.value( "/Path/DefaultFlightDirectory",
                                       _mainWindow->getApplicationDataDirectory() ).toString();

  QString fileName = flightDir + "/";

  int flightID(item->text(colNo).toInt() - 1);

  //warning("Loading flight %d (%d)", flightID, flightList->itemPos(item));
  qWarning("%s", (const char*)dirList.at(flightID)->longFileName);
  qWarning("%s", (const char*)dirList.at(flightID)->shortFileName);

//  QTimer::singleShot( 0, this, SLOT(slotDisablePages()) );

  qWarning("fileName: %s", fileName.toLatin1().data());

  if(useLongNames->isChecked()) {
    fileName += dirList.at(flightID)->longFileName.upper();
  }
  else {
    fileName += dirList.at(flightID)->shortFileName.upper();
  }
  qWarning("flightdir: %s, filename: %s", flightDir.toLatin1().data(), fileName.toLatin1().data());

  QString filter;
  filter.append(tr("IGC") + " (*.igc)");

  fileName = QFileDialog::getSaveFileName( this,
                                           tr( "Select IGC file to save to" ),
                                           fileName,
                                           filter );

  if ( fileName.isEmpty() )
    {
      return;
    }

  QMessageBox* statusDlg = new QMessageBox ( tr("downloading flight"), tr("downloading flight"),
      QMessageBox::Information, Qt::NoButton, Qt::NoButton,
      Qt::NoButton, this, "statusDialog", true);

  statusDlg->show();

  qApp->processEvents();

  qWarning("%s", (const char*)fileName);

  if( !activeRecorder )
    {
      return;
    }

  qApp->processEvents();

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );
  int ret = activeRecorder->downloadFlight(flightID,!useFastDownload->isChecked(),fileName);

  delete statusDlg;
  qApp->processEvents();

  QApplication::restoreOverrideCursor();

  if( ret == FR_ERROR )
    {
      if( (errorDetails = activeRecorder->lastError()) != "" )
        {
          qWarning( "%s", (const char*) errorDetails );

          QMessageBox::critical(
              this,
              tr( "Library Error" ),
              tr( "There was an error downloading the flight!\n" ) + errorDetails,
              QMessageBox::Ok );
        }
      else
        {
          QMessageBox::critical( this, tr( "Library Error" ),
              tr( "There was an error downloading the flight!" ),
              QMessageBox::Ok );
        }
    }

  slotEnablePages();
}

void RecorderDialog::slotWriteDeclaration()
{
  QMessageBox* statusDlg = new QMessageBox ( tr("send declaration"), tr("send flight declaration to the recorder"),
      QMessageBox::Information, Qt::NoButton, Qt::NoButton,
      Qt::NoButton, this, "statusDialog", true);
  statusDlg->show();

  int ret;
  FRTaskDeclaration taskDecl;
  taskDecl.pilotA = pilotName->text();
  taskDecl.pilotB = copilotName->text();
  taskDecl.gliderID = gliderID->text();
  taskDecl.gliderType = gliderType->currentText();
  taskDecl.compID = editCompID->text();
  taskDecl.compClass = compClass->text();
  QString errorDetails;

  if (!activeRecorder) return;
  qApp->processEvents();
  if (!activeRecorder->capabilities().supUlDeclaration) {
    QMessageBox::warning(this,
                       tr("Declaration upload"),
                       tr("Function not implemented"), QMessageBox::Ok, 0);
    return;
  }


  if (taskSelection->currentItem() >= 0) {
    QList<Waypoint*> wpList = tasks.at(taskSelection->currentItem())->getWPList();

    qWarning("Writing task to logger...");

    ret=activeRecorder->writeDeclaration(&taskDecl,&wpList);

    //evaluate result
    if (ret==FR_NOTSUPPORTED) {
      QMessageBox::warning(this,
                         tr("Declaration upload"),
                         tr("Function not implemented"), QMessageBox::Ok, 0);
      return;
    }

    if (ret==FR_ERROR) {
      if ((errorDetails=activeRecorder->lastError())!="") {
        QMessageBox::critical(this,
            tr("Library Error"),
            tr("There was an error writing the declaration!") + errorDetails, QMessageBox::Ok, 0);
      } else {
        QMessageBox::critical(this,
                           tr("Library Error"),
                           tr("There was an error writing the declaration!"), QMessageBox::Ok, 0);
      }
      return;
    }

    qWarning("   ... ready (%d)", ret);
    QMessageBox::information(this,
        tr("Declaration upload"),
        tr("The declaration was uploaded to the recorder."), QMessageBox::Ok, 0);
  }

  delete statusDlg;

}

int RecorderDialog::__fillDirList()
{
  if( !activeRecorder )
    {
      return FR_ERROR;
    }

  if( activeRecorder->capabilities().supDlFlight )
    {
      return activeRecorder->getFlightDir( &dirList );
    }
  else
    {
      return FR_NOTSUPPORTED;
    }
}

bool RecorderDialog::__openLib( const QString& libN )
{
  qDebug() << "RecorderDialog::__openLib: " << libN;

  if( libName == libN )
    {
      qWarning( "OK, Library is already opened." );
      return true;
    }

  libName = "";
  apiID->setText("");
  serID->setText("");
  recType->setText("");
  pltName->setText("");
  gldType->setText("");
  gldID->setText("");
  compID->setText("");

  QString libPath = getLibraryPath() + "/" + libN;

  libHandle = dlopen( libPath.toAscii().data(), RTLD_NOW);

  char *error = (char *) dlerror();

  if (error != 0)
  {
    qWarning() << "RecorderDialog::__openLib() Error:" << error;

    QMessageBox::critical( this,
                           tr("Plugin is missing!"),
                           tr("Cannot open plugin library:\n\n" + libPath ) );

    return false;
  }

  FlightRecorderPluginBase* (*getRecorder)();

  getRecorder = (FlightRecorderPluginBase* (*) ()) dlsym(libHandle, "getRecorder");

  if( !getRecorder )
    {
      qWarning( "getRecorder function not defined in library!" );
      return false;
    }

  activeRecorder = getRecorder();

  if( !activeRecorder )
    {
      qWarning( "No recorder object returned!" );
      return false;
    }

  activeRecorder->setParent(this);

  apiID->setText(activeRecorder->getLibName());

  isOpen = true;
  libName = libN;

  return true;
}

void RecorderDialog::slotSwitchTask( int idx )
{
  FlightTask *task = tasks.at(idx);

  if( ! task )
    {
      return;
    }

  declarationList->clear();

  QList<Waypoint*> wpList = ((FlightTask*) task)->getWPList();

  for( int i = 0; i < wpList.size(); i++ )
    {
      Waypoint *wp = wpList.at(i);

      QTreeWidgetItem *item = new QTreeWidgetItem;

      item->setIcon(declarationColName, _globalMapConfig->getPixmap(wp->type, false, true) );

      item->setText(declarationColNo, QString("%1").arg( i + 1));
      item->setText(declarationColName, wp->name);
      item->setText(declarationColLat, WGSPoint::printPos(wp->origP.lat(), true));
      item->setText(declarationColLon, WGSPoint::printPos(wp->origP.lon(), false));
      item->setText(declarationColDummy, "");

      item->setTextAlignment( waypointColNo, Qt::AlignRight|Qt::AlignVCenter );
      item->setTextAlignment( waypointColName, Qt::AlignLeft|Qt::AlignVCenter );
      item->setTextAlignment( waypointColLat, Qt::AlignCenter );
      item->setTextAlignment( waypointColLon, Qt::AlignCenter );

      declarationList->insertTopLevelItem( i, item );
    }

  declarationList->slotResizeColumns2Content();
}

void RecorderDialog::slotReadTasks()
{
  if( !activeRecorder )
    {
      return;
    }

  if( !activeRecorder->capabilities().supDlTask )
    {
      QMessageBox::warning( this,
                            tr("Download task"),
                            tr("Function not implemented"),
                            QMessageBox::Ok );
      return;
    }

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Reading Tasks from recorder") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  int ret = activeRecorder->readTasks( &tasks );

  int cnt = 0;

  if( ret < FR_OK )
    {
      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Cannot read tasks from recorder." );

      if( ! errorDetails.isEmpty() )
        {
          errorText += "\n" + errorDetails;
        }

      QMessageBox::critical( this,
                             tr( "Library Error" ),
                             errorText,
                             QMessageBox::Ok );
    }
  else
    {
      FlightTask *task;
      Waypoint *wp;

      foreach(task, tasks)
        {
          QList<Waypoint*> wpList = task->getWPList();

          // here we overwrite the original task name (if needed) to get a unique internal name
          task->setTaskName( _globalMapContents->genTaskName( task->getFileName() ) );

          foreach(wp, wpList)
            {
              wp->projP = _globalMapMatrix->wgsToMap( wp->origP );
            }

          task->setWaypointList( wpList );
          emit addTask( task );
          cnt++;
        }

    // fill task list with new tasks
    fillTaskList( tasks );

    lblTaskList->setText( tr("Recorder Tasks") );

    QApplication::restoreOverrideCursor();

    QMessageBox::information( this,
                              tr("Task download"),
                              tr("%1 task(s) are downloaded from the recorder.").arg(cnt),
                              QMessageBox::Ok );
  }

  QApplication::restoreOverrideCursor();
  statusBar->setText("");
}

void RecorderDialog::slotWriteTasks()
{
  if( !activeRecorder || tasks.size() == 0 )
    {
      return;
    }

  if( !activeRecorder->capabilities().supUlTask )
    {
      QMessageBox::warning( this,
                            tr("Task upload"),
                            tr("Function not implemented"),
                            QMessageBox::Ok );
      return;
    }

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Writing Tasks to recorder") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();


  int maxNrTasks = activeRecorder->capabilities().maxNrTasks;

  if( maxNrTasks == 0 )
    {
      QMessageBox::critical( this,
                             tr( "Library Error" ),
                             tr( "Cannot obtain maximum number of tasks!" ),
                             QMessageBox::Ok );

      QApplication::restoreOverrideCursor();
      statusBar->setText("");
      return;
    }

  int maxNrWayPointsPerTask = activeRecorder->capabilities().maxNrWaypointsPerTask;

  if( maxNrWayPointsPerTask == 0 )
    {
      QMessageBox::critical( this,
                             tr( "Library Error" ),
                             tr( "Cannot obtain maximum number of waypoints per task!" ),
                             QMessageBox::Ok );

      QApplication::restoreOverrideCursor();
      statusBar->setText("");
      return;
    }

  int cnt;
  QList<Waypoint*> wpListCopy;
  Waypoint *wp;
  QList<FlightTask*> frTasks;

  for( cnt = 0; cnt < tasks.size(); cnt++ )
    {
      if( frTasks.size() > maxNrTasks )
        {
          QString msg = QString( tr( "Maximum number of %1 tasks reached!\n"
                                     "Further tasks will be ignored." ) ).arg(maxNrTasks);

          int res = QMessageBox::warning( this,
                                          tr( "Recorder Warning" ),
                                          msg,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No );

          if( res == QMessageBox::No )
            {
              qDeleteAll( frTasks );
              QApplication::restoreOverrideCursor();
              statusBar->setText( "" );
              return;
            }
          else
            {
              break;
            }
        }

      FlightTask *task = tasks.at(cnt);

      QList<Waypoint*> wpListOrig = task->getWPList();

      wpListCopy.clear();

      foreach(wp, wpListOrig)
      {
        if( wpListCopy.count() > maxNrWayPointsPerTask )
          {
            QString msg = QString( tr( "Maximum number of turnpoints/task %1 in %2 reached!\n"
                                       "Further turnpoints will be ignored." ) )
                                   .arg(maxNrWayPointsPerTask)
                                   .arg(task->getFileName() );

            int res = QMessageBox::warning( this,
                                            tr( "Recorder Warning" ),
                                            msg,
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::No );

            if( res == QMessageBox::No )
              {
                qDeleteAll( frTasks );
                QApplication::restoreOverrideCursor();
                statusBar->setText( "" );
                return;
              }
            else
              {
                break;
              }
          }

        wpListCopy.append(wp);
      }

      frTasks.append(new FlightTask(wpListCopy, true, task->getFileName()));
    }

  int ret = activeRecorder->writeTasks( &frTasks );

  if( ret < FR_OK )
    {
      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Cannot write tasks to recorder." );

      if( ! errorDetails.isEmpty() )
        {
          errorText += "\n" + errorDetails;
        }

      QMessageBox::critical( this,
                             tr( "Library Error" ),
                             errorText,
                             QMessageBox::Ok );
    }
  else
    {
      QMessageBox::information( this,
                                tr("Task upload"),
                                tr("%1 tasks were uploaded to the recorder.").arg(cnt),
                                QMessageBox::Ok );
    }

  qDeleteAll( frTasks );
  QApplication::restoreOverrideCursor();
  statusBar->setText( "" );
}

void RecorderDialog::slotReadWaypoints()
{
  QList<Waypoint*> frWaypoints;

  if( !activeRecorder )
    {
      return;
    }

  if( !activeRecorder->capabilities().supDlWaypoint )
    {
      QMessageBox::warning( this,
                            tr("Waypoints download"),
                            tr("Function not implemented"),
                            QMessageBox::Ok );
      return;
    }

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Reading Waypoints from recorder") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  int ret = activeRecorder->readWaypoints( &frWaypoints );

  if( ret < FR_OK )
    {
      QApplication::restoreOverrideCursor();

      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Cannot read waypoints from recorder." );

      if( ! errorDetails.isEmpty() )
        {
          errorText += "\n" + errorDetails;
        }

      QMessageBox::critical( this,
                             tr( "Library Error" ),
                             errorText,
                             QMessageBox::Ok );

      statusBar->setText("");
      return;
    }

  WaypointCatalog *wpCat = new WaypointCatalog( selectType->currentText() + "_" + serID->text() );
  wpCat->modified = true;

  for( int i = 0; i < frWaypoints.size(); i++ )
    {
      wpCat->insertWaypoint( frWaypoints.at( i ) );
    }

  emit addCatalog(wpCat);

  fillWaypointList( frWaypoints );

  lblWpList->setText( tr("Recorder Waypoints") );

  QApplication::restoreOverrideCursor();

  QMessageBox::information( this,
                            tr("Waypoints reading finished"),
                            tr("%1 waypoints have been read from the recorder.").arg(frWaypoints.size()),
                            QMessageBox::Ok );

  statusBar->setText("");
}

void RecorderDialog::slotWriteWaypoints()
{
  if( !activeRecorder || waypoints.size() == 0 )
    {
      return;
    }

  int res = QMessageBox::warning( this,
                                  tr("Waypoints upload"),
                                  tr("Uploading waypoints to the recorder will overwrite existing waypoints on the recorder. Do want to continue uploading?"),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No );

  if( res == QMessageBox::No )
    {
      return;
    }

  if( !activeRecorder->capabilities().supUlWaypoint )
    {
      QMessageBox::warning( this,
                            tr("Waypoints upload"),
                            tr("Function not implemented"),
                            QMessageBox::Ok );
      return;
    }

  statusBar->setText( tr("Writing Waypoints to recorder") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  int maxNrWaypoints = activeRecorder->capabilities().maxNrWaypoints;

  if( maxNrWaypoints == 0 )
    {
      QMessageBox::critical( this,
                             tr("Library Error"),
                             tr("Cannot obtain maximum number of waypoints from library"),
                             QMessageBox::Ok );

      statusBar->setText( "" );
      return;
    }

  QList<Waypoint*> frWaypoints;

  int cnt;

  for( cnt = 0; cnt < waypoints.size(); cnt++ )
    {
      if( frWaypoints.size() > maxNrWaypoints )
        {
          QString msg = QString( tr( "Maximum number of %1 waypoints reached!\n"
                                     "Further waypoints will be ignored." ) ).arg(maxNrWaypoints);

          int res = QMessageBox::warning( this,
                                          tr( "Recorder Warning" ),
                                          msg,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No );

          if( res == QMessageBox::No )
            {
              statusBar->setText( "" );
              return;
            }
          else
            {
              break;
            }
          }

        frWaypoints.append( waypoints.at(cnt) );
      }

    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

    int ret = activeRecorder->writeWaypoints( &frWaypoints );

    if( ret < FR_OK )
      {
        QApplication::restoreOverrideCursor();

        QString errorDetails = activeRecorder->lastError();

        QString errorText = tr( "Cannot write waypoints to recorder." );

        if( ! errorDetails.isEmpty() )
          {
            errorText += "\n" + errorDetails;
          }

        QMessageBox::critical( this,
                               tr( "Library Error" ),
                               errorText,
                               QMessageBox::Ok );
      }
    else
      {
        QMessageBox::information( this,
                                  tr("Waypoints upload"),
                                  QString(tr("%1 waypoints have been uploaded to the recorder.")).arg(cnt),
                                  QMessageBox::Ok );
      }

  QApplication::restoreOverrideCursor();
  statusBar->setText( "" );
}

void RecorderDialog::slotReadDatabase()
{
  if( !activeRecorder )
    {
      return;
    }

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Reading recorder data") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  FlightRecorderPluginBase::FR_Capabilities cap = activeRecorder->capabilities();

  int ret = activeRecorder->getBasicData(basicdata);

  if( ret == FR_OK )
    {
      if (cap.supDspSerialNumber)
        serID->setText(basicdata.serialNumber);
      if (cap.supDspRecorderType)
        recType->setText(basicdata.recorderType);
      if (cap.supDspPilotName)
        pltName->setText(basicdata.pilotName.stripWhiteSpace());
      if (cap.supDspGliderType)
        gldType->setText(basicdata.gliderType.stripWhiteSpace());
      if (cap.supDspGliderID)
        gldID->setText(basicdata.gliderID.stripWhiteSpace());
      if (cap.supDspCompetitionID)
        compID->setText(basicdata.competitionID.stripWhiteSpace());
    }
  else
    {
      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Sorry, could not connect to recorder.\n"
                              "Please check connections and settings." );

      if( ! errorDetails.isEmpty() )
        {
          errorText += "\n" + errorDetails;
        }

      QMessageBox::warning( this,
                             tr( "Recorder Connection" ),
                             errorText,
                             QMessageBox::Ok );

      QApplication::restoreOverrideCursor();
      statusBar->setText("");
      return;
    }

  if (cap.supEditGliderID     ||
      cap.supEditGliderType   ||
      cap.supEditGliderPolar  ||
      cap.supEditPilotName    ||
      cap.supEditUnits        ||
      cap.supEditGoalAlt      ||
      cap.supEditArvRadius    ||
      cap.supEditAudio        ||
      cap.supEditLogInterval)
    {
      int ret = activeRecorder->getConfigData( configdata );

      if( ret == FR_OK )
      {
        // now that we read the information from the logger, we can enable the write button:
        cmdUploadConfig->setEnabled(true);
        LD->setValue(configdata.LD);
        speedLD->setValue(configdata.speedLD);
        speedV2->setValue(configdata.speedV2);
        dryweight->setValue(configdata.dryweight);
        maxwater->setValue(configdata.maxwater);
        sinktone->setChecked(configdata.sinktone);
        approachradius->setValue(configdata.approachradius);
        arrivalradius->setValue(configdata.arrivalradius);
        goalalt->setValue(configdata.goalalt);
        sloginterval->setValue(configdata.sloginterval);
        floginterval->setValue(configdata.floginterval);
        gaptime->setValue(configdata.gaptime);
        minloggingspd->setValue(configdata.minloggingspd);
        stfdeadband->setValue(configdata.stfdeadband);
        unitVarioButtonGroup->setButton(configdata.units & FlightRecorderPluginBase::FR_Unit_Vario_kts);
        unitAltButtonGroup->setButton(configdata.units & FlightRecorderPluginBase::FR_Unit_Alt_ft);
        unitTempButtonGroup->setButton(configdata.units & FlightRecorderPluginBase::FR_Unit_Temp_F);
        unitQNHButtonGroup->setButton(configdata.units & FlightRecorderPluginBase::FR_Unit_Baro_inHg);
        unitDistButtonGroup->setButton(configdata.units & (FlightRecorderPluginBase::FR_Unit_Dist_nm|FlightRecorderPluginBase::FR_Unit_Dist_sm));
        unitSpeedButtonGroup->setButton(configdata.units & (FlightRecorderPluginBase::FR_Unit_Spd_kts|FlightRecorderPluginBase::FR_Unit_Spd_mph));
      }
  }

  if (cap.supEditGliderID     ||
      cap.supEditGliderType   ||
      cap.supEditPilotName)
    {
      cmdUploadBasicConfig->setEnabled(true);
      pltName->setEnabled(true);
      gldType->setEnabled(true);
      gldID->setEnabled(true);
    }

  QApplication::restoreOverrideCursor();
  statusBar->setText("");
}

void RecorderDialog::slotWriteConfig()
{
  if( !activeRecorder )
    {
      return;
    }

  basicdata.pilotName = pltName->text();
  basicdata.gliderType = gldType->text();
  basicdata.gliderID = gldID->text();
  basicdata.competitionID = compID->text();

  configdata.LD = atoi(LD->text());
  configdata.speedLD = atoi(speedLD->text());
  configdata.speedV2 = atoi(speedV2->text());
  configdata.dryweight = atoi(dryweight->text());
  configdata.maxwater = atoi(maxwater->text());

  configdata.sinktone = (int)sinktone->isChecked();

  configdata.approachradius = atoi(approachradius->text());
  configdata.arrivalradius = atoi(arrivalradius->text());
  configdata.goalalt = atoi(goalalt->text());
  configdata.sloginterval = atoi(sloginterval->text());
  configdata.floginterval = atoi(floginterval->text());
  configdata.gaptime = atoi(gaptime->text());
  configdata.minloggingspd = atoi(minloggingspd->text());
  configdata.stfdeadband = atoi(stfdeadband->text());

  configdata.units = unitAltButtonGroup->selectedId()   |
                     unitVarioButtonGroup->selectedId() |
                     unitSpeedButtonGroup->selectedId() |
                     unitQNHButtonGroup->selectedId()   |
                     unitTempButtonGroup->selectedId()  |
                     unitDistButtonGroup->selectedId();

  QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

  statusBar->setText( tr("Writing recorder data") );
  QCoreApplication::processEvents();
  QCoreApplication::flush();

  int ret = activeRecorder->writeConfigData(basicdata, configdata);

  if( ret != FR_OK )
    {
      QString errorDetails = activeRecorder->lastError();

      QString errorText = tr( "Sorry, could not write configuration to recorder.\n"
                              "Please check connections and settings." );

      if( ! errorDetails.isEmpty() )
        {
          errorText += "\n" + errorDetails;
        }

      QMessageBox::warning( this,
                             tr( "Recorder Connection" ),
                             errorText,
                             QMessageBox::Ok );
    }

  QApplication::restoreOverrideCursor();
  statusBar->setText("");
}

void RecorderDialog::slotDisablePages()
{
  flightPage->setEnabled(false);
  waypointPage->setEnabled(false);
  taskPage->setEnabled(false);
  declarationPage->setEnabled(false);
  configPage->setEnabled(false);
}

/** Enable/Disable pages when (not) connected to a recorder */
void RecorderDialog::slotEnablePages()
{
  //first, disable all pages
  configPage->setEnabled(false);
  declarationPage->setEnabled(false);
  flightPage->setEnabled(false);
  taskPage->setEnabled(false);
  waypointPage->setEnabled(false);

  // If there is an active recorder and that recorder is connected,
  // selectively re-activate them.
  if( !activeRecorder )
    {
      return;
    }

  FlightRecorderPluginBase::FR_Capabilities cap=activeRecorder->capabilities();

  if( isConnected )
    {
      // flight page
      if( cap.supDlFlight )
        {
          flightPage->setEnabled( true );
          useFastDownload->setEnabled( cap.supSignedFlight );
        }

      // waypoint page
      if( cap.supDlWaypoint || cap.supUlWaypoint )
        {
          waypointPage->setEnabled( true );
          cmdUploadWaypoints->setEnabled( cap.supUlWaypoint );
          cmdDownloadWaypoints->setEnabled( cap.supDlWaypoint );
        }

      // task page
      if( cap.supDlTask || cap.supUlTask )
        {
          taskPage->setEnabled( true );
          cmdUploadTasks->setEnabled( cap.supUlTask );
          cmdDownloadTasks->setEnabled( cap.supDlTask );
        }

      // declaration page
      if( cap.supUlDeclaration )
        {
          declarationPage->setEnabled( true );
        }

    // configuration page
    if (cap.supEditGliderPolar  ||
        cap.supEditUnits        ||
        cap.supEditGoalAlt      ||
        cap.supEditArvRadius    ||
        cap.supEditAudio        ||
        cap.supEditLogInterval)
      {
        configPage->setEnabled(true);
      }
  }
}

/** Opens the new recorder plugin library, if necessary. */
void RecorderDialog::slotRecorderTypeChanged(const QString& newRecorderName )
{
  if( newRecorderName.isEmpty() )
    {
      return;
    }

  QString newLibName = libNameList[newRecorderName];

  if( isOpen && libName != newLibName )
    {
      // closing old library
      dlclose( libHandle );
      slotCloseRecorder();
      isConnected = isOpen = false;
      slotEnablePages();
    }

  if( ! __openLib( newLibName ) )
    {
      __setRecorderConnectionType( FlightRecorderPluginBase::none );
      return;
    }

  __setRecorderConnectionType( activeRecorder->getTransferMode() );
  __setRecorderCapabilities();
}

/**
  *  If the recorder supports auto-detection of transfer speed,
  *  it will signal the new speed to adjust the gui
  */
void RecorderDialog::slotNewSpeed (int speed)
{
  selectSpeed->setCurrentText(QString("%1").arg(speed));
}

/** No descriptions */
void RecorderDialog::__addConfigPage()
{
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText( 0, tr("Configuration") );
  item->setData( 0, Qt::UserRole, "Configuration" );
  item->setIcon( 0, _mainWindow->getPixmap("kde_configure_48.png") );
  setupTree->addTopLevelItem( item );

  configPage = new QWidget(this);
  configPage->setObjectName( "ConfigurationPage" );
  configPage->setVisible( false );

  configLayout->addWidget( configPage, 0, 1 );

  //----------------------------------------------------------------------------

  QGridLayout* configLayout = new QGridLayout(configPage, 16, 8, 10, 1);

  Q3GroupBox* gGroup = new Q3GroupBox(configPage, "gliderGroup");
  gGroup->setTitle(tr("Glider Settings") + ":");

  Q3GroupBox* vGroup = new Q3GroupBox(configPage, "varioGroup");
  vGroup->setTitle(tr("Variometer Settings") + ":");

  configLayout->addMultiCellWidget(gGroup, 0, 4, 0, 7);
  configLayout->addMultiCellWidget(vGroup, 5, 14, 0, 7);

  QGridLayout *ggrid = new QGridLayout(gGroup, 3, 4, 15, 1);

  ggrid->addWidget(new QLabel(tr("Best L/D:"), gGroup), 0, 0);
  LD = new QSpinBox (gGroup, "LD");
  LD->setRange(0, 255);
  LD->setLineStep(1);
  LD->setButtonSymbols(QSpinBox::PlusMinus);
  LD->setEnabled(true);
  LD->setValue(0);
  ggrid->addWidget(LD, 0, 1);

  ggrid->addWidget(new QLabel(tr("Best L/D speed (km/h):"), gGroup), 1, 0);
  speedLD = new QSpinBox (gGroup, "speedLD");
  speedLD->setRange(0, 255);
  speedLD->setLineStep(1);
  speedLD->setButtonSymbols(QSpinBox::PlusMinus);
  speedLD->setEnabled(true);
  speedLD->setValue(0);
  ggrid->addWidget(speedLD, 1, 1);

  ggrid->addWidget(new QLabel(tr("2 m/s sink speed (km/h):"), gGroup), 2, 0);
  speedV2 = new QSpinBox (gGroup, "speedV2");
  speedV2->setRange(0, 255);
  speedV2->setLineStep(1);
  speedV2->setButtonSymbols(QSpinBox::PlusMinus);
  speedV2->setEnabled(true);
  speedV2->setValue(0);
  ggrid->addWidget(speedV2, 2, 1);

  ggrid->addWidget(new QLabel(tr("Dry weight (kg):"), gGroup), 0, 2);
  dryweight = new QSpinBox (gGroup, "dryweight");
  dryweight->setRange(0, 1000);
  dryweight->setLineStep(1);
  dryweight->setButtonSymbols(QSpinBox::PlusMinus);
  dryweight->setEnabled(true);
  dryweight->setValue(0);
  ggrid->addWidget(dryweight, 0, 3);

  ggrid->addWidget(new QLabel(tr("Max. water ballast (l):"), gGroup), 1, 2);
  maxwater = new QSpinBox (gGroup, "maxwater");
  maxwater->setRange(0, 500);
  maxwater->setLineStep(1);
  maxwater->setButtonSymbols(QSpinBox::PlusMinus);
  maxwater->setEnabled(true);
  maxwater->setValue(0);
  ggrid->addWidget(maxwater, 1, 3);


  QGridLayout *vgrid = new QGridLayout(vGroup, 9, 7, 15, 1);

  vgrid->addWidget(new QLabel(tr("Approach radius (m):"), vGroup), 0, 0);
  approachradius = new QSpinBox (vGroup, "approachradius");
  approachradius->setRange(0, 65535);
  approachradius->setLineStep(10);
  approachradius->setButtonSymbols(QSpinBox::PlusMinus);
  approachradius->setEnabled(true);
  approachradius->setValue(0);
  vgrid->addWidget(approachradius, 0, 1);

  vgrid->addWidget(new QLabel(tr("Arrival radius (m):"), vGroup), 2, 0);
  arrivalradius = new QSpinBox (vGroup, "arrivalradius");
  arrivalradius->setRange(0, 65535);
  arrivalradius->setLineStep(10);
  arrivalradius->setButtonSymbols(QSpinBox::PlusMinus);
  arrivalradius->setEnabled(true);
  arrivalradius->setValue(0);
  vgrid->addWidget(arrivalradius, 2, 1);

  vgrid->addWidget(new QLabel(tr("Goal altitude (m):"), vGroup), 4, 0);
  goalalt = new QSpinBox (vGroup, "goalalt");
  goalalt->setRange(0, 6553);
  goalalt->setLineStep(1);
  goalalt->setButtonSymbols(QSpinBox::PlusMinus);
  goalalt->setEnabled(true);
  goalalt->setValue(0);
  vgrid->addWidget(goalalt, 4, 1);

  vgrid->addWidget(new QLabel(tr("Min. flight time (min):"), vGroup), 6, 0);
  gaptime = new QSpinBox (vGroup, "gaptime");
  gaptime->setRange(0, 600);
  gaptime->setLineStep(1);
  gaptime->setButtonSymbols(QSpinBox::PlusMinus);
  gaptime->setEnabled(true);
  gaptime->setValue(0);
  vgrid->addWidget(gaptime, 6, 1);

  vgrid->addWidget(new QLabel(tr("Slow log interval (s):"), vGroup), 0, 2);
  sloginterval = new QSpinBox (vGroup, "sloginterval");
  sloginterval->setRange(0, 600);
  sloginterval->setLineStep(1);
  sloginterval->setButtonSymbols(QSpinBox::PlusMinus);
  sloginterval->setEnabled(true);
  sloginterval->setValue(0);
  vgrid->addWidget(sloginterval, 0, 3);

  vgrid->addWidget(new QLabel(tr("Fast log interval (s):"), vGroup), 2, 2);
  floginterval = new QSpinBox (vGroup, "floginterval");
  floginterval->setRange(0, 600);
  floginterval->setLineStep(1);
  floginterval->setButtonSymbols(QSpinBox::PlusMinus);
  floginterval->setEnabled(true);
  floginterval->setValue(0);
  vgrid->addWidget(floginterval, 2, 3);

  vgrid->addWidget(new QLabel(tr("Min. logging spd (km/h):"), vGroup), 4, 2);
  minloggingspd = new QSpinBox (vGroup, "minloggingspd");
  minloggingspd->setRange(0, 100);
  minloggingspd->setLineStep(1);
  minloggingspd->setButtonSymbols(QSpinBox::PlusMinus);
  minloggingspd->setEnabled(true);
  minloggingspd->setValue(0);
  vgrid->addWidget(minloggingspd, 4, 3);

  vgrid->addWidget(new QLabel(tr("Audio dead-band (km/h):"), vGroup), 6, 2);
  stfdeadband = new QSpinBox (vGroup, "stfdeadband");
  stfdeadband->setRange(0, 90);
  stfdeadband->setLineStep(1);
  stfdeadband->setButtonSymbols(QSpinBox::PlusMinus);
  stfdeadband->setEnabled(true);
  stfdeadband->setValue(0);
  vgrid->addWidget(stfdeadband, 6, 3);

  vgrid->addWidget(new QLabel("     ", vGroup), 0, 4);  // Just a filler

  vgrid->addWidget(new QLabel(tr("Altitude:"), vGroup), 0, 5);
  unitAltButtonGroup = new Q3ButtonGroup(vGroup);
  unitAltButtonGroup-> hide();
  unitAltButtonGroup-> setExclusive(true);
  QRadioButton *rb = new QRadioButton(tr("m"), vGroup);
  unitAltButtonGroup-> insert(rb, 0);
  vgrid->addWidget(rb, 1, 5);
  rb = new QRadioButton(tr("ft"), vGroup);
  unitAltButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Alt_ft);
  vgrid->addWidget(rb, 2, 5);

  vgrid->addWidget(new QLabel(tr("QNH:"), vGroup), 5, 5);
  unitQNHButtonGroup = new Q3ButtonGroup(vGroup);
  unitQNHButtonGroup-> hide();
  unitQNHButtonGroup-> setExclusive(true);
  rb = new QRadioButton(tr("mbar"), vGroup);
  unitQNHButtonGroup-> insert(rb, 0);
  vgrid->addWidget(rb, 6, 5);
  rb = new QRadioButton(tr("inHg"), vGroup);
  unitQNHButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Baro_inHg);
  vgrid->addWidget(rb, 7, 5);

  vgrid->addWidget(new QLabel(tr("Speed:"), vGroup), 0, 6);
  unitSpeedButtonGroup = new Q3ButtonGroup(vGroup);
  unitSpeedButtonGroup-> hide();
  unitSpeedButtonGroup-> setExclusive(true);
  rb = new QRadioButton(tr("km/h"), vGroup);
  unitSpeedButtonGroup-> insert(rb, 0);
  vgrid->addWidget(rb, 1, 6);
  rb = new QRadioButton(tr("kts"), vGroup);
  unitSpeedButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Spd_kts);
  vgrid->addWidget(rb, 2, 6);
  rb = new QRadioButton(tr("mph"), vGroup);
  unitSpeedButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Spd_mph);
  vgrid->addWidget(rb, 3, 6);

  vgrid->addWidget(new QLabel(tr("Vario:"), vGroup), 5, 6);
  unitVarioButtonGroup = new Q3ButtonGroup(vGroup);
  unitVarioButtonGroup-> hide();
  unitVarioButtonGroup-> setExclusive(true);
  rb = new QRadioButton(tr("m/s"), vGroup);
  unitVarioButtonGroup-> insert(rb, 0);
  vgrid->addWidget(rb, 6, 6);
  rb = new QRadioButton(tr("kts"), vGroup);
  unitVarioButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Vario_kts);
  vgrid->addWidget(rb, 7, 6);

  vgrid->addWidget(new QLabel(tr("Distance:"), vGroup), 0, 7);
  unitDistButtonGroup = new Q3ButtonGroup(vGroup);
  unitDistButtonGroup-> hide();
  unitDistButtonGroup-> setExclusive(true);
  rb = new QRadioButton(tr("km"), vGroup);
  unitDistButtonGroup-> insert(rb, 0);
  vgrid->addWidget(rb, 1, 7);
  rb = new QRadioButton(tr("nm"), vGroup);
  unitDistButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Dist_nm);
  vgrid->addWidget(rb, 2, 7);
  rb = new QRadioButton(tr("sm"), vGroup);
  unitDistButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Dist_sm);
  vgrid->addWidget(rb, 3, 7);

  vgrid->addWidget(new QLabel(tr("Temp.:"), vGroup), 5, 7);
  unitTempButtonGroup = new Q3ButtonGroup(vGroup);
  unitTempButtonGroup-> hide();
  unitTempButtonGroup-> setExclusive(true);
  rb = new QRadioButton(tr("C"), vGroup);
  unitTempButtonGroup-> insert(rb, 0);
  vgrid->addWidget(rb, 6, 7);
  rb = new QRadioButton(tr("F"), vGroup);
  unitTempButtonGroup-> insert(rb, FlightRecorderPluginBase::FR_Unit_Temp_F);
  vgrid->addWidget(rb, 7, 7);

  sinktone = new QCheckBox(tr("sink tone"), vGroup);
  sinktone->setChecked(true);
  vgrid->addWidget(sinktone, 8, 2);


  cmdUploadConfig = new QPushButton(tr("write config to recorder"), configPage);
  // disable this button until we read the information from the flight recorder:
  cmdUploadConfig->setEnabled(false);
  connect(cmdUploadConfig, SIGNAL(clicked()), SLOT(slotWriteConfig()));
  configLayout->addWidget(cmdUploadConfig, 15, 6);
}
