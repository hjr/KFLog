/***********************************************************************
**
**   mapcontents.cpp
**
**   This file is part of KFLog2.
**
************************************************************************
**
**   Copyright (c):  2000 by Heiner Lamprecht, Florian Ehinger
**
**   This file is distributed under the terms of the General Public
**   Licence. See the file COPYING for more information.
**
**   $Id$
**
***********************************************************************/

#include <cmath>
#include <stdlib.h>

#include "mapcontents.h"
#include <mapcalc.h>

#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstddirs.h>
#include <qdatastream.h>
#include <qdatetime.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qprogressdialog.h>
#include <qregexp.h>
#include <qstring.h>
#include <qtextstream.h>

#include <mapmatrix.h>

#include <airport.h>
#include <airspace.h>
#include <basemapelement.h>
#include <elevpoint.h>
#include <flight.h>
#include <glidersite.h>
#include <isohypse.h>
#include <lineelement.h>
#include <radiopoint.h>
#include <singlepoint.h>

#define MAX_FILE_COUNT 16200
#define ISO_LINE_NUM 46

#define MAP_FILE_FORMAT "#KFLog-Map-file Version: 0.6 (c) 2000 The KFLog-Team"
#define DEM_FILE_FORMAT "#KFLog-DEM-file Version: 0.6 (c) 2000 The KFLog-Team"

#define CHECK_BORDER if(i == 0) { \
    border.north = lat_temp;   border.south = lat_temp; \
    border.east = lon_temp;    border.west = lon_temp; \
  } else { \
    border.north = MAX(border.north, lat_temp); \
    border.south = MIN(border.south, lat_temp); \
    border.east = MAX(border.east, lon_temp); \
    border.west = MIN(border.west, lon_temp); \
  }

#define READ_POINT_LIST tA.resize(length); \
  for(unsigned int i = 0; i < length; i++) { \
    in >> lat_temp;          in >> lon_temp; \
    tA.setPoint(i, _globalMapMatrix.wgsToMap(lat_temp, lon_temp)); \
  }

#define READ_POINT_LIST_ISO latList = new int[isoLength]; \
  lonList = new int[isoLength]; \
  for(unsigned int i = 0; i < isoLength; i++) { \
    in >> lat_temp;          in >> lon_temp; \
    latList[i] = lat_temp;   lonList[i] = lon_temp; \
    CHECK_BORDER \
  }

MapContents::MapContents()
  : isFirst(true)
{
  // Liste der H�henstufen (insg. 46 Stufen):
  // Compiler gibt hier eine Warnung aus. Gibt es eine andere M�glichkeit?
  isoLines = new int[ISO_LINE_NUM]  = { 0, 10, 25, 50, 75, 100, 200, 300, 400,
            500, 600, 700, 800, 900, 1000, 1250, 1500, 1750, 2000, 2250, 2500,
            2750, 3000, 3250, 3500, 3750, 4000, 4250, 4500, 4750, 5000, 5250,
            5500, 5750, 6000, 6250, 6500, 6750, 7000, 7250, 7500, 7750,
            8000, 8250, 8500, 8750};

  sectionArray.resize(MAX_FILE_COUNT);
  for(unsigned int loop = 0; loop < MAX_FILE_COUNT; loop++)
      sectionArray.clearBit(loop);

  // Wir nehmen zun�chst 4 Schachtelungstiefen an ...
  for(unsigned int loop = 0; loop < ( ISO_LINE_NUM * 4 ); loop++)
      isoList.append(new QList<Isohypse>);

  airportList.setAutoDelete(true);
  airspaceList.setAutoDelete(true);
  gliderList.setAutoDelete(true);
  flightList.setAutoDelete(true);
  highEntryList.setAutoDelete(true);
  highwayList.setAutoDelete(true);
  hydroList.setAutoDelete(true);
  landmarkList.setAutoDelete(true);
  navList.setAutoDelete(true);
  obstacleList.setAutoDelete(true);
  outList.setAutoDelete(true);
  railList.setAutoDelete(true);
  reportList.setAutoDelete(true);
  roadList.setAutoDelete(true);
  stationList.setAutoDelete(true);
  topoList.setAutoDelete(true);

  waypointNumber = 0;
  waypointList = new unsigned int[waypointNumber];
  waypointIndex = new unsigned int[waypointNumber];
}

MapContents::~MapContents()
{
  delete[] isoLines;
  // Hier m�ssen ALLE Listen gel�scht werden!!!
  airportList.~QList();
  airspaceList.~QList();
  cityList.~QList();
  gliderList.~QList();
  flightList.~QList();
  highEntryList.~QList();
  highwayList.~QList();
  hydroList.~QList();
  landmarkList.~QList();
  navList.~QList();
  obstacleList.~QList();
  outList.~QList();
  railList.~QList();
  reportList.~QList();
  roadList.~QList();
  stationList.~QList();
  topoList.~QList();
}

int MapContents::__degreeToNum(const char* degree)
{
  int deg = 0, min = 0, sec = 0;
  int result;
  int count = 0;

  QRegExp number("^-?[0-9]+$");
  QRegExp deg1("[�.]");
  QRegExp deg2(",");
  QRegExp deg3("'");
  QRegExp dir("[swSW]$");
  if(number.match(degree) != -1)
    {
      unsigned int id = 1;
      if(degree[0] == '-')
        {
          result = degree[1] - '0';
          id++;
        }
      else
        result = degree[0] - '0';

      for(unsigned int loop = id ; loop < strlen(degree); loop++)
        result = 10 * result + (degree[loop] - '0');

      if(id == 2) return -result;

      return result;
    }

  switch(deg1.match(degree))
    {
      case 1:
        deg = degree[0] - '0';
        degree += 2;
        break;
      case 2:
        deg = 10 * (degree[0] - '0') + (degree[1] - '0');
        degree += 3;
        break;
      case 3:
        deg = 100 * (degree[0] - '0') + 10 * (degree[1] - '0')
            + (degree[2] - '0');
        degree += 4;
        break;
      default:
        if(deg1.match(degree) > 3)  return 0;    // << degree is not correct!
        switch(strlen(degree))
          {
            case 1:
              deg = degree[0] - '0';
              break;
            case 2:
              deg = 10 * (degree[0] - '0') + (degree[1] -'0');
              break;
            case 3:
              deg = 100 * (degree[0] - '0') + 10 * (degree[1] - '0')
                    + (degree[2] - '0');
              break;
            default:
              return 0;                           // << degree is not correct!
          }
    }

  if( deg2.match(degree) != -1 )
    {
      // Minuten mit Nachkommastellen!
      switch(deg2.match(degree))
        {
          case 1:
            min = degree[0] - '0';
            for(unsigned int loop = 2; loop < strlen(degree); loop++)
              if( ( degree[loop] >= '0' ) && ( degree[loop] <= '9' ) )
                {
                  sec = 10 * sec + (degree[loop] - '0');
                  count++;
                }
            break;
          case 2:
            min = 10 * (degree[0] - '0') + (degree[1] -'0');
            for(unsigned int loop = 3; loop < strlen(degree); loop++)
              if( ( degree[loop] >= '0' ) && ( degree[loop] <= '9' ) )
                {
                  sec = (degree[loop] - '0') + (sec / 10);
                  count++;
                }
            break;
          default:
            if( ( deg2.match(degree) > 2 ) || ( deg2.match(degree) == 0 ) )
                return 0;    // << degree is not correct!
        }
    }
  else if( deg3.match(degree) != -1 )
    {
      // es folgen "echte" Sekunden
      switch( deg3.match(degree) )
        {
          case 1:
            min = degree[0] - '0';
            for(unsigned int loop = 2; loop < strlen(degree); loop++)
              if( ( degree[loop] >= '0' ) && ( degree[loop] <= '9' ) )
                {
                  sec = sec * 10 + (degree[loop] - '0');
                  count++;
                }
            break;
          case 2:
            min = 10 * (degree[0] - '0') + (degree[1] -'0');
            for(unsigned int loop = 3; loop < strlen(degree); loop++)
              if( ( degree[loop] >= '0' ) && ( degree[loop] <= '9' ) )
                {
                  sec = sec * 10 + (degree[loop] - '0');
                  count++;
                }
            break;
          default:
            if( ( deg2.match(degree) > 2 ) || ( deg2.match(degree) == 0 ) )
                return 0;    // << degree is not correct!
        }
    }
  result = (int) ((600000 * deg) + (10000 * (min + (sec * pow(10,-count)))));

  if(dir.match(degree) >= 0) return -result;

  return result;
}

bool MapContents::__readAsciiFile(const char* fileName)
{
  extern const MapMatrix _globalMapMatrix;

  QFile file(fileName);
  QTextStream stream(&file);
  QString line;

  unsigned int ulimit = 0, llimit = 0, ulimitType = 0, llimitType = 0;

  if(!file.open(IO_ReadOnly))
    {
      // Fehler-Handling bislang nicht ausreichend ...
      warning("KFLog: No Mapfile found: %s", fileName);
      return false;
    }

  warning("KFLog: parsing mapfile %s", fileName);

  QProgressDialog importProgress(0,0,true);

  importProgress.setCaption(i18n("Import mapfile ..."));
  importProgress.setLabelText((QString)i18n("Please wait while loading file")
          + "<BR><B>" + fileName + "</B>");
  importProgress.setMinimumWidth(importProgress.sizeHint().width() + 45);
  importProgress.show();
  importProgress.setMinimumDuration(0);

  importProgress.setProgress(0);

  QFileInfo fileInfo(fileName);
  unsigned int fileLength = fileInfo.size();
  unsigned int filePos = 0;

  bool isObject = false;
  bool isValley = false;
  int objectCount = 0;
  int l = 0;
  int* runwayDir;
  int* runwayLength;
  int* runwayType;
  int lat_temp = 0, lon_temp = 0;
  int* latitude;
  int* longitude;
  unsigned int posLength;
  unsigned int runLength;

  QString alias = 0;
  QString abbr = 0;
  unsigned int elev = 0;
  QString frequency = 0;
  QString name;
  unsigned int type = 0;
  bool vdf, winch, isWayP;
  int sortID = 0;

  QPointArray tA;
  QPoint position;

  Airspace* newAir;
  LineElement* newLine;

  while (!stream.eof())
    {
      if(importProgress.wasCancelled()) break;

      line = stream.readLine();
      filePos += line.length();
      importProgress.setProgress(( filePos * 100 ) / fileLength);
      line = line.simplifyWhiteSpace();
      // if it is a comment, ignore it!
      if(line.mid(0,1) == "#") continue;
      if(line.mid(0,5) == "[NEW]")
        {
          // a new object starts here
          isObject = true;
          objectCount++;

          l = 0;
          name = "";
          type = 0;
          ulimit = 0;
          vdf = false;
          isValley = false;
          frequency = "";
          alias = "";
          elev = 0;
          winch = false;
          lat_temp = 0;
          lon_temp = 0;
          posLength = 0;
          runLength = 0;
          sortID = 0;
          latitude = new int[1];
          longitude = new int[1];
          runwayDir = new int[1];
          runwayLength = new int[1];
          runwayType = new int[1];
          isWayP = false;

          tA.resize(0);
        }
      else if(isObject)
        {
          if(line.mid(0,4) == "TYPE")
              type = line.mid(5,2).toUInt();
          else if(line.mid(0,5) == "ALIAS")
              alias = line.mid(6,100);
          else if(line.mid(0,6) == "ABBREV")
              abbr = line.mid(7,6);
          else if(line.mid(0,2) == "AT")
            {
              unsigned int loop;
              for(loop = 3; loop < strlen(line); loop++)
                  if(line.mid(loop, 1) == " ") break;

              position = _globalMapMatrix.wgsToMap(
                  __degreeToNum( line.mid( 3, ( loop - 3 ) ) ),
                  __degreeToNum( line.mid( ( loop + 1 ), 100 ) ) );
            }
          else if(line.mid(0,5) == "LTYPE")
              llimitType = line.mid(6,1).toUInt();
          else if(line.mid(0,5) == "UTYPE")
              ulimitType = line.mid(6,1).toUInt();
          else if(line.mid(0,5) == "LOWER")
              llimit = line.mid(6,100).toUInt();
          else if(line.mid(0,5) == "UPPER")
              ulimit = line.mid(6,100).toUInt();
          else if(line.mid(0,9) == "ELEVATION")
              elev = line.mid(10,100).toUInt();
          else if(line.mid(0,4) == "ELEV")
              elev = line.mid(5,100).toUInt();
          else if(line.mid(0,9) == "FREQUENCY")
              frequency = line.mid(10,100);
          else if(line.mid(0,8) == "MOUNTAIN")
            {
              if(line.mid(9,10).toInt() == 1)
                  isValley = true;
            }
          else if(line.mid(0,4) == "NAME")
              name = line.mid(5,1000);
          else if(line.mid(0,4) == "SORT")
              sortID = line.mid(5,10).toInt();
          else if(line.mid(0,6) == "RUNWAY")
            {
              runLength++;
              runwayDir = (int*)realloc(runwayDir,
                  (runLength * sizeof(int)));
              runwayLength = (int*)realloc(runwayLength,
                  (runLength * sizeof(int)));
              runwayType = (int*)realloc(runwayType,
                  (runLength * sizeof(int)));

              unsigned int loop;
              unsigned int loop2;
              for(loop = 0; loop < strlen(line); loop++)
                  if(line.mid(loop, 1) == " ") break;

              runwayDir[runLength - 1] = line.mid(7,(loop - 7)).toInt();
              loop2 = ++loop;
              for(loop = loop; loop < strlen(line); loop++)
                  if(line.mid(loop, 1) == " ") break;

              runwayLength[runLength - 1] = line.mid(loop2,
                  (loop - loop2)).toInt();
              runwayType[runLength - 1] = line.mid(++loop,1).toInt();
            }
          else if((line.mid(0,1) >= "0") && (line.mid(0,1) <= "9"))
            {
              posLength++;
              tA.resize(posLength);

              unsigned int loop;
              for(loop = 0; loop < strlen(line); loop++)
                  if(line.mid(loop, 1) == " ") break;

              lat_temp = __degreeToNum(line.left(loop));
              lon_temp = __degreeToNum(line.mid((loop + 1),100));
              tA.setPoint(posLength - 1,
                  _globalMapMatrix.wgsToMap(lat_temp, lon_temp));
            }
          else if(line.mid(0,3) == "VDF")
              vdf = line.mid(4,1).toUInt();
          else if(line.mid(0,5) == "WINCH")
              winch = line.mid(6,1).toUInt();
          else if(line.mid(0,8) == "WAYPOINT")
              isWayP = line.mid(9,1).toUInt();
          else if(line.mid(0,5) == "[END]")
            {
              switch (type)
                {
                  case BaseMapElement::IntAirport:
                    break;
                  case BaseMapElement::Airport:
                  case BaseMapElement::MilAirport:
                  case BaseMapElement::CivMilAirport:
                  case BaseMapElement::Airfield:
                    airportList.append(new Airport(name, alias, abbr, type,
                        position, elev, frequency, vdf, isWayP));
                    break;
                  case BaseMapElement::ClosedAirfield:
                    airportList.append(new Airport(name, 0, abbr, type,
                        position, 0, 0, 0, isWayP));
                    break;
                  case BaseMapElement::CivHeliport:
                  case BaseMapElement::MilHeliport:
                  case BaseMapElement::AmbHeliport:
                    airportList.append(new Airport(name, alias, abbr, type,
                        position, elev, frequency, 0, isWayP));
                    break;
                  case BaseMapElement::Glidersite:
                    // Wieso k�nnen hier keine Startbahn-Daten angegeben werden?
                    gliderList.append(new GliderSite(name, abbr, position,
                        elev, frequency, winch, isWayP));
                    break;
                  case BaseMapElement::UltraLight:
                  case BaseMapElement::HangGlider:
                  case BaseMapElement::Parachute:
                  case BaseMapElement::Ballon:
                    break;
                  case BaseMapElement::Outlanding:
                    break;
                  case BaseMapElement::VOR:
                  case BaseMapElement::VORDME:
                  case BaseMapElement::VORTAC:
                  case BaseMapElement::NDB:
                    navList.append(new RadioPoint(name, abbr, type, position,
                        frequency, alias));
                    break;
                  case BaseMapElement::AirC:
                  case BaseMapElement::AirCtemp:
                  case BaseMapElement::AirD:
                  case BaseMapElement::AirDtemp:
                  case BaseMapElement::ControlD:
                  case BaseMapElement::AirElow:
                  case BaseMapElement::AirEhigh:
                  case BaseMapElement::AirF:
                  case BaseMapElement::Restricted:
                  case BaseMapElement::Danger:
                  case BaseMapElement::LowFlight:
                    newAir = new Airspace(name, type, tA);
                    newAir->setValues(ulimit, ulimitType, llimit, llimitType);
                    airspaceList.append(newAir);
                    break;
                  case BaseMapElement::Obstacle:
                  case BaseMapElement::LightObstacle:
                  case BaseMapElement::ObstacleGroup:
                  case BaseMapElement::LightObstacleGroup:
                    obstacleList.append(new ElevPoint(0, 0, type, position,
                        elev, isWayP));
                    break;
                  case BaseMapElement::CompPoint:
//                    reportList.append(
//                        new SinglePoint(name, abbr, type, position, isWayP));
                    break;
                  case BaseMapElement::HugeCity:
                  case BaseMapElement::BigCity:
                  case BaseMapElement::MidCity:
                  case BaseMapElement::SmallCity:
                    cityList.append(new LineElement(name, type, tA, sortID));
                    break;
                  case BaseMapElement::Oiltank:
                  case BaseMapElement::Factory:
                  case BaseMapElement::Castle:
                  case BaseMapElement::Church:
                  case BaseMapElement::Tower:
                    break;
                  case BaseMapElement::Highway:
                    highwayList.append(new LineElement(name, type, tA));
                    break;
                  case BaseMapElement::HighwayEntry:
                    break;
                  case BaseMapElement::MidRoad:
                  case BaseMapElement::SmallRoad:
                    roadList.append(new LineElement(name, type, tA));
                    break;
                  case BaseMapElement::RoadBridge:
                  case BaseMapElement::RoadTunnel:
                    break;
                  case BaseMapElement::Railway:
                    railList.append(new LineElement(name, type, tA));
                    break;
                  case BaseMapElement::RailwayBridge:
                  case BaseMapElement::Station:
                    break;
                  case BaseMapElement::AerialRailway:
                    newLine = new LineElement(name, type, tA);
                    railList.append(new LineElement(name, type, tA));
                    break;
                  case BaseMapElement::Coast:
                  case BaseMapElement::BigLake:
                  case BaseMapElement::MidLake:
                  case BaseMapElement::SmallLake:
                  case BaseMapElement::BigRiver:
                  case BaseMapElement::MidRiver:
                  case BaseMapElement::SmallRiver:
                    hydroList.append(new LineElement(name, type, tA, sortID));
                    break;
                  case BaseMapElement::Dam:
                  case BaseMapElement::Lock:
                    break;
                  case BaseMapElement::Spot:
                  case BaseMapElement::Pass:
                    obstacleList.append(new ElevPoint(0, 0, type, position,
                        elev, isWayP));
                    break;
                  case BaseMapElement::Glacier:
                    topoList.append(new LineElement(name, type, tA));
                    break;
                  case BaseMapElement::Isohypse:
                    break;
                  default:
                    break;
                }
              isObject = false;
            }
          else
            {
              warning("KFLog: Unknown field detected: \"%s\"",
                (const char*)line);
            }
        }
    }

  file.close();

  debug("KFLog: %d mapobjects found", objectCount);

  return true;
}

bool MapContents::__readBinaryFile(const char* fileName)
{
  extern const MapMatrix _globalMapMatrix;

  KStandardDirs* globalDirs = KGlobal::dirs();
  QString pathName;
  pathName.sprintf("kflog/mapdata/%s", fileName);
  pathName = globalDirs->findResource("data", pathName);
  QFile eingabe(pathName);
  if(!eingabe.open(IO_ReadOnly))
    {
      warning("KFLog: Can not open mapfile %s", (const char*)pathName);
      return false;
    }

  QDataStream in(&eingabe);
  /******************************************************************
   *                                                                *
   *  Version auf altes Qt setzen. Auf diese Weise k�nnen die alten *
   *  H�henlinien gelesen werden.                                   *
   *                                                                *
   *  Kann auf Dauer aber so nicht bleiben! Wir m�ssen die Dateien  *
   *  neu erzeugen!                                                 *
   *                                                                *
   ******************************************************************/
  in.setVersion(1);

  Q_UINT8 typeIn, llimitType, ulimitType, vdf, winch,
          rwNum, rwdir, rwsur, isW, isValley;
  Q_UINT16 elev, llimit, ulimit, length, rwlength, secNumber;
  Q_UINT32 isoLength;

  char* name;
  char* frequency;

  QString alias, abbr, header;

  Q_INT32 at_lat, at_lon, lat_temp, lon_temp, latA, latB, lonA, lonB;

  unsigned int type = BaseMapElement::NotSelected, countObject = 0;

  struct runway* rwData;
  struct intrunway * irwData;

  in >> header;

  if(header != MAP_FILE_FORMAT && header != DEM_FILE_FORMAT)
    {
      // falsches Kartenformat !!!
      warning("KFLog: Trying to open old map-file; aborting!");
      warning(pathName);
      warning(header);
      return false;
    }

  in >> secNumber;

  int anzahl = 0;

  if(header == DEM_FILE_FORMAT)
    {
      while(!in.eof())
        {
          int sort_temp;

          Q_UINT8 type;
          Q_INT16 elevation;
          Q_INT8 valley, sort;
          Q_INT32 locLength, latList_temp, lonList_temp;

          in >> type;
          in >> elevation;
          in >> valley;
          in >> sort;
          in >> locLength;

          QPointArray tA(locLength);

          for(int i = 0; i < locLength; i++)
            {
              in >> latList_temp;
              in >> lonList_temp;

              tA.setPoint(i, _globalMapMatrix.wgsToMap(latList_temp,
                  lonList_temp));
            }
          /*
           * �nderungen zwischen den Formaten:
           *
           * -> In den alten Dateien hat der Wert "valley" genau die
           *    falsche Bedeutung ...
           *
           */
          sort_temp = (int)sort;

          for(unsigned int pos = 0; pos < ISO_LINE_NUM; pos++)
              if(isoLines[pos] == elevation)
                  sort_temp = ISO_LINE_NUM * sort_temp + pos;

          Isohypse* newItem = new Isohypse(tA, elevation, !valley, sort_temp);
          isoList.at(newItem->sortID())->append(newItem);
        }
    }
  else if(header == MAP_FILE_FORMAT)
    {
    // Einlesen einer "normalen" Karte in dieser Form noch nicht implementiert
    }

  QTime readT;
  readT.restart();

return true;

  Airspace* newAir;
  QPoint position;

  while(!in.eof()) {
    anzahl++;
    name = 0;
    in >> typeIn;
    type = typeIn;
    length = 0;
    countObject++;
    winch = 0;
    abbr = "";
    isValley = 0;

    QPointArray tA;

    switch (type) {
      case BaseMapElement::IntAirport:
        in >> name;
        in >> alias;
        in >> elev;
        in >> frequency;
        in >> vdf;
        in >> rwNum;
        irwData = new intrunway[rwNum];
        for(unsigned int i = 0; i < rwNum; i++) {
          in >> rwdir;
          in >> rwlength;
          in >> rwsur;
          in >> latA;
          in >> lonA;
          in >> latB;
          in >> lonB;

          irwData[i].direction = rwdir;
          irwData[i].length = rwlength;
          irwData[i].surface = rwsur;
          irwData[i].latitudeA = latA;
          irwData[i].longitudeA = lonA;
          irwData[i].latitudeB = latB;
          irwData[i].longitudeB = lonB;
        }
        break;
      case BaseMapElement::Airport:
      case BaseMapElement::MilAirport:
      case BaseMapElement::CivMilAirport:
      case BaseMapElement::Airfield:
        in >> name;
        in >> abbr;
        in >> alias;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        in >> elev;
        in >> frequency;
        in >> vdf;
        in >> rwNum;
        rwData = new runway[rwNum];
        for(unsigned int i = 0; i < rwNum; i++)
          {
            in >> rwdir;
            in >> rwlength;
            in >> rwsur;

            rwData[i].direction = rwdir;
            rwData[i].length = rwlength;
            rwData[i].surface = rwsur;
          }
//        airportList.append(new Airport(name, alias, abbr, type, position, elev,
//            frequency, vdf));
        break;
      case BaseMapElement::ClosedAirfield:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
//        airportList.append(new Airport(name, 0, abbr, type, position, 0, 0, 0));
        break;
      case BaseMapElement::CivHeliport:
      case BaseMapElement::MilHeliport:
      case BaseMapElement::AmbHeliport:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        in >> elev;
        in >> frequency;
//        airportList(new Airport(name, alias, abbr, type, at_lat, at_lon, elev,
//                          frequency));
        break;
      case BaseMapElement::Glidersite:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> elev;
        in >> frequency;
        in >> winch;
        in >> isW;
        in >> rwNum;
        rwData = new runway[rwNum];
        for(unsigned int i = 0; i < rwNum; i++)
          {
            in >> rwdir;
            in >> rwlength;
            in >> rwsur;

            rwData[i].direction = rwdir;
            rwData[i].length = rwlength;
            rwData[i].surface = rwsur;
          }
//        gliderList.append(new GliderSite(name, abbr, at_lat, at_lon, elev,
//                          frequency, winch, isW, rwData, rwNum));
        break;
      case BaseMapElement::UltraLight:
      case BaseMapElement::HangGlider:
      case BaseMapElement::Parachute:
      case BaseMapElement::Ballon:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
//        addSitesList.append(new SinglePoint(name, abbr, type, at_lat, at_lon));
        break;
      case BaseMapElement::Outlanding:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        in >> elev;
//        outList.append(new ElevPoint(name, abbr, type, at_lat, at_lon, elev));
        break;
      case BaseMapElement::VOR:
      case BaseMapElement::VORDME:
      case BaseMapElement::VORTAC:
      case BaseMapElement::NDB:
        in >> name;
        in >> abbr;
        in >> alias;
        in >> at_lat;
        in >> at_lon;
        in >> frequency;
//        navList.append(new RadioPoint(name, abbr, type, at_lat, at_lon,
//                          frequency, alias));
        break;
      case BaseMapElement::AirC:
      case BaseMapElement::AirCtemp:
      case BaseMapElement::AirD:
      case BaseMapElement::AirDtemp:
        in >> name;
        in >> llimit;
        in >> llimitType;
        in >> ulimit;
        in >> ulimitType;
        in >> length;

        READ_POINT_LIST

        newAir = new Airspace(name, type, tA);
        newAir->setValues(ulimit, ulimitType, llimit, llimitType);
        airspaceList.append(newAir);
        break;
      case BaseMapElement::ControlD:
        in >> name;
        in >> ulimit;
        in >> ulimitType;
        in >> length;

        READ_POINT_LIST

        newAir = new Airspace(name, type, tA);
        newAir->setValues(ulimit, ulimitType, llimit, llimitType);
        airspaceList.append(newAir);
        break;
      case BaseMapElement::AirElow:
      case BaseMapElement::AirEhigh:
        in >> name;
        in >> length;

        READ_POINT_LIST

        newAir = new Airspace(name, type, tA);
        newAir->setValues(ulimit, ulimitType, llimit, llimitType);
        airspaceList.append(newAir);
        break;
      case BaseMapElement::AirF:
      case BaseMapElement::Restricted:
      case BaseMapElement::Danger:
        in >> name;
        in >> llimit;
        in >> llimitType;
        in >> ulimit;
        in >> ulimitType;
        in >> length;

        READ_POINT_LIST

        newAir = new Airspace(name, type, tA);
        newAir->setValues(ulimit, ulimitType, llimit, llimitType);
        airspaceList.append(newAir);
        break;
      case BaseMapElement::LowFlight:
        in >> name;
        in >> length;

        READ_POINT_LIST

        newAir = new Airspace(name, type, tA);
        newAir->setValues(ulimit, ulimitType, llimit, llimitType);
        airspaceList.append(newAir);
        break;
      case BaseMapElement::Obstacle:
      case BaseMapElement::LightObstacle:
      case BaseMapElement::ObstacleGroup:
      case BaseMapElement::LightObstacleGroup:
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        in >> elev;
        obstacleList.append(new ElevPoint(name, abbr, type, position, elev));
        break;
      case BaseMapElement::CompPoint:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        reportList.append(new SinglePoint(name, abbr, type, position));
        break;
      case BaseMapElement::HugeCity:
      case BaseMapElement::BigCity:
      case BaseMapElement::MidCity:
      case BaseMapElement::SmallCity:
        in >> name;
        in >> length;

        READ_POINT_LIST

        cityList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::Oiltank:
      case BaseMapElement::Factory:
      case BaseMapElement::Castle:
      case BaseMapElement::Church:
      case BaseMapElement::Tower:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        landmarkList.append(new SinglePoint(name, abbr, type, position));
        break;
      case BaseMapElement::Highway:
        in >> length;

        READ_POINT_LIST

        highwayList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::HighwayEntry:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        highEntryList.append(new SinglePoint(name, abbr, type, position));
        break;
      case BaseMapElement::MidRoad:
      case BaseMapElement::SmallRoad:
        in >> length;

        READ_POINT_LIST

        roadList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::RoadBridge:
      case BaseMapElement::RoadTunnel:
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        // Bislang falsche Liste (Eintrag wird ignoriert) !!!
        break;
      case BaseMapElement::Railway:
        in >> length;

        READ_POINT_LIST

        railList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::RailwayBridge:
      case BaseMapElement::Station:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        stationList.append(new SinglePoint(name, abbr, type, position));
        break;
      case BaseMapElement::AerialRailway:
        in >> length;

        READ_POINT_LIST

        railList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::Coast:
        in >> length;

        READ_POINT_LIST

        hydroList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::BigLake:
      case BaseMapElement::MidLake:
      case BaseMapElement::SmallLake:
        in >> name; /* weiter mit L�nge */
      case BaseMapElement::BigRiver:
      case BaseMapElement::MidRiver:
      case BaseMapElement::SmallRiver:
        in >> length;

        READ_POINT_LIST

        hydroList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::Dam:
      case BaseMapElement::Lock:
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        break;
      case BaseMapElement::Spot:
      case BaseMapElement::Pass:
        in >> name;
        in >> abbr;
        in >> at_lat;
        in >> at_lon;
        in >> isW;
        in >> elev;
        obstacleList.append(new ElevPoint(name, abbr, type, position, elev));
        break;
      case BaseMapElement::Glacier:
        in >> length;

        READ_POINT_LIST

        topoList.append(new LineElement(name, type, tA));
        break;
      case BaseMapElement::Isohypse:
        in >> elev;
        in >> isValley;
        in >> isoLength;

        READ_POINT_LIST

        if(isoLength >= 3)
          {
//          addElement(new Isohypse(isoLength, latList, lonList, elev,
//              border, isValley, 0));
          }
        break;
    }
  }
  eingabe.close();

  warning("KFlog: %d mapelements found in binary-file %s", anzahl, fileName);
  warning("Dauer: %d", readT.restart());

  return true;
}

int MapContents::searchFlightPoint(QPoint cPos, struct flightPoint* fP)
{
  if(flightList.count())
      return (flightList.current()->searchPoint(cPos, fP));

  return -1;
}

Flight* MapContents::getFlight()
{
  if(flightList.count())
      return flightList.current();

  return 0;
}

bool MapContents::loadFlight(QFile igcFile)
{
  warning("MapContents::loadFlight(%s)", (const char*)igcFile.name());

  QFileInfo fInfo(igcFile);
  if(!fInfo.exists())
    {
      KMessageBox::error(0, i18n("The selected file") + "<BR><B>"
          + igcFile.name() + "</B><BR>" + i18n("does not exist!"));
      return false;
    }
  if(!fInfo.size())
    {
      KMessageBox::sorry(0, i18n("The selected file") + "<BR><B>"
          + igcFile.name() + "</B><BR>" + i18n("is empty!"));
      return false;
    }
  /*
   * Wir brauchen eine bessere Formatpr�fung als nur die
   * �berpr�fung der Endung und der Gr��e ...
   */
  if((fInfo.extension() != "igc") && (fInfo.extension() != "IGC"))
    {
      KMessageBox::error(0, i18n("The selected file") + "<BR><B>"
          + igcFile.name() + "</B><BR>" + i18n("is not an igc-file!"));
      return false;
    }

  if(!igcFile.open(IO_ReadOnly))
    {
      KMessageBox::error(0, i18n("You don't have permission to access file")
          + "<BR><B>" + igcFile.name() + "</B>", i18n("No permission"));
      return false;
    }

  QProgressDialog importProgress(0,0,true);

  importProgress.setCaption(i18n("Import mapfile ..."));
  importProgress.setLabelText((QString)i18n("Please wait while loading file")
          + "<BR><B>" + igcFile.name() + "</B>");
  importProgress.setMinimumWidth(importProgress.sizeHint().width() + 45);
  importProgress.setTotalSteps(200);
  importProgress.show();
  importProgress.setMinimumDuration(0);

  importProgress.setProgress(0);

  unsigned int fileLength = fInfo.size();
  unsigned int filePos = 0;
  QString s;
  QTextStream stream(&igcFile);

  QString pilotName, gliderType, gliderID, date;
  char latChar, lonChar;
  bool launched = false, append = true, isFirst = true, isFirstWP = true;
  int dT, lat, latmin, latTemp, lon, lonmin, lonTemp;
  int hh = 0, mm = 0, ss = 0, curTime = 0, preTime = 0;

  float v, speed;

  struct flightPoint newPoint;
  struct flightPoint prePoint;
  QList<flightPoint> flightRoute;
  QList<struct wayPoint> wpList;
  struct wayPoint* newWP;
  struct wayPoint* preWP;

  extern const MapMatrix _globalMapMatrix;

  while (!stream.eof())
    {
      if(importProgress.wasCancelled()) return false;

      s = stream.readLine();
      filePos += s.length();
      importProgress.setProgress(( filePos * 200 ) / fileLength);

      if(s.mid(0,1) == "H")
        {
          // We have a headerline
          if(s.mid(5,5) == "PILOT")
              pilotName = s.mid(11,100);
          else if(s.mid(5,10) == "GLIDERTYPE")
              gliderType = s.mid(16,100);
          else if(s.mid(5,8) == "GLIDERID")
              gliderID = s.mid(14,100);
          else if(s.mid(1,4) == "FDTE")
              // Hier bislang nur "deutsches" Format.
              date = s.mid(5,2) + "." + s.mid(7,2) + "." + s.mid(9,2);
        }
      else if(s.mid(0,1) == "B")
        {
          // We have a point
          sscanf(s.mid(1,23), "%2d%2d%2d%2d%5d%1c%3d%5d%1c",
              &hh, &mm, &ss, &lat, &latmin, &latChar, &lon, &lonmin, &lonChar);
          latTemp = lat * 600000 + latmin * 10;
          lonTemp = lon * 600000 + lonmin * 10;

          if(latChar == 'S') latTemp = -latTemp;
          if(lonChar == 'W') lonTemp = -lonTemp;

          curTime = 3600 * hh + 60 * mm + ss;

          newPoint.time = curTime;
          newPoint.origP = QPoint(latTemp, lonTemp);
          newPoint.projP = _globalMapMatrix.wgsToMap(newPoint.origP);

          sscanf(s.mid(25,10),"%5d%5d", &newPoint.height, &newPoint.gpsHeight);

          if(isFirst)
            {
              prePoint = newPoint;
              preTime = curTime;
              isFirst = false;
              newPoint.dS = 0;
              newPoint.dH = 0;
              newPoint.dT = 0;
              speed = 0;
              v = 0;

              continue;
            }

          /* dtime may change, even if the intervall, in wich the
           * logger gets the position, is allways the same. If the
           * intervall is f.e. 10 sec, dtime may change to 11 or 9 sec.
           */
          dT = curTime - preTime;
          newPoint.dT = dT;
          newPoint.dH = newPoint.height - prePoint.height;
          newPoint.dS = (int)(dist(latTemp, lonTemp,
              prePoint.origP.x(), prePoint.origP.y()) * 1000.0);

          speed = 3600 * newPoint.dS / dT;  // [km/h]
          v = newPoint.dH / dT * 1.0;       // [m/s]

          if(launched)
            {
              flightRoute.append(new flightPoint);
              *(flightRoute.last()) = newPoint;

              if(!append)
                {
                  if( ( speed > 10 ) && ( ( v > 0.5 ) || ( v < -0.5 ) ) )
                      append = true;
                  else
                      /* We are realy back on the ground, again.
                       * Now we can stop reading the file!           */
                      break;
                }
              /*
               * Die Landebedingungen sind, besonders bei einem gro�en
               * Zeitabstand der Messungen noch nicht korrekt!
               *
               * Bedingung sollte �ber mehrere Punkte gehen. Ausserdem
               * eventuell anhand dS und dH erfolgen.
               *
               */
              if( ( speed < 10 ) && ( ( v < 0.5 ) && ( v > -0.5 ) ) )
                  /* We might be back on the ground, again. But
                   * we wait for the next point.                     */
                  append = false;
            }
          else
            {
              if( ( speed > 20 ) && ( v > 1.5 ) )
                {
                  launched = true;
                  flightRoute.append(new flightPoint);
                  *(flightRoute.last()) = prePoint;

                  flightRoute.append(new flightPoint);
                  *(flightRoute.last()) = newPoint;
                }
            }
          prePoint = newPoint;
          preTime = curTime;
        }
      else if(s.mid(0,1) == "C")
        {
          if( ( ( ( s.mid( 8,1) == "N" ) || ( s.mid( 8,1) == "S" ) ) ||
                ( ( s.mid(17,1) == "W" ) || ( s.mid(17,1) == "E" ) ) ) &&
              ( s.mid(18,20) != 0 ) )
            {
              // We have a waypoint
              sscanf(s.mid(1,17), "%2d%5d%1c%3d%5d%1c",
                  &lat, &latmin, &latChar, &lon, &lonmin, &lonChar);
              latTemp = lat * 600000 + latmin * 10;
              lonTemp = lon * 600000 + lonmin * 10;

              if(latChar == 'S') latTemp = -latTemp;
              if(lonChar == 'W') lonTemp = -lonTemp;

              newWP = new wayPoint;
              newWP->name = s.mid(18,20);
              newWP->origP = QPoint(latTemp, lonTemp);
              newWP->projP = _globalMapMatrix.wgsToMap(newWP->origP);
              newWP->sector1 = 0;
              newWP->sector2 = 0;
              newWP->sectorFAI = 0;
              newWP->angle = -100;
              newWP->type = Flight::NotSet;
              if(isFirstWP)
                  newWP->distance = 0;
              else
                  newWP->distance = dist(latTemp, lonTemp,
                      preWP->origP.y(), preWP->origP.x());

              if(!isFirstWP && newWP->distance <= 0.1)  continue;

              wpList.append(newWP);
              isFirstWP = false;
              preWP = newWP;
            }
        }
      else if(s.mid(0,1) == "L")
        {
          if(s.mid(13,16) == "TAKEOFF DETECTED")
            {
              // Der Logger hat den Start erkannt !
              launched = true;
            }
        }
    }

  flightList.append(new Flight(flightRoute, pilotName, gliderType,
      gliderID, wpList, date));

  return true;
}

void MapContents::proofeSection()
{
  extern const MapMatrix _globalMapMatrix;
  const QRect mapBorder = _globalMapMatrix.getViewBorder();

  int westCorner = ( ( mapBorder.left() / 600000 / 2 ) * 2 + 180 ) / 2;
  int eastCorner = ( ( mapBorder.right() / 600000 / 2 ) * 2 + 180 ) / 2;
  int northCorner = ( ( mapBorder.top() / 600000 / 2 ) * 2 - 88 ) / -2;
  int southCorner = ( ( mapBorder.bottom() / 600000 / 2 ) * 2 - 88 ) / -2;

  if(mapBorder.left() < 0)  westCorner -= 1;
  if(mapBorder.right() < 0)  eastCorner -= 1;
  if(mapBorder.top() < 0) northCorner -= 1;
  if(mapBorder.bottom() < 0) southCorner -= 1;

  if(isFirst)
    {
      __readAsciiFile("/home/heiner/Entwicklung/import/luftraume.out");
      __readAsciiFile("/home/heiner/Entwicklung/KFLog_Daten/karte/kflog_sites.out");
      isFirst = false;
    }

  for(int row = northCorner; row <= southCorner; row++) {
    for(int col = westCorner; col <= eastCorner; col++) {
      if( sectionArray.testBit( row + ( col + ( row * 89 ) ) ) ) {
        // Kachel ist geladen!
      } else {
        // Kachel fehlt!
        int latID = (int)row * -2 + 88;
        int lonID = (int)col * 2 - 180;
        QString latID_S, lonID_S;
        if(latID < 0) {
          latID_S.sprintf("%dS", -latID);
        } else {
          latID_S.sprintf("%dN", latID);
        }
        if(lonID < 0) {
          lonID_S.sprintf("%dW", -lonID);
        } else {
          lonID_S.sprintf("%dE", lonID);
        }
        /* H�hendaten (Digital Elevation Model): */
        QString demSecName = latID_S + "_" + lonID_S + "_dem.wld";
        /* Nullmeterlinie */
        QString dem0SecName = latID_S + "_" + lonID_S + "_0_dem.wld";
         /* �brige Kartendaten: */
        QString mapSecName = latID_S + "_" + lonID_S + "_map.wld";
        QString asciiName =
            "/data/KartenDaten/KFLog-Karten/Staaten_Kachel/" + latID_S
            + "_" + lonID_S + ".out";
        QString cityName =
            "/data/KartenDaten/KFLog-Karten/St�dte/" + latID_S
            + "_" + lonID_S + ".out";
        __readAsciiFile(cityName);

           /* Nun m�ssen die korrekten Dateien geladen werden ... */
        __readBinaryFile(demSecName);
        __readBinaryFile(dem0SecName);
//        if(__readAsciiIsoFile(asciiName))
//            __readAsciiIsoFile(asciiName);
          {
            asciiName = "/data/KartenDaten/KFLog-Karten/LinienKacheln/" +
            latID_S + "_" + lonID_S + ".roads.out";
            __readAsciiFile(asciiName);

            asciiName = "/data/KartenDaten/KFLog-Karten/LinienKacheln/" +
              latID_S + "_" + lonID_S + ".railway.out";
            __readAsciiFile(asciiName);

           asciiName = "/data/KartenDaten/KFLog-Karten/LinienKacheln/" +
              latID_S  + "_" + lonID_S + ".river.out";
           __readAsciiFile(asciiName);

           asciiName = "/data/KartenDaten/KFLog-Karten/Seen_Kachel/europa_seen." +
              latID_S  + "_" + lonID_S + "_kor.dnnet.out";
           __readAsciiFile(asciiName);

//            warning("    Kachel geladen: %s", (const char*)demSecName);
            sectionArray.setBit( row + ( col + ( row * 89 ) ), true );
          }
//        else
//          warning("KACHEL FEHLT: %s", (const char*)demSecName);
      }
    }
  }

}

unsigned int MapContents::getListLength(int listIndex) const
{
  switch(listIndex) {
//    case IntAirportList:
//      return intairportList.count();
    case AirportList:
      return airportList.count();
    case GliderList:
      return gliderList.count();
    case OutList:
      return outList.count();
    case NavList:
      return navList.count();
    case AirspaceList:
      warning("MapContents::getListLength(%d) -> AirspaceList", listIndex);
      return airspaceList.count();
    case ObstacleList:
      return obstacleList.count();
    case ReportList:
      return reportList.count();
    case CityList:
      return cityList.count();
//    case VillageList:
//      return villageList.count();
    case LandmarkList:
      return landmarkList.count();
    case HighwayList:
      return highwayList.count();
    case HighwayEntryList:
      return highEntryList.count();
    case RoadList:
      return roadList.count();
    case RailList:
      return railList.count();
    case StationList:
      return stationList.count();
    case HydroList:
      return hydroList.count();
    case TopoList:
      return topoList.count();
//    case IsohypseList:
//      //warning("Anzahl der H�henlinien: %d", isohypseList.count());
//      return isohypseList.count();
    default:
      return 0;
  }
}

Airspace* MapContents::getAirspace(unsigned int index)
{
  return airspaceList.at(index);
}

Airport* MapContents::getAirport(unsigned int index)
{
  return airportList.at(index);
}

GliderSite* MapContents::getGlidersite(unsigned int index)
{
  return gliderList.at(index);
}

BaseMapElement* MapContents::getElement(int listIndex, unsigned int index)
{
  switch(listIndex) {
//    case IntAirportList:
//      return intairportList.at(index);
    case AirportList:
      return airportList.at(index);
    case GliderList:
      return gliderList.at(index);
    case OutList:
      return outList.at(index);
    case NavList:
      return navList.at(index);
    case AirspaceList:
      return airspaceList.at(index);
    case ObstacleList:
      return obstacleList.at(index);
    case ReportList:
      return reportList.at(index);
    case CityList:
      return cityList.at(index);
//    case VillageList:
//      return villageList.at(index);
    case LandmarkList:
      return landmarkList.at(index);
    case HighwayList:
      return highwayList.at(index);
    case HighwayEntryList:
      return highEntryList.at(index);
    case RoadList:
      return roadList.at(index);
    case RailList:
      return railList.at(index);
    case StationList:
      return stationList.at(index);
    case HydroList:
      return hydroList.at(index);
    case TopoList:
      return topoList.at(index);
    default:
      // Should never happen!
      fatal("KFLog: trying to acces unknown mapelementlist");
      return 0;
  }
}

SinglePoint* MapContents::getSinglePoint(int listIndex, unsigned int index)
{
  switch(listIndex) {
    case AirportList:
      return airportList.at(index);
    case GliderList:
      return gliderList.at(index);
    case OutList:
      return outList.at(index);
    case NavList:
      return navList.at(index);
    case ObstacleList:
      return obstacleList.at(index);
    case ReportList:
      return reportList.at(index);
//    case VillageList:
//      return villageList.at(index);
    case LandmarkList:
      return landmarkList.at(index);
    case HighwayEntryList:
      return highEntryList.at(index);
    case StationList:
      return stationList.at(index);
    default:
      return 0;
  }
}

void MapContents::printList(QPainter* targetPainter, unsigned int listID)
{
  switch(listID)
    {
      case AirportList:
        for(unsigned int loop = 0; loop < airportList.count(); loop++)
            airportList.at(loop)->printMapElement(targetPainter);
        break;
      case GliderList:
        for(unsigned int loop = 0; loop < gliderList.count(); loop++)
            gliderList.at(loop)->printMapElement(targetPainter);
        break;
      case OutList:
        for(unsigned int loop = 0; loop < outList.count(); loop++)
            outList.at(loop)->printMapElement(targetPainter);
        break;
      case NavList:
        for(unsigned int loop = 0; loop < navList.count(); loop++)
            navList.at(loop)->printMapElement(targetPainter);
        break;
      case AirspaceList:
        for(unsigned int loop = 0; loop < airspaceList.count(); loop++)
            airspaceList.at(loop)->printMapElement(targetPainter);
        break;
      case ObstacleList:
        for(unsigned int loop = 0; loop < obstacleList.count(); loop++)
            obstacleList.at(loop)->printMapElement(targetPainter);
        break;
      case ReportList:
        for(unsigned int loop = 0; loop < reportList.count(); loop++)
            reportList.at(loop)->printMapElement(targetPainter);
        break;
      case CityList:
        for(unsigned int loop = 0; loop < cityList.count(); loop++)
            cityList.at(loop)->printMapElement(targetPainter);
        break;
//      case VillageList:
//        for(unsigned int loop = 0; loop < villageList.count(); loop++)
//            villageList.at(loop)->drawMapElement(targetPainter);
//        break;
      case LandmarkList:
        for(unsigned int loop = 0; loop < landmarkList.count(); loop++)
            landmarkList.at(loop)->printMapElement(targetPainter);
        break;
      case HighwayList:
        for(unsigned int loop = 0; loop < highwayList.count(); loop++)
            highwayList.at(loop)->printMapElement(targetPainter);
        break;
      case RoadList:
        for(unsigned int loop = 0; loop < roadList.count(); loop++)
            roadList.at(loop)->printMapElement(targetPainter);
        break;
      case RailList:
        for(unsigned int loop = 0; loop < railList.count(); loop++)
            railList.at(loop)->printMapElement(targetPainter);
        break;
      case HydroList:
        for(unsigned int loop = 0; loop < hydroList.count(); loop++)
            hydroList.at(loop)->printMapElement(targetPainter);
        break;
      case TopoList:
        for(unsigned int loop = 0; loop < topoList.count(); loop++)
            topoList.at(loop)->printMapElement(targetPainter);
        break;
      case FlightList:
        for(unsigned int loop = 0; loop < flightList.count(); loop++)
            flightList.at(loop)->printMapElement(targetPainter);
        break;
      default:
        return;
    }
}

void MapContents::drawList(QPainter* targetPainter, QPainter* maskPainter,
    unsigned int listID)
{
warning("MapContents::drawList(%d)", listID);
  switch(listID)
    {
      case AirportList:
        warning("AirportList");
        for(unsigned int loop = 0; loop < airportList.count(); loop++)
            airportList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case GliderList:
        warning("GliderList");
        for(unsigned int loop = 0; loop < gliderList.count(); loop++)
            gliderList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case OutList:
        warning("OutList");
        for(unsigned int loop = 0; loop < outList.count(); loop++)
            outList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case NavList:
        warning("NavList");
        for(unsigned int loop = 0; loop < navList.count(); loop++)
            navList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case AirspaceList:
        warning("AirspaceList");
        for(unsigned int loop = 0; loop < airspaceList.count(); loop++)
            airspaceList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case ObstacleList:
        warning("ObstacleList");
        for(unsigned int loop = 0; loop < obstacleList.count(); loop++)
            obstacleList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case ReportList:
        warning("ReportList");
        for(unsigned int loop = 0; loop < reportList.count(); loop++)
            reportList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case CityList:
        warning("CityList");
        for(unsigned int loop = 0; loop < cityList.count(); loop++)
            cityList.at(loop)->drawMapElement(targetPainter, maskPainter);
        for(unsigned int loop = 0; loop < cityList.count(); loop++)
            cityList.at(loop)->drawMapElement(targetPainter, maskPainter, false);
        break;
//      case VillageList:
//        for(unsigned int loop = 0; loop < villageList.count(); loop++)
//            villageList.at(loop)->drawMapElement(targetPainter, maskPainter);
//        break;
      case LandmarkList:
        warning("LandmarkList");
        for(unsigned int loop = 0; loop < landmarkList.count(); loop++)
            landmarkList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case HighwayList:
        warning("HighwayList");
        for(unsigned int loop = 0; loop < highwayList.count(); loop++)
            highwayList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case RoadList:
        warning("RoadList");
        for(unsigned int loop = 0; loop < roadList.count(); loop++)
            roadList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case RailList:
        warning("RailList");
        for(unsigned int loop = 0; loop < railList.count(); loop++)
            railList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case HydroList:
        warning("HydroList");
        for(unsigned int loop = 0; loop < hydroList.count(); loop++)
            hydroList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case TopoList:
        warning("TopoList");
        for(unsigned int loop = 0; loop < topoList.count(); loop++)
            topoList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      case FlightList:
        warning("FlightList");
        for(unsigned int loop = 0; loop < flightList.count(); loop++)
            flightList.at(loop)->drawMapElement(targetPainter, maskPainter);
        break;
      default:
        return;
    }
}
void MapContents::__setPainterColor(QPainter* targetPainter, int height)
{
  /* Fr�here Werte:
   *
   *  height <    0 ( 96, 128, 248);
   *  height <   10 (174, 208, 129);
   *  height <   50 (201, 230, 178);
   *  height <  100 (231, 255, 231);
   *  height <  250 (221, 245, 183);
   *  height <  500 (240, 240, 168);
   *  height <  750 (240, 223, 140);
   *  height < 1000 (235, 185, 128);
   *  height < 2000 (235, 155,  98);
   *  height < 3000 (210, 115,  50);
   *  height < 4000 (180,  75,  25);
   *  height > 4000 (130,  65,  20);
   *                 -34
   * Definierte Farbwerte bei (dazwischen linear �ndern):
   *
   *   <0 m :    96 / 128 / 248
   *    0 m :   174 / 208 / 129 *
   *   10 m :   201 / 230 / 178 *
   *   50 m :   231 / 255 / 231 *
   *  100 m :   221 / 245 / 183 *
   *  250 m :   240 / 240 / 168 *
   * 1000 m :   235 / 155 /  98 *
   * 4000 m :   130 /  65 /  20 *
   * 9000 m :    96 /  43 /  16 *
   */
#define H00_R  96
#define H00_G 128
#define H00_B 248

#define H0_R 165
#define H0_G 214
#define H0_B 126

#define H10_R 185
#define H10_G 220
#define H10_B 131

#define H50_R 245
#define H50_G 244
#define H50_B 164

#define H100_R 237
#define H100_G 252
#define H100_B 178

#define H200_R 235
#define H200_G 226
#define H200_B 156

#define H1000_R 235
#define H1000_G 155
#define H1000_B  98

#define H4000_R 130
#define H4000_G  65
#define H4000_B  20

  double rot = 0, gruen = 0, blau = 0;

  if(height < -1)
    {
      rot = H00_R;
      gruen = H00_G;
      blau = H00_B;
    }
  else if(height <= 10)
    {
      rot = H0_R + ( height / 10.0 * (H10_R - H0_R) );
      gruen = H0_G + ( height / 10.0 * (H10_G - H0_G) );
      blau = H0_B + ( height / 10.0 * (H10_B - H0_B) );
    }
/*  else if(height <= 50)
    {
      rot = H10_R + ( ( height - 10 ) / 40.0 * (H100_R - H50_R) );
      gruen = H10_G + ( ( height - 10 ) / 40.0 * (H100_G - H50_G) );
      blau = H10_B + ( ( height - 10 ) / 40.0 * (H100_B - H50_B) );
    }
*/
  else if(height <= 100)
    {
      rot = H10_R + ( ( height - 10 ) / 90.0 * (H100_R - H10_R) );
      gruen = H10_G + ( ( height - 10) / 90.0 * (H100_G - H10_G) );
      blau = H10_B + ( ( height - 10 ) / 90.0 * (H100_B - H10_B) );
    }
/*  else if(height <= 250)
    {
      rot = H100_R + ( ( height - 80 ) / 170.0 * (H200_R - H100_R) );
      gruen = H100_G + ( ( height - 80 ) / 170.0 * (H200_G - H100_G) );
      blau = H100_B + ( ( height - 80 ) / 170.0 * (H200_B - H100_B) );
    }
    */
  else if(height <= 1000)
    {
      rot = H100_R + ( ( height - 100 ) / 900.0 * (H1000_R - H100_R) );
      gruen = H100_G + ( ( height - 100 ) / 900.0 * (H1000_G - H100_G) );
      blau = H100_B + ( ( height - 100 ) / 900.0 * (H1000_B - H100_B) );
    }
  else if(height <= 4000)
    {
      rot = H1000_R + ( ( height - 1000 ) / 3000.0 * (H4000_R - H1000_R) );
      gruen = H1000_G + ( ( height - 1000 ) / 3000.0 * (H4000_G - H1000_G) );
      blau = H1000_B + ( ( height - 1000 ) / 3000.0 * (H4000_B - H1000_B) );
    }
  else if(height <= 9000)
    {
      rot = H4000_R + ( ( height - 4000 ) / 5000.0 * (96 - H4000_R) );
      gruen = H4000_G + ( ( height - 4000 ) / 5000.0 * (43 - H4000_G) );
      blau = H4000_B + ( ( height - 4000 ) / 5000.0 * (16 - H4000_B) );
    }

  targetPainter->setPen(QPen(QColor(rot, gruen, blau), 0));
  targetPainter->setBrush(QBrush(QColor(rot, gruen, blau),
      QBrush::SolidPattern));
}

void MapContents::drawIsoList(QPainter* targetPainter, QPainter* maskPainter)
{
  int height = 0;
  for(unsigned int loop = 0; loop < isoList.count(); loop++)
    {
      if(isoList.at(loop)->count() == 0) continue;
      for(unsigned int pos = 0; pos < ISO_LINE_NUM; pos++)
        {
          if(isoLines[pos] == isoList.at(loop)->getFirst()->getElevation())
            {
              if(isoList.at(loop)->getFirst()->isValley())
                {
                  if(pos) height = isoLines[pos - 1];
                  else height = -1;
                }
              else
                height = isoLines[pos];
            }
        }
      __setPainterColor(targetPainter, height);

      for(unsigned int loop2 = 0; loop2 < isoList.at(loop)->count(); loop2++)
          isoList.at(loop)->at(loop2)->drawMapElement(targetPainter,
              maskPainter);
    }
}
