// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Gwenview - A simple image viewer for KDE
Copyright (c) 2000-2003 Aur�lien G�teau

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

// KDE includes
#include <kdialogbase.h>

class GVMainWindow;
class ConfigDialogPrivate;

class ConfigDialog : public KDialogBase {
Q_OBJECT
public:
	ConfigDialog(QWidget* parent,GVMainWindow*);
	~ConfigDialog();

protected slots:
	void slotOk();
	void slotApply();

private slots:
	void calculateCacheSize();
	void emptyCache();
	void onCacheEmptied(KIO::Job*);

private:
	ConfigDialogPrivate* d;
};



#endif
