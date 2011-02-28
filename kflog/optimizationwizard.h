/***********************************************************************
**
**   optimizationwizard.h
**
**   This file is part of KFLog4.
**
************************************************************************
**
** Created: Sam Mär 8 12:18:37 2003
**
**   This file is distributed under the terms of the General Public
**   License. See the file COPYING for more information.
**
**   $Id$
**
****************************************************************************/

#ifndef OLC_OPTIMIZATION_H
#define OLC_OPTIMIZATION_H

#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QProgressBar>
#include <QTextBrowser>
#include <QVariant>
#include <QWizard>

#include "evaluationdialog.h"
#include "flight.h"
#include "map.h"
#include "mapcontents.h"

class OptimizationWizard : public QWizard
{
  Q_OBJECT

  private:

  Q_DISABLE_COPY( OptimizationWizard )

public:

  OptimizationWizard( QWidget* parent = 0 );

  virtual ~OptimizationWizard();

  QWizardPage *page;
  EvaluationDialog* evaluation;
  QGroupBox* groupBox1;
  QLabel* lblStartHeight;
  QLabel* lblStopTime;
  QLabel* lblDiffHeight;
  QLabel* textLabel1_3_2_2;
  QLabel* lblStartTime;
  QLabel* textLabel1_4_2;
  QLabel* textLabel1_2_2_2;
  QLabel* lblStopHeight;
  QLabel* lblDiffTime;
  QPushButton* kPushButton2;
  QWizardPage* page_2;
  QTextBrowser* kTextBrowser1;
  QFrame* frame3;
  QProgressBar* progress;
  QPushButton* btnStart;
  QPushButton* btnStop;

  virtual void init();
  virtual double optimizationResult( unsigned int pointList[LEGS+3], double * points );

public slots:

  virtual void slotStartOptimization();
  virtual void slotStopOptimization();
  virtual void slotSetTimes();
  virtual void setMapContents( Map * _map );

protected:

  Flight* flight;
  QList<FlightPoint*> route;
  Optimization* optimization;

  QVBoxLayout* pageLayout;
  QVBoxLayout* layout14;
  QHBoxLayout* layout12;
  QGridLayout* layout3;
  QVBoxLayout* pageLayout_2;
  QVBoxLayout* layout13;
  QHBoxLayout* layout12_2;
  QVBoxLayout* frame3Layout;
  QVBoxLayout* layout5;
  QHBoxLayout* layout2;

protected slots:

  virtual void languageChange();

private:

  QPixmap image0;

};

#endif // OLC_OPTIMIZATION_H
