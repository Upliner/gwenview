/*
Gwenview - A simple image viewer for KDE
Copyright (C) 2000-2002 Aur�lien G�teau

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

// Qt includes
#include <qlayout.h>
#include <qpainter.h>
#include <qpen.h>
#include <qpixmap.h>
#include <qpixmapcache.h>
#include <qpushbutton.h>

// KDE includes
#include <kapplication.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kurldrag.h>
#include <kwordwrap.h>

// Our includes
#include "filethumbnailviewitem.h"
#include "gvarchive.h"
#include "thumbnailloadjob.h"

#include "filethumbnailview.moc"

static const char* CONFIG_THUMBNAIL_SIZE="thumbnail size";
static const char* CONFIG_MARGIN_SIZE="margin size";
static const char* CONFIG_WORD_WRAP_FILENAME="word wrap filename";


FileThumbnailView::FileThumbnailView(QWidget* parent)
: KIconView(parent), GVFileView(), mViewedItem(0L), mThumbnailLoadJob(0L)
{
	setAutoArrange(true);
	QIconView::setSorting(true);
	setItemsMovable(false);
	setResizeMode(Adjust);
	setShowToolTips(true);
	setSpacing(0);
	viewport()->setAcceptDrops(false);

// If we use KIconView::Execute mode, the current item is unselected after
// being clicked, so we use KIconView::Select mode and emit ourself the
// execute() signal with slotClicked()
	setMode(KIconView::Select);
	connect(this,SIGNAL(clicked(QIconViewItem*,const QPoint&)),
		this,SLOT(slotClicked(QIconViewItem*,const QPoint&)) );
	
	QIconView::setSelectionMode(Extended);
}


FileThumbnailView::~FileThumbnailView() {
    stopThumbnailUpdate();
}


void FileThumbnailView::setThumbnailSize(ThumbnailSize value) {
	if (value==mThumbnailSize) return;
	mThumbnailSize=value;
	updateGrid();
}


void FileThumbnailView::setMarginSize(int value) {
	if (value==mMarginSize) return;
	mMarginSize=value;
	updateGrid();
}


void FileThumbnailView::setThumbnailPixmap(const KFileItem* fileItem,const QPixmap& thumbnail) {
	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	int pixelSize=mThumbnailSize.pixelSize();

// Draw the thumbnail to the center of the icon
	QPainter painter(iconItem->pixmap());
	painter.eraseRect(0,0,pixelSize,pixelSize);
	painter.drawPixmap(
		(pixelSize-thumbnail.width())/2,
		(pixelSize-thumbnail.height())/2,
		thumbnail);
	iconItem->repaint();

// Notify others that one thumbnail has been updated
	emit updatedOneThumbnail();
}


void FileThumbnailView::startThumbnailUpdate()
{
	stopThumbnailUpdate(); // just in case

	mThumbnailLoadJob = new ThumbnailLoadJob(this->items(),mThumbnailSize);
	connect(mThumbnailLoadJob, SIGNAL(thumbnailLoaded(const KFileItem*,const QPixmap&)),
		this, SLOT(setThumbnailPixmap(const KFileItem*,const QPixmap&)) );
	connect(mThumbnailLoadJob, SIGNAL(result(KIO::Job*)),
		this, SIGNAL(updateEnded()) );
	emit updateStarted(QIconView::count());
	mThumbnailLoadJob->start();
}


void FileThumbnailView::stopThumbnailUpdate()
{
	if (!mThumbnailLoadJob.isNull()) {
		emit updateEnded();
		mThumbnailLoadJob->kill();
	}
}


void FileThumbnailView::setViewedFileItem(const KFileItem* fileItem) {
	FileThumbnailViewItem* oldViewed=mViewedItem;
	FileThumbnailViewItem* item=viewItem(fileItem);
	
	mViewedItem=item;
	if (oldViewed) repaintItem(oldViewed);
	if (mViewedItem) repaintItem(mViewedItem);
}


//-KFileView methods--------------------------------------------------------
void FileThumbnailView::clearView() {
    stopThumbnailUpdate();
	mViewedItem=0L;
	QIconView::clear();
}


void FileThumbnailView::insertItem(KFileItem* item) {
	if (!item) return;

	bool isDirOrArchive=item->isDir() || GVArchive::fileItemIsArchive(item);
	
	int pixelSize=mThumbnailSize.pixelSize();
	QPixmap thumbnail(pixelSize,pixelSize);
	QPainter painter(&thumbnail);
	painter.eraseRect(0,0,pixelSize,pixelSize);

	if (isDirOrArchive) {
		// Load the icon
		QPixmap itemPix=item->pixmap(pixelSize);
		painter.drawPixmap(
			(pixelSize-itemPix.width())/2,
			(pixelSize-itemPix.height())/2,
			itemPix);
	} else {
		// Create an empty thumbnail
		painter.setPen(colorGroup().button());
		painter.drawRect(0,0,pixelSize,pixelSize);
	}

	// Create icon item
	QDir::SortSpec spec = KFileView::sorting();
	FileThumbnailViewItem* iconItem=new	FileThumbnailViewItem(this,item->text(),thumbnail,item);
	iconItem->setKey( sortingKey( item->text(), isDirOrArchive, spec ));

	item->setExtraData(this,iconItem);
}


void FileThumbnailView::updateView(const KFileItem* fileItem) {
	if (!fileItem) return;

	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (iconItem) iconItem->setText(fileItem->text());
	sort();
}


void FileThumbnailView::ensureItemVisible(const KFileItem* fileItem) {
	if (!fileItem) return;

	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (iconItem) QIconView::ensureItemVisible(iconItem);
}


void FileThumbnailView::setCurrentItem(const KFileItem* fileItem) {
	if (!fileItem) return;

	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (iconItem) QIconView::setCurrentItem(iconItem);
}


void FileThumbnailView::setSelected(const KFileItem* fileItem,bool enable) {
	if (!fileItem) return;

	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (iconItem) QIconView::setSelected(iconItem,enable);
}


bool FileThumbnailView::isSelected(const KFileItem* fileItem) const {
	if (!fileItem) return false;

	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (!iconItem) return false;

	return iconItem->isSelected();
}


void FileThumbnailView::removeItem(const KFileItem* fileItem) {
	if (!fileItem) return;

// Remove it from the image preview job
	if (!mThumbnailLoadJob.isNull())
		mThumbnailLoadJob->itemRemoved(fileItem);

// Remove it from our view
	FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (iconItem==mViewedItem) mViewedItem=0L;
	if (iconItem) delete iconItem;
	KFileView::removeItem(fileItem);
	arrangeItemsInGrid();
}


KFileItem* FileThumbnailView::firstFileItem() const {
	FileThumbnailViewItem* iconItem=static_cast<FileThumbnailViewItem*>(firstItem());
	if (!iconItem) return 0L;
	return iconItem->fileItem();
}


KFileItem* FileThumbnailView::prevItem(const KFileItem* fileItem) const {
	const FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (!iconItem) return 0L;

	iconItem=static_cast<const FileThumbnailViewItem*>(iconItem->prevItem());
	if (!iconItem) return 0L;

	return iconItem->fileItem();
}


KFileItem* FileThumbnailView::currentFileItem() const {
	const QIconViewItem* iconItem=currentItem();
	if (!iconItem) return 0L;

	return static_cast<const FileThumbnailViewItem*>(iconItem)->fileItem();
}


KFileItem* FileThumbnailView::nextItem(const KFileItem* fileItem) const {
	const FileThumbnailViewItem* iconItem=viewItem(fileItem);
	if (!iconItem) return 0L;

	iconItem=static_cast<const FileThumbnailViewItem*>(iconItem->nextItem());
	if (!iconItem) return 0L;

	return iconItem->fileItem();
}


//-Private -----------------------------------------------------------------
void FileThumbnailView::updateGrid() {
	setGridX(mThumbnailSize.pixelSize() + mMarginSize);
}


//-Private slots------------------------------------------------------------
void FileThumbnailView::slotClicked(QIconViewItem* iconItem,const QPoint& pos) {
	if (!iconItem) return;
	if (!KGlobalSettings::singleClick()) return;
	FileThumbnailViewItem* thumbItem=static_cast<FileThumbnailViewItem*>(iconItem);

	KFileItem* fileItem=thumbItem->fileItem();
	if (!fileItem) return;

	if (fileItem->isDir() || GVArchive::fileItemIsArchive(fileItem)) {
		emit executed(iconItem);
		emit executed(iconItem,pos);
	}
}


//-Protected----------------------------------------------------------------
void FileThumbnailView::startDrag() {
	KURL::List urls;
	KFileItemListIterator it(*KFileView::selectedItems());

	for ( ; it.current(); ++it ) {
		urls.append(it.current()->url());
	}

	if (urls.isEmpty()) {
		kdWarning() << "No item to drag\n";
		return;
	}

	QDragObject* drag=KURLDrag::newDrag(urls, this);
	QPixmap dragPixmap;
	if (urls.count()>1) {
		dragPixmap=SmallIcon("kmultiple");
	} else {
		dragPixmap=KFileView::selectedItems()->getFirst()->pixmap(16);
	}
	drag->setPixmap( dragPixmap, QPoint(dragPixmap.width()/2, dragPixmap.height()/2) );

	drag->dragCopy();
}


//-Configuration------------------------------------------------------------
void FileThumbnailView::readConfig(KConfig* config,const QString& group) {
	config->setGroup(group);

	mThumbnailSize=config->readEntry(CONFIG_THUMBNAIL_SIZE);
	mMarginSize=config->readNumEntry(CONFIG_MARGIN_SIZE,5);

	updateGrid();
	setWordWrapIconText(config->readBoolEntry(CONFIG_WORD_WRAP_FILENAME,false));
	arrangeItemsInGrid();
}


void FileThumbnailView::writeConfig(KConfig* config,const QString& group) const {
	config->setGroup(group);
	config->writeEntry(CONFIG_THUMBNAIL_SIZE,QString(mThumbnailSize));
	config->writeEntry(CONFIG_MARGIN_SIZE,mMarginSize);
	config->writeEntry(CONFIG_WORD_WRAP_FILENAME,wordWrapIconText());
}

