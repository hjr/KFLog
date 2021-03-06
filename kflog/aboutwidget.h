/***********************************************************************
**
**   aboutwidget.h
**
**   This file is part of KFLog.
**
************************************************************************
**
**   Copyright (c): 2010-2014 Axel Pauli
**
**   This file is distributed under the terms of the General Public
**   License. See the file COPYING for more information.
**
**   $Id: aboutwidget.h 4502 2010-12-09 22:32:02Z axel $
**
***********************************************************************/

/**
 * \class AboutWidget
 *
 * \author Axel Pauli
 *
 * \brief A widget to display the about application data.
 *
 * This widget displays the about application data in a tabbed window
 * decorated this a headline and an icon on top.
 *
 * \date 2010-2014
 *
*/

#ifndef ABOUT_WIDGET_H
#define ABOUT_WIDGET_H

#include <QWidget>
#include <QFile>
#include <QLabel>
#include <QString>
#include <QTextBrowser>

class QPixmap;

class AboutWidget : public QWidget
{
  Q_OBJECT

  private:

  Q_DISABLE_COPY ( AboutWidget )

 public:

  AboutWidget( QWidget *parent = 0 );

  virtual ~AboutWidget() {};

  /**
  * Sets the passed pixmap at the left side of the headline.
  *
  * \param pixmap The pixmap to be set in the headline.
  */
  void setHeaderIcon( const QPixmap pixmap )
  {
    m_headerIcon->setPixmap( pixmap );
  };

  /**
  * Sets the passed text in the headline. The text can be HTML formatted.
  *
  * \param text The text to be set in the headline.
  */
  void setHeaderText( const QString& text )
  {
    m_headerText->setText( text );
  };

  /**
  * Sets the passed text on the about page. The text can be HTML formatted.
  *
  * \param text The text to be set on the about page.
  */
  void setAboutText( const QString& text )
  {
    m_about->setHtml( text );
  };

  /**
  * Sets the passed text on the team page. The text can be HTML formatted.
  *
  * \param text The text to be set on the team page.
  */
  void setTeamText( const QString& text )
  {
    m_team->setHtml( text );
  };

  /**
  * Sets the passed text on the eula page. The text can be HTML formatted.
  *
  * \param text The text to be set on the eula page.
  */
  void setEulaText( const QString& text )
  {
    m_eula->setText( text );
  };

 private:

  /** The header icon widget. */
  QLabel       *m_headerIcon;

  /** The header text widget. */
  QLabel       *m_headerText;

  /** The about text browser page widget. */
  QTextBrowser *m_about;

  /** The team text browser page widget. */
  QTextBrowser *m_team;

  /** The Eula browser page widget. */
  QTextBrowser *m_eula;

};

#endif /* ABOUT_WIDGET_H */
