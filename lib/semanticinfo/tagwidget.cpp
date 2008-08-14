// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <aurelien.gateau@free.fr>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "tagwidget.moc"

// Qt
#include <QCompleter>
#include <QItemDelegate>
#include <QListWidget>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

// KDE
#include <kdebug.h>
#include <kiconloader.h>
#include <klineedit.h>

// Local
#include <lib/semanticinfo/tagmodel.h>

namespace Gwenview {


class TagItemDelegate : public QItemDelegate {
public:
	TagItemDelegate(QObject* parent = 0)
	: QItemDelegate(parent)
	, mRemovePixmap(SmallIcon("list-remove"))
	{}

protected:
	void drawDisplay(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QString& text) const {
		QItemDelegate::drawDisplay(painter, option, rect, text);
		int left = rect.right() - mRemovePixmap.width();
		int top = rect.top() + (rect.height() - mRemovePixmap.height() ) / 2;
		painter->drawPixmap(left, top, mRemovePixmap);
	}

private:
	QPixmap mRemovePixmap;
};


class TagCompleterModel : public QSortFilterProxyModel {
public:
	TagCompleterModel(QObject* parent)
	: QSortFilterProxyModel(parent)
	{
	}


	void setTagInfo(const TagInfo& tagInfo) {
		mExcludedTagSet.clear();
		TagInfo::ConstIterator
			it = tagInfo.begin(),
			end = tagInfo.end();
		for(; it!=end; ++it) {
			if (it.value()) {
				mExcludedTagSet << it.key();
			}
		}
		invalidate();
	}


	void setMetaDataBackEnd(AbstractMetaDataBackEnd* backEnd) {
		setSourceModel(new TagModel(this, backEnd));
	}


protected:
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
		QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
		MetaDataTag tag = sourceIndex.data(TagModel::TagRole).toString();
		return !mExcludedTagSet.contains(tag);
	}


private:
	TagSet mExcludedTagSet;
};


struct TagWidgetPrivate {
	TagWidget* that;
	TagInfo mTagInfo;
	QListWidget* mTreeWidget;
	KLineEdit* mLineEdit;
	AbstractMetaDataBackEnd* mBackEnd;
	TagCompleterModel* mTagCompleterModel;


	void setupWidgets() {
		mTreeWidget = new QListWidget;
		mTreeWidget->setItemDelegate(new TagItemDelegate(mTreeWidget));

		mLineEdit = new KLineEdit;
		mLineEdit->setTrapReturnKey(true);

		mTagCompleterModel = new TagCompleterModel(that);
		QCompleter* completer = new QCompleter(that);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		completer->setModel(mTagCompleterModel);
		mLineEdit->setCompleter(completer);

		QVBoxLayout* layout = new QVBoxLayout(that);
		layout->setMargin(0);
		layout->addWidget(mTreeWidget);
		layout->addWidget(mLineEdit);

		that->setTabOrder(mLineEdit, mTreeWidget);
	}


	void fillTreeWidget() {
		Q_ASSERT(mBackEnd);
		mTreeWidget->clear();

		TagInfo::ConstIterator
			it = mTagInfo.begin(),
			end = mTagInfo.end();
		for(; it!=end; ++it) {
			MetaDataTag tag = it.key();
			QString label = mBackEnd->labelForTag(tag);
			QListWidgetItem* item = new QListWidgetItem(label, mTreeWidget);
			item->setData(TagModel::TagRole, QVariant(tag));
		}

		updateCompleterModel();
	}


	void updateCompleterModel() {
		mTagCompleterModel->setTagInfo(mTagInfo);
	}
};


TagWidget::TagWidget(QWidget* parent)
: QWidget(parent)
, d(new TagWidgetPrivate) {
	d->that = this;
	d->mBackEnd = 0;
	d->setupWidgets();

	connect(d->mTreeWidget, SIGNAL(itemClicked(QListWidgetItem*)),
		SLOT(slotItemClicked(QListWidgetItem*)) );

	connect(d->mLineEdit, SIGNAL(returnPressed()),
		SLOT(assignTag()) );
}


TagWidget::~TagWidget() {
	delete d;
}


void TagWidget::setMetaDataBackEnd(AbstractMetaDataBackEnd* backEnd) {
	d->mBackEnd = backEnd;
	d->mTagCompleterModel->setMetaDataBackEnd(backEnd);
}


void TagWidget::setTagInfo(const TagInfo& tagInfo) {
	d->mTagInfo = tagInfo;
	d->fillTreeWidget();
}


void TagWidget::assignTag() {
	Q_ASSERT(d->mBackEnd);
	QString label = d->mLineEdit->text();
	MetaDataTag tag = d->mBackEnd->tagForLabel(label);
	d->mTagInfo[tag] = true;
	d->fillTreeWidget();
	d->mLineEdit->clear();

	emit tagAssigned(tag);
}


void TagWidget::slotItemClicked(QListWidgetItem* item) {
	if (!item) {
		return;
	}
	MetaDataTag tag = item->data(TagModel::TagRole).toString();
	d->mTagInfo.remove(tag);
	d->fillTreeWidget();

	emit tagRemoved(tag);
}


} // namespace