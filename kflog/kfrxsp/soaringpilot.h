/***********************************************************************
**
**   soaringpilot.h
**
**   This file is part of KFLog2.
**
************************************************************************
**
**   Copyright (c):  2003 by Harald Maier
**
**   This file is distributed under the terms of the General Public
**   Licence. See the file COPYING for more information.
**
**   $Id$
**
***********************************************************************/

#ifndef SOARINGPILOT_H
#define SOARINGPILOT_H

#include <qstring.h>

#include <../frstructs.h>
#include <../flighttask.h>
#include <../flightrecorderpluginbase.h>

/**Implementation for SoaringPilot.

  *@author Harald Maier
  */

class SoaringPilot:public FlightRecorderPluginBase
{
public: 
  SoaringPilot();
  ~SoaringPilot();

  /**
   * Returns the name of the lib.
   */
  virtual QString getLibName();
  /**
   * Returns the transfermode this plugin supports.
   */
  virtual TransferMode getTransferMode();
  /**
   * Returns a list of recorded flights in this device.
   */
  virtual int getFlightDir(QList<FRDirEntry>*);
  /**
   *
   */
  virtual int downloadFlight(int flightID, int secMode, const QString fileName);
  /**
   * get recorder info serial id
   */
  virtual QString getRecorderSerialNo();
  /**
   * Opens the recorder for serial communication.
   */
  virtual int openRecorder(const QString portName, int baud);
  /**
   * Opens the recorder for other communication.
   */
  virtual int openRecorder(QString URL);
  /**
   * Closes the connection with the flightrecorder.
   */
  virtual int closeRecorder();
  /**
   * Write flight declaration to recorder
   */
  virtual int writeDeclaration(FRTaskDeclaration *taskDecl, QList<Waypoint> *taskPoints);
  /**
   * Read waypoint and flight declaration form from recorder into mem
   */
  virtual int readDatabase();
  /**
   * Read tasks from recorder
   */
  virtual int readTasks(QList<FlightTask> *tasks);
  /**
   * Write tasks to recorder
   */
  virtual int writeTasks(QList<FlightTask> *tasks);
  /**
   * Read waypoints from recorder
   */
  virtual int readWaypoints(QList<Waypoint> *waypoints);
  /**
   * Write waypoints to recorder
   */
  virtual int writeWaypoints(QList<Waypoint> *waypoints);

private: // Private methods
  /** write a file like structure to the device */
  int writeFile(QStringList &file);
  /** read a file like structure from the device */
  int readFile(QStringList &file);
  int coordToDegree(QString &s);
  QString degreeToDegMin(int d, bool isLat);
  QString degreeToDegMinSec(int d, bool isLat);
  int feetToMeter(QString &s);
  QString meterToFeet(int m);
};

#endif