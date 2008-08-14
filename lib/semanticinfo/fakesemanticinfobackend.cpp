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
#include "fakesemanticinfobackend.moc"

// Qt
#include <QStringList>

// KDE
#include <kurl.h>

// Local

namespace Gwenview {


FakeMetaDataBackEnd::FakeMetaDataBackEnd(QObject* parent, InitializeMode mode)
: AbstractMetaDataBackEnd(parent)
, mInitializeMode(mode) {
	mAllTags
		<< tagForLabel("beach")
		<< tagForLabel("mountains")
		<< tagForLabel("wallpaper")
		;
}


void FakeMetaDataBackEnd::storeMetaData(const KUrl& url, const MetaData& metaData) {
	mMetaDataForUrl[url] = metaData;
	mergeTagsWithAllTags(metaData.mTags);
}


void FakeMetaDataBackEnd::mergeTagsWithAllTags(const TagSet& set) {
	int size = mAllTags.size();
	mAllTags |= set;
	if (mAllTags.size() > size) {
		emit allTagsUpdated();
	}
}


TagSet FakeMetaDataBackEnd::allTags() const {
	return mAllTags;
}


void FakeMetaDataBackEnd::retrieveMetaData(const KUrl& url) {
	if (!mMetaDataForUrl.contains(url)) {
		QString urlString = url.url();
		MetaData metaData;
		if (mInitializeMode == InitializeRandom) {
			metaData.mRating = int(urlString.length()) % 6;
			metaData.mDescription = url.fileName();
			QStringList lst = url.path().split("/");
			Q_FOREACH(const QString& token, lst) {
				if (!token.isEmpty()) {
					metaData.mTags << '#' + token.toLower();
				}
			}
			metaData.mTags << QString("#length-%1").arg(url.fileName().length());

			mergeTagsWithAllTags(metaData.mTags);
		} else {
			metaData.mRating = 0;
		}
		mMetaDataForUrl[url] = metaData;
	}
	emit metaDataRetrieved(url, mMetaDataForUrl.value(url));
}


QString FakeMetaDataBackEnd::labelForTag(const MetaDataTag& tag) const {
	return tag[1].toUpper() + tag.mid(2);
}


MetaDataTag FakeMetaDataBackEnd::tagForLabel(const QString& label) const {
	return '#' + label.toLower();
}


} // namespace