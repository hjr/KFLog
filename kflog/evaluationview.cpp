/***********************************************************************
**
**   evaluationview.cpp
**
**   This file is part of KFLog.
**
************************************************************************
**
**   Copyright (c):  2000 by Heiner Lamprecht, Florian Ehinger
**                   2011 by Axel Pauli
**
**   This file is distributed under the terms of the General Public
**   License. See the file COPYING for more information.
**
**   $Id$
**
***********************************************************************/

#include <QtGui>

#include "evaluationdialog.h"
#include "evaluationview.h"
#include "flight.h"
#include "mapcalc.h"
#include "resource.h"
#include "mainwindow.h"

extern MainWindow *_mainWindow;

// needs to be the same as in EvaluationFrame!!!
#define X_DISTANCE 100
#define Y_DISTANCE 30
#define COORD_DISTANCE 60

EvaluationView::EvaluationView(QScrollArea* parent, EvaluationDialog* dialog) :
  QWidget(parent),
  cursor1(0),
  cursor2(0),
  cursor_alt(0),
  startTime(0),
  secWidth(5),
  scrollFrame(parent),
  evalDialog(dialog),
  flight(0),
  flightPointPointer(0)
{
  setObjectName( "EvaluationView" );
  setMouseTracking(true);

  QPalette p = palette();
  p.setColor(backgroundRole(), Qt::white);
  setPalette(p);

  mouseB = Qt::NoButton | NotReached;

  preparePointer();

  connect( evalDialog, SIGNAL(showFlightPoint(const FlightPoint*)),
           this, SLOT(slotShowPointer(const FlightPoint*)) );
}

EvaluationView::~EvaluationView()
{
}

QSize EvaluationView::sizeHint()
{
  return scrollFrame->viewport()->size();
}

void EvaluationView::paintEvent( QPaintEvent* )
{
  QPainter painter(this);

  if( ! pixBufferKurve.isNull() )
    {
      painter.drawPixmap( 0, 0, pixBufferKurve );
    }

  if( (!((vario && speed) || (vario && baro) || (baro && speed))) &&
      pixBufferYAxis.isNull() == false )
    {
      // int x = scrollFrame->horizontalScrollBar()->value();

      // There is a problem with the paint update atm. The screen refresh
      // did not work properly.
      painter.drawPixmap( 0, 0, pixBufferYAxis );
    }

  if( ! pixBufferMouse.isNull() )
    {
      // Draws a vertical bar in the flight diagram at the new mouse position.
      painter.drawPixmap( 0, 0, pixBufferMouse );
    }

  // Draws a flight pointer on request.
  if( flight && flightPointPointer && ! pixPointer.isNull() )
    {
      int time = flightPointPointer->time - startTime;

      lastPointerPosition.setX( X_DISTANCE + (time / secWidth) - 6 );
      lastPointerPosition.setY( scrollFrame->viewport()->height() - Y_DISTANCE + 4 );

      painter.drawPixmap( lastPointerPosition.x(), lastPointerPosition.y(), pixPointer );
    }
}

void EvaluationView::mousePressEvent(QMouseEvent* event)
{
  // qDebug() << "EvaluationView::mousePressEvent()";

  int x1 = ( cursor1 - startTime ) / secWidth + X_DISTANCE ;
  int x2 = ( cursor2 - startTime ) / secWidth + X_DISTANCE ;

  if(event->pos().x() < x1 + 5 && event->pos().x() > x1 - 5)
    {
      mouseB = event->button() | Reached;
      leftB = 1;
      cursor_alt = cursor1;
    }
  else if(event->pos().x() < x2 + 5 && event->pos().x() > x2 - 5)
    {
      mouseB = event->button() | Reached;
      leftB = 2;
      cursor_alt = cursor2;
    }
  else
    {
      mouseB = event->button() | NotReached;
      leftB = 0;
    }
}

void EvaluationView::mouseReleaseEvent(QMouseEvent* event)
{
  // qDebug() << "EvaluationView::mouseReleaseEvent()";

  time_t time_alt;
  int cursor = -1;

  if (flight)
    {
      if(mouseB == (Qt::MiddleButton | Reached) ||
         mouseB == (Qt::MiddleButton | NotReached))
        {
          time_alt = cursor1;
          cursor = 1;
        }
    else if(mouseB == (Qt::RightButton | Reached) ||
            mouseB == (Qt::RightButton | NotReached))
      {
        time_alt = cursor2;
        cursor = 2;
      }
    else if(mouseB == (Qt::LeftButton | Reached))
      {
        if(leftB == 1)
          {
            time_alt =  cursor1;
            cursor = 1;
          }
        else
          {
            time_alt = cursor2;
            cursor = 2;
          }
      }

    mouseB = Qt::NoButton | NotReached;

    this->setCursor(Qt::ArrowCursor);

    if(cursor == 1)
      {
        cursor1 = flight->getPointByTime( (time_t)(( event->pos().x() - X_DISTANCE ) * secWidth + startTime)).time;

        if(cursor1 > cursor2)
          {
            cursor1 = cursor2;
          }
      }
    else if(cursor == 2)
      {
        cursor2 =  flight->getPointByTime( (time_t)(( event->pos().x() - X_DISTANCE ) * secWidth + startTime)).time;

        if(cursor1 > cursor2)
          {
            cursor2 = cursor1;
          }
      }
    else
      {
        return;
      }

    flight->setTaskByTimes(cursor1,cursor2);

    evalDialog->updateText(flight->getPointIndexByTime(cursor1),
                           flight->getPointIndexByTime(cursor2), true);

    __draw();

    // Reset mouse cursors, if the new end position is reached.
    pixBufferMouse = QPixmap();
    repaint();
  }
}

void EvaluationView::mouseMoveEvent(QMouseEvent* event)
{
  // qDebug() << "EvaluationView::mouseMoveEvent()";

  if( ! flight )
    {
      return;
    }

  int x1 = (( cursor1 - startTime ) / secWidth) + X_DISTANCE ;
  int x2 = (( cursor2 - startTime ) / secWidth) + X_DISTANCE ;

  if(mouseB == (Qt::NoButton | NotReached))
    {
      if(event->pos().x() < x1 + 5 && event->pos().x() > x1 - 5)
          this->setCursor(Qt::PointingHandCursor);
      else if(event->pos().x() < x2 + 5 && event->pos().x() > x2 - 5)
          this->setCursor(Qt::PointingHandCursor);
      else
        {
          this->setCursor(Qt::ArrowCursor);
          return;
        }
    }
  else if(mouseB != (Qt::LeftButton | NotReached) &&
          mouseB != (Qt::NoButton | Reached))
    {
      time_t cursor = flight->getPointByTime( (time_t)((event->pos().x() - X_DISTANCE ) * secWidth + startTime)).time;

      time_t cursor_1 = cursor1;
      time_t cursor_2 = cursor2;

      int movedCursor = 1;

      if(mouseB == (Qt::MiddleButton | Reached) ||
         mouseB == (Qt::MiddleButton | NotReached))
        {
          cursor_1 = cursor;
        }
      else if(mouseB == (Qt::RightButton | Reached) ||
              mouseB == (Qt::RightButton | NotReached))
        {
          cursor_2 = cursor;
          movedCursor = 0;
        }
      else if(mouseB == (Qt::LeftButton | Reached))
        {
          if(leftB == 1)
            {
              cursor_1 = cursor;
            }
          else
            {
              cursor_2 = cursor;
              movedCursor = 0;
            }
        }

      // don't move cursor1 behind cursor2
      if(cursor_1 > cursor_2)
        {
          return;
        }

      // Draw the mouse cursor at the new position.
      __paintCursor( ( cursor  - startTime ) / secWidth + X_DISTANCE, 1, movedCursor );
      repaint();

      cursor_alt = flight->getPointByTime((time_t)((event->pos().x() - X_DISTANCE ) *
                          secWidth + startTime)).time;

      evalDialog->updateText(flight->getPointIndexByTime(cursor_1),
                             flight->getPointIndexByTime(cursor_2));
    }
}

QPoint EvaluationView::__baroPoint(int durch[], int gn, int i)
{
  int x = ( curTime - startTime ) / secWidth + X_DISTANCE ;

  int gesamt = 0;

  for(int loop = 0; loop < qMin(gn, (i * 2 + 1)); loop++)
    {
      gesamt += durch[loop];
    }

  int y = this->height() - (int)( ( gesamt / qMin(gn, (i * 2 + 1)) ) / scale_h )
                                                  - Y_DISTANCE;

  return QPoint(x, y);
}

QPoint EvaluationView::__speedPoint(float durch[], int gn, int i)
{
  //  = Abstand am Anfang der Kurve
  int x = ( curTime - startTime ) / secWidth + X_DISTANCE ;

  float gesamt = 0;
  /*
   * Jetzt läuft die Schleife immer durch. Wenn i < gn ist, nur bis i, sonst bis gn.
   * Das Ganze beruht auf der Annahme, dass die zur Berechnung nötigen Punkte am Anfang
   * des Arrays stehen. Das klappt am Anfang der Kurve sehr gut, am Ende würde es unter
   * Umständen zu Fehlern führen ...
   */
  for(int loop = 0; loop < qMin(gn, (i * 2 + 1)); loop++)
    {
      gesamt += durch[loop];
    }

  int y = this->height() - (int)( ( gesamt / qMin(gn, (i *  2 + 1)) ) / scale_v ) - Y_DISTANCE;

  return QPoint(x, y);
}

QPoint EvaluationView::__varioPoint(float durch[], int gn, int i)
{
  int x = ( curTime - startTime ) / secWidth + X_DISTANCE ;
  // PRE_GRAPH_DISTANCE = Abstand am Anfang der Kurve

  float gesamt = 0;

  for(int loop = 0; loop < qMin(gn, (i * 2 + 1)); loop++)
    {
      gesamt += durch[loop];
    }

  int y = (this->height() / 2) - (int)( ( gesamt / qMin(gn, (i * 2 + 1)) ) / scale_va );

  return QPoint(x, y);
}

void EvaluationView::__drawCsystem(QPainter* painter)
{
  pixBufferYAxis.fill(Qt::white);

  if( scale_h < 0.0 )
    {
      // Die Schleife unten kann nicht terminieren, wenn scale_h negativ ist!
      return;
    }

  QPainter painterText(&pixBufferYAxis);

  QString text;

  int width = ((landTime - startTime) / secWidth) + (COORD_DISTANCE * 2) ;
  int height = scrollFrame->viewport()->height();

  // coordinate axes
  painter->setPen(QPen(QColor(0,0,0), 1));
  painter->drawLine(COORD_DISTANCE, height - Y_DISTANCE, width, height - Y_DISTANCE);
  painter->drawLine(COORD_DISTANCE, height - Y_DISTANCE, COORD_DISTANCE, Y_DISTANCE);

  // variometer null line
  if(vario)
    {
      painter->setPen(QPen(QColor(255,100,100), 2));
      painter->drawLine(COORD_DISTANCE, height / 2, width, height / 2);
    }

  // time axis
  int time_plus, time_small_plus;

  if(secWidth > 22)
    {
      time_plus = 3600;
      time_small_plus = 1800;
    }
  else if (secWidth > 14)
    {
      time_plus = 1800;
      time_small_plus = 900;
    }
  else if (secWidth > 10)
    {
      time_plus = 900;
      time_small_plus = 300;
    }
  else
    {
      time_plus = 600;
      time_small_plus = 60;
    }

  int time = (((startTime - 1) / time_plus) + 1) * time_plus - startTime;
  int time_small = (((startTime - 1) / time_small_plus) + 1)
                       * time_small_plus - startTime;

  // draw major tick marks and time labels
  while(time / (int)secWidth < width - 2*COORD_DISTANCE)
    {
      painter->setPen(QPen(QColor(0,0,0), 2));
      painter->drawLine(X_DISTANCE + (time / secWidth), height - Y_DISTANCE,
                  X_DISTANCE + (time / secWidth), height - Y_DISTANCE + 10);

      painter->setPen(QPen(QColor(0,0,0), 1));
      // the "true" makes sure the time reads 12:00 and not 12: 0 on the axis:
      text = printTime(startTime + time,true,false);
      painter->drawText(X_DISTANCE + (time / secWidth) - 40,
                         height - 21, 80, 20, Qt::AlignCenter, text);

      time += time_plus;
    }

  // draw minor tick marks (min)
  while(time_small / (int)secWidth < width - 2*COORD_DISTANCE)
    {
      painter->setPen(QPen(QColor(0,0,0), 1));
      painter->drawLine(X_DISTANCE + (time_small / secWidth), height - Y_DISTANCE,
             X_DISTANCE + (time_small / secWidth), height - Y_DISTANCE + 5);

      time_small += time_small_plus;
    }

  // Y axes
  painterText.setPen(QPen(QColor(0,0,0), 1));
  painterText.drawLine(COORD_DISTANCE,height - Y_DISTANCE,
                       COORD_DISTANCE, Y_DISTANCE);

  if(!vario && !speed && baro)
    {
      // Barogramm
      int dh = 100;
      if(scale_h > 10)     dh = 500;
      else if(scale_h > 8) dh = 250;
      else if(scale_h > 3) dh = 200;

      int h = dh;
      painterText.setFont(QFont("helvetica",10));

      while(h / scale_h < height - (Y_DISTANCE * 2))
        {
          painterText.setPen(QPen(QColor(100,100,255), 1));
          text.sprintf("%d m",h);
          painterText.drawText(0,height - (int)( h / scale_h ) - Y_DISTANCE - 10,
                           COORD_DISTANCE - 3,20,Qt::AlignRight | Qt::AlignVCenter,text);


          if(h == 1000 || h == 2000 || h == 3000 || h == 4000 ||
                h == 5000 || h == 6000 || h == 7000 || h == 8000 || h == 9000)
              painter->setPen(QPen(QColor(200,200,255), 2));
          else
              painter->setPen(QPen(QColor(200,200,255), 1));

          painter->drawLine(COORD_DISTANCE - 3,
                      height - (int)( h / scale_h ) - Y_DISTANCE,
                      width - 20,height - (int)( h / scale_h ) - Y_DISTANCE);

          h += dh;
        }
    }
  else if(!speed && !baro && vario)
    {
      // Variogramm
      painter->setPen(QPen(QColor(255,100,100), 1));
      painter->drawLine(COORD_DISTANCE, (height / 2), width - 20, (height / 2));

      float dva = 2.0;
      if(scale_va > 0.15)      dva = 5.0;
      else if(scale_va > 0.1) dva = 3.5;
      else if(scale_va > 0.08) dva = 2.5;

      float va = 0;
      painterText.setFont(QFont("helvetica",8));

      while(va / scale_va < (height / 2) - Y_DISTANCE)
        {
          text.sprintf("%.1f m/s",va);
          painterText.setPen(QPen(QColor(255,100,100), 1));
          painterText.drawText(0,(height / 2) - (int)( va / scale_va ) - 10,
                           COORD_DISTANCE - 3,20,Qt::AlignRight | Qt::AlignVCenter,text);

          painter->setPen(QPen(QColor(255,200,200), 1));
          int y=(height / 2) - (int)( va / scale_va );
          painter->drawLine(COORD_DISTANCE - 3, y, width - 20, y);

          if(va != 0)
            {
              text.sprintf("-%.1f m/s",va);
              painterText.setPen(QPen(QColor(255,100,100), 1));

              painterText.drawText(0,(height / 2) + (int)( va / scale_va ) -10,
                         COORD_DISTANCE - 3,20,Qt::AlignRight | Qt::AlignVCenter,text);

              painter->setPen(QPen(QColor(255,200,200), 1));
              painter->drawLine(COORD_DISTANCE,(height / 2) + (int)( va / scale_va ) /**- 3*/,
                      width - 20,(height / 2) + (int)( va / scale_va ));

            }
          va += dva;
        }
    }
  else if(!baro && !vario && speed)
    {
      // Speedogramm

      int dv = 10;
      if(scale_v > 0.9)     dv = 40;
      else if(scale_v > 0.6) dv = 25;
      else if(scale_v > 0.3) dv = 20;

      int v = dv;
      painterText.setFont(QFont("helvetica",8));

      while(v / scale_v < height - (Y_DISTANCE * 2))
        {
          text.sprintf("%d km/h",v);
          painterText.setPen(QPen(QColor(0,0,0), 1));
          painterText.drawText(0,height - (int)( v / scale_v ) - Y_DISTANCE - 10,
                     COORD_DISTANCE - 3,20,Qt::AlignRight | Qt::AlignVCenter,text);

          painter->setPen(QPen(QColor(200,200,200), 1));
          painter->drawLine(COORD_DISTANCE - 3,
                      height - (int)( v / scale_v ) - Y_DISTANCE,
                      width - 20,height - (int)( v / scale_v ) - Y_DISTANCE);

          v += dv;
        }
    }

    painterText.end();
}


void EvaluationView::drawCurve( bool arg_vario,
                                bool arg_speed,
                                bool arg_baro,
                                unsigned int arg_smoothness_va,
                                unsigned int arg_smoothness_v,
                                unsigned int arg_smoothness_h,
                                unsigned int secW )
{
  Flight* newFlight = evalDialog->getFlight();

  // qDebug() << "EvaluationView::drawCurve(): Flight=" << newFlight;

  if( flight != newFlight )
    {
      flight = newFlight;

      if( flight )
        {
          cursor1 = startTime = flight->getStartTime();
          cursor2 = landTime  = flight->getLandTime();
        }
      else
        {
          cursor1 = 0;
          cursor2 = 0;
        }
    }

  if( flight )
    {
      cursor1 = qMax( startTime, cursor1 );
      cursor2 = qMax( startTime, cursor2 );
      cursor1 = qMin( landTime, cursor1 );
      cursor2 = qMin( landTime, cursor2 );

      secWidth = secW;

      int width = (landTime - startTime) / secWidth + (COORD_DISTANCE * 2) + 20;

      int maxWidth = qMax( width, scrollFrame->viewport()->width() );

      // Resize the draw widget to the needed size.
      resize( maxWidth, scrollFrame->viewport()->height() );

      // Resize pixmaps to the needed size.
      pixBufferKurve = QPixmap( width, scrollFrame->viewport()->height());
      pixBufferYAxis = QPixmap(COORD_DISTANCE + 1, scrollFrame->viewport()->height());

      // Clear pixmaps.
      pixBufferKurve.fill( Qt::white );
      pixBufferYAxis.fill( Qt::white );

      vario = arg_vario;
      speed = arg_speed;
      baro  = arg_baro;
      smoothness_va = arg_smoothness_va;
      smoothness_v  = arg_smoothness_v;
      smoothness_h  = arg_smoothness_h;

      __draw();
    }
  else
    {
      // Clear pixmaps, if no flight is assigned.
      pixBufferKurve.fill( Qt::white );
      pixBufferYAxis.fill( Qt::white );
    }

  repaint();
}

void EvaluationView::__draw()
{
  // vertical scale factor
  scale_v = getSpeed(flight->getPoint(Flight::V_MAX)) /
          ((double)(this->height() - 2*Y_DISTANCE));

  scale_h = flight->getPoint(Flight::H_MAX).height /
          ((double)(this->height() - 2*Y_DISTANCE));

  scale_va = qMax((double)getVario(flight->getPoint(Flight::VA_MAX)),
              ( -1.0 * (double)getVario(flight->getPoint(Flight::VA_MIN))) ) /
          ((double)(this->height() - 2*Y_DISTANCE) / 2.0);

  unsigned int gn_v  = smoothness_v * 2 + 1;
  unsigned int gn_va = smoothness_va * 2 + 1;
  unsigned int gn_h  = smoothness_h * 2 + 1;

  int*   baro_d       = new int[gn_h];
  int*   baro_d_last  = new int[gn_h];
  int*   elev_d       = new int[gn_h];
  int*   elev_d_last  = new int[gn_h];
  float* speed_d      = new float[gn_v];
  float* speed_d_last = new float[gn_v];
  float* vario_d      = new float[gn_va];
  float* vario_d_last = new float[gn_va];

  for(unsigned int loop = 0; loop < gn_h; loop++)
    {
      baro_d[loop] = flight->getPoint(loop).height;
      baro_d_last[loop] = flight->getPoint(flight->getRouteLength() - loop - 1).height;
      elev_d[loop] = flight->getPoint(loop).surfaceHeight;
      elev_d_last[loop] = flight->getPoint(flight->getRouteLength() - loop - 1).surfaceHeight;
    }
  for(unsigned int loop = 0; loop < gn_v; loop++)
    {
      speed_d[loop] = getSpeed(flight->getPoint(loop));
      speed_d_last[loop] = getSpeed(flight->getPoint(flight->getRouteLength() - loop - 1));
    }
  for(unsigned int loop = 0; loop < gn_va; loop++)
    {
      vario_d[loop] = getVario(flight->getPoint(loop));
      vario_d_last[loop] =
          getVario(flight->getPoint(flight->getRouteLength() - loop - 1));
    }

  QPolygon baroArray(flight->getRouteLength());
  QPolygon elevArray(flight->getRouteLength()+2);
  QPolygon varioArray(flight->getRouteLength());
  QPolygon speedArray(flight->getRouteLength());

  for(unsigned int loop = 0; loop < flight->getRouteLength(); loop++)
    {
      curTime = flight->getPoint(loop).time;

      // Correct time for overnight-flights:
      if(curTime < startTime)
        {
          curTime += 86400;
        }

      /* Das Array wird hier noch falsch gefüllt. Wenn über 3 Punkte geglättet wird, stimmt
       * alles. Wenn jedoch z.B. über 5 Punkte geglättet wird, werden die Punkte
       * ( -4, -3, -2, -1, 0, 1) genommen, statt (-2, -1, 0, 1, 2). Das ist vermutlich
       * die Ursache dafür, dass die Kurve "wandert".
       */
      if(loop < flight->getRouteLength() - smoothness_h && loop > smoothness_h) {
          baro_d[(loop - smoothness_h - 1) % gn_h] = flight->getPoint(loop + smoothness_h).height;
          elev_d[(loop - smoothness_h - 1) % gn_h] = flight->getPoint(loop + smoothness_h).surfaceHeight;
      }

      if(loop < flight->getRouteLength() - smoothness_v && loop > smoothness_v)
          speed_d[(loop - smoothness_v - 1) % gn_v] = getSpeed(flight->getPoint(loop + smoothness_v));

      if(loop < flight->getRouteLength() - smoothness_va && loop > smoothness_va)
          vario_d[(loop - smoothness_va - 1) % gn_va] = getVario(flight->getPoint(loop + smoothness_va));

      /* Wenn das Glätten wie bei __speedPoint() erfolgt, können gn_? und loop auch als
       * unsigned übergeben werden ...
       */
      if(loop < flight->getRouteLength() - smoothness_h) {
          baroArray.setPoint(loop, __baroPoint(baro_d, gn_h, loop));
          elevArray.setPoint(loop, __baroPoint(elev_d, gn_h, loop));
      } else {
          baroArray.setPoint(loop, __baroPoint(baro_d_last, gn_h,
                        flight->getRouteLength() - loop - 1));
          elevArray.setPoint(loop, __baroPoint(elev_d_last, gn_h,
                        flight->getRouteLength() - loop - 1));
      }
      if(loop < flight->getRouteLength() - smoothness_va)
          varioArray.setPoint(loop,
              __varioPoint(vario_d, gn_va, loop));
      else
          varioArray.setPoint(loop,
              __varioPoint(vario_d_last, gn_va,
                        flight->getRouteLength() - loop - 1));

      if(loop < flight->getRouteLength() - smoothness_v)
          speedArray.setPoint(loop,
              __speedPoint(speed_d, gn_v, loop));
      else
          speedArray.setPoint(loop,
              __speedPoint(speed_d_last, gn_v,
                        flight->getRouteLength() - loop - 1));
    }

  pixBufferKurve.fill(Qt::white);
  QPainter painter;
  painter.begin(&pixBufferKurve);

  if(baro)
    { // draw elevation
      painter.setBrush(QColor(35, 120, 20));
      painter.setPen(QPen(QColor(35, 120, 20), 1));
      // add two points so we can draw a filled area

      elevArray.setPoint(flight->getRouteLength(),
        QPoint(
      elevArray[flight->getRouteLength()-1].x(),
      scrollFrame->viewport()->height() - Y_DISTANCE ) );

      elevArray.setPoint( flight->getRouteLength()+1,
        QPoint(
      X_DISTANCE,
      scrollFrame->viewport()->height() - Y_DISTANCE ) );
      painter.drawPolygon(elevArray);
    }

  __drawCsystem(&painter);

  int xpos = 0;

  // turnpoints
  QList<Waypoint*>  wP;
  QString timeText = 0;

  wP = flight->getWPList();

  for(int n = 1; n + 1 < wP.count(); n++)
    {
      xpos = (wP.at(n)->sector1 - startTime ) / secWidth + X_DISTANCE;

      painter.setPen(QPen(QColor(100,100,100), 3));
      painter.drawLine(xpos, this->height() - Y_DISTANCE, xpos, Y_DISTANCE + 5);
      painter.setPen(QPen(QColor(0,0,0), 3));
      painter.setFont(QFont("helvetica",8));
      painter.drawText (xpos - 40, Y_DISTANCE - 20 - 5,80,10, Qt::AlignCenter, wP.at(n)->name);

      if(wP.at(n)->sector1 != 0)
          timeText = printTime(wP.at(n)->sector1);
      else if(wP.at(n)->sector2 != 0)
          timeText = printTime(wP.at(n)->sector2);
      else if(wP.at(n)->sectorFAI != 0)
          timeText = printTime(wP.at(n)->sectorFAI);
      painter.setFont(QFont("helvetica",7));
      painter.drawText(xpos - 40, Y_DISTANCE - 10 - 5, 80, 10, Qt::AlignCenter,timeText);
    }

  if(vario)
    {
      painter.setPen(QPen(QColor(255,100,100), 1));
      painter.drawPolyline(varioArray);
    }

  if(speed)
    {
      painter.setPen(QPen(QColor(0,0,0), 1));
      painter.drawPolyline(speedArray);
    }

  if(baro)
    {
      painter.setPen(QPen(QColor(100, 100, 255), 1));
      painter.drawPolyline(baroArray);
    }

  painter.end();

  __paintCursor(( cursor1 - startTime ) / secWidth + X_DISTANCE, 0, 1);
  __paintCursor(( cursor2 - startTime ) / secWidth + X_DISTANCE, 0, 2);

  delete [] baro_d;
  delete [] baro_d_last;
  delete [] speed_d;
  delete [] speed_d_last;
  delete [] vario_d;
  delete [] vario_d_last;
}

void EvaluationView::__paintCursor(int xpos, int move, int cursor)
{
  // Screen coordinates !!!
  QPainter painter;

  if( move == 1 )
    {
      // This part is called by the mouse move event to replace the current cursor.
      if( pixBufferKurve.isNull() )
        {
          return;
        }

      if( pixBufferMouse.size() != pixBufferKurve.size() )
        {
          pixBufferMouse = QPixmap( pixBufferKurve.size() );
        }

      // Reset pixmap content
      pixBufferMouse.fill( Qt::transparent );

      painter.begin( &pixBufferMouse );

      if(cursor == 1)
        {
          painter.setPen(QPen(QColor(0,200,0), 1)); // green color
        }
      else
        {
          painter.setPen(QPen(QColor(200,0,0), 1)); // red color
        }

      painter.drawLine( xpos, this->height() - Y_DISTANCE, xpos, Y_DISTANCE );
      painter.end();
    }
   else
    {
      painter.begin( &pixBufferKurve );

      if(cursor == 1)
        {
          painter.setPen(QPen(QColor(0,200,0), 1));
          painter.setBrush(QBrush(QColor(0,200,0), Qt::SolidPattern));
        }
      else
        {
          painter.setPen(QPen(QColor(200,0,0), 1));
          painter.setBrush(QBrush(QColor(200,0,0), Qt::SolidPattern));
        }

      QPixmap pixCursor1 = _mainWindow->getPixmap("flag_green.png");
      QPixmap pixCursor2 = _mainWindow->getPixmap("flag_red.png");

      // draw new line
      painter.drawLine(xpos, this->height() - Y_DISTANCE, xpos, Y_DISTANCE);

      // draw flags
      if(cursor == 1)
        {
          painter.drawPixmap(xpos - 32, Y_DISTANCE - 30, pixCursor1);
        }
      else
        {
          painter.drawPixmap(xpos, Y_DISTANCE - 30, pixCursor2);
        }

      painter.end();
    }
}

void EvaluationView::resizeEvent(QResizeEvent* event)
{
  QWidget::resize( event->size() );
}

/**
 * Makes a drawn flight pointer visible.
 */
void EvaluationView::makeFlightPointerVisible()
{
  int left = lastPointerPosition.x();
  int top  = lastPointerPosition.y();

  if( ! flightPointPointer || left < 0 || top < 0 )
    {
      return;
    }

  // get the current scroll position
  int cx = scrollFrame->horizontalScrollBar()->value();

  if(!((vario && speed) || (vario && baro) || (baro && speed)))
    {
      // The Y axis is being drawn, so we need to take this into account then
      // ensuring visibility of our pointer.
      if( cx + pixBufferYAxis.width() + 50 > left )
        {
          scrollFrame->ensureVisible( left, top, pixBufferYAxis.width() + 50, 0 );
        }
      else
        {
          scrollFrame->ensureVisible( left, top );
        }
    }
  else
    {
      scrollFrame->ensureVisible( left, top );
    }
}

/** Prepares the flight point pointer. */
void EvaluationView::preparePointer()
{
  // create pointer pixmap
  pixPointer = QPixmap(12,9);
  pixPointer.fill( Qt::transparent );

  // draw the pointer in the buffer
  QPolygon pa = QPolygon(3);
  pa.setPoint(0,6,0);
  pa.setPoint(1,0,9);
  pa.setPoint(2,12,9);

  QPainter painter(&pixPointer);

  painter.setPen(QPen(QColor(255,128,0),1));
  painter.setBrush(QBrush(QColor(255,128,0))); // orange
  painter.drawPolygon(pa);

  lastPointerPosition = QPoint(-1000, -1000);
}

/* Shows a pointer under the time axis to indicate the position
 * of FlightPoint fp in the graph. If fp=0, then the flight point
 * is removed.
 */
void EvaluationView::slotShowPointer(const FlightPoint* fp)
{
  flightPointPointer = fp;

  if( flight )
    {
      // Draw only, if a flight is defined.
      repaint();
      makeFlightPointerVisible();
    }

  if( ! fp )
    {
      // Clear the last flight point pointer position.
      lastPointerPosition = QPoint(-1000, -1000);
    }
}
