/***********************************************************************
**
**   struct.h
**
**   This file is part of kio-logger.
**
************************************************************************
**
**   Copyright (c):  2002 by Heiner Lamprecht
**
**   This file is distributed under the terms of the General Public
**   License. See the file COPYING for more information.
**
**   $Id$
**
***********************************************************************/

#ifndef FRSTRUCTS_H
#define FRSTRUCTS_H

#include <time.h>

#include <QString>

/**
 *
 */
struct FRDirEntry
{
  /**
   * Contains the name of the pilot. If there is a co-pilot in the
   * glider, he is not listed here.
   */
  QString pilotName;
  /**
   * The ID of the glider
   */
  QString gliderID;
  /**
   * The type of the glider.
   */
  QString gliderType;
  /**
   * The short filename used for this flight. The naming sheme is
   * defined by the IGC.
   */
  QString shortFileName;
  /**
   * The long filename used for this flight. The naming sheme is
   * defined by the IGC.
   */
  QString longFileName;
  /**
   * The time of the first fix (i.e. first B-record) of the flight.
   * According to the IGC-specification, the time is given in UTC.
   */
  tm firstTime;
  /**
   * The time of the last fix (i.e. last B-record) of the flight.
   * According to the IGC-specification, the time is given in UTC.
   */
  tm lastTime;
  /**
   * The duration of the flight in seconds.
   */
  int duration;
};

/** */
struct FRTaskDeclaration
{
  QString pilotA;
  QString pilotB;
  QString gliderID;
  QString gliderType;
  QString compID;
  QString compClass;
};

#endif
