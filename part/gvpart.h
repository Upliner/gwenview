/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <aurelien.gateau@free.fr>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#ifndef GVPART_H
#define GVPART_H

// Qt
#include <QPoint>

// KDE
#include <kparts/part.h>

// Local
#include <lib/document/document.h>

class KAboutData;
class KAction;

class QLabel;

namespace KParts {
class StatusBarExtension;
}

namespace Gwenview {

class DocumentView;
class ImageView;

class GVPart : public KParts::ReadOnlyPart {
	Q_OBJECT
public:
	GVPart(QWidget* parentWidget, QObject* parent, const QStringList&);

	static KAboutData* createAboutData();

public Q_SLOTS:
	virtual void loadConfig();

protected:
	virtual bool openFile();
	virtual bool openUrl(const KUrl&);

private Q_SLOTS:
	void showContextMenu();
	void showProperties();
	void saveAs();
	void showJobError(KJob*);
	void slotLoaded();
	void slotLoadingFailed();

private:
    DocumentView* mDocumentView;
	ImageView* mView;
	Document::Ptr mDocument;

	QWidget* mErrorWidget;
	QLabel* mErrorLabel;

	void addPartSpecificActions();

	void createErrorLabel();
};

} // namespace


#endif /* GVPART_H */
