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
#include "fullscreencontent.h"

// Qt
#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>

// KDE
#include <kactioncollection.h>
#include <klocale.h>
#include <kmacroexpander.h>
#include <kstandarddirs.h>

// Local
#include "imagemetainfodialog.h"
#include "thumbnailbarview.h"
#include <lib/document/document.h>
#include <lib/document/documentfactory.h>
#include <lib/gwenviewconfig.h>
#include <lib/imagemetainfomodel.h>
#include <lib/slideshow.h>


namespace Gwenview {


static QString loadFullScreenStyleSheet() {
	// Get themeDir
	QString themeName = GwenviewConfig::fullScreenTheme();
	QString themeDir = KStandardDirs::locate("appdata", "fullscreenthemes/" + themeName + "/");
	if (themeDir.isEmpty()) {
		kWarning() << "Couldn't find fullscreen theme" << themeName;
		return QString();
	}

	// Read css file
	QString styleSheetPath = themeDir + "/style.css";
	QFile file(styleSheetPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		kWarning() << "Couldn't open" << styleSheetPath;
		return QString();
	}
	QString styleSheet = QString::fromUtf8(file.readAll());

	// Replace vars
	QHash<QString, QString> macros;
	macros["themeDir"] = themeDir;
	styleSheet = KMacroExpander::expandMacros(styleSheet, macros, QLatin1Char('$'));

	return styleSheet;
}


class FullScreenConfigDialog : public QFrame {
public:
	FullScreenConfigDialog(QWidget* parent)
	: QFrame(parent) {
		setWindowFlags(Qt::Popup);
		setObjectName("configDialog");

		QString styleSheet = loadFullScreenStyleSheet();
		if (!styleSheet.isEmpty()) {
			setStyleSheet(styleSheet);
		}

		QPushButton* closeButton = new QPushButton;
		closeButton->setText(i18n("Close"));
		connect(closeButton, SIGNAL(clicked()), SLOT(close()) );

		QVBoxLayout* mainLayout = new QVBoxLayout(this);
		mLayout = new QVBoxLayout;
		mainLayout->addLayout(mLayout);
		mainLayout->addWidget(closeButton);
	}

	void addAction(QAction* action) {
		if (action->isCheckable()) {
			QCheckBox* checkBox = new QCheckBox;
			checkBox->setText(action->text());
			checkBox->setChecked(action->isChecked());
			QObject::connect(checkBox, SIGNAL(toggled(bool)),
				action, SLOT(trigger()) );
			mLayout->addWidget(checkBox);

		} else {
			QPushButton* button = new QPushButton(this);
			button->setText(action->text());
			QObject::connect(button, SIGNAL(clicked()),
				action, SLOT(trigger()) );
			mLayout->addWidget(button);
		}
	}

	void addWidget(QWidget* widget) {
		widget->setParent(this);
		mLayout->addWidget(widget);
	}

private:
	QVBoxLayout* mLayout;
};


struct FullScreenContentPrivate {
	FullScreenContent* that;
	SlideShow* mSlideShow;
	ThumbnailBarView* mThumbnailBar;
	QLabel* mInformationLabel;
	Document::Ptr mCurrentDocument;
	QPointer<ImageMetaInfoDialog> mImageMetaInfoDialog;
	QToolButton* mOptionsButton;

	void createOptionsButton() {
		mOptionsButton = new QToolButton;
		mOptionsButton->setIcon(KIcon("configure"));
		mOptionsButton->setToolTip(i18nc("@info:tooltip", "Slideshow options"));
		QObject::connect(mOptionsButton, SIGNAL(clicked()),
			that, SLOT(showFullScreenConfigDialog()) );
	}


	QWidget* createSlideShowIntervalWidget() {
		QSlider* slider = new QSlider;
		slider->setRange(2, 60);
		slider->setOrientation(Qt::Horizontal);
		slider->setMinimumWidth(180);
		QObject::connect(slider, SIGNAL(valueChanged(int)),
			mSlideShow, SLOT(setInterval(int)) );

		QLabel* label = new QLabel;
		QObject::connect(slider, SIGNAL(valueChanged(int)),
			label, SLOT(setNum(int)) );
		label->setFixedWidth(label->fontMetrics().width(" 88 "));

		slider->setValue(int(GwenviewConfig::interval()));

		QLabel* caption = new QLabel;
		caption->setText(i18n("Slideshow interval:"));
		caption->setBuddy(slider);

		QWidget* container = new QWidget;
		QHBoxLayout* layout = new QHBoxLayout(container);
		layout->setMargin(0);
		layout->addWidget(caption);
		layout->addWidget(slider);
		layout->addWidget(label);

		return container;
	}
};


FullScreenContent::FullScreenContent(QWidget* parent, KActionCollection* actionCollection, SlideShow* slideShow)
: QObject(parent)
, d(new FullScreenContentPrivate) {
	d->that = this;
	d->mSlideShow = slideShow;
	parent->installEventFilter(this);

	d->createOptionsButton();

	QToolBar* bar = new QToolBar;
	bar->setFloatable(false);
	bar->setIconSize(QSize(32, 32));
	bar->addAction(actionCollection->action("fullscreen"));
	bar->addAction(actionCollection->action("go_previous"));
	bar->addAction(actionCollection->action("go_next"));

	bar->addAction(actionCollection->action("toggle_slideshow"));
	bar->addWidget(d->mOptionsButton);

	d->mThumbnailBar = new ThumbnailBarView(parent);
	ThumbnailBarItemDelegate* delegate = new ThumbnailBarItemDelegate(d->mThumbnailBar);
	d->mThumbnailBar->setItemDelegate(delegate);
	d->mThumbnailBar->setThumbnailSize(64);
	d->mThumbnailBar->setFullScreenMode(true);

	d->mInformationLabel = new QLabel;
	d->mInformationLabel->setWordWrap(true);
	d->mInformationLabel->setAlignment(Qt::AlignCenter);

	QGridLayout* layout = new QGridLayout(parent);
	layout->setMargin(0);
	layout->setSpacing(0);
	layout->addWidget(bar, 0, 0);
	layout->addWidget(d->mInformationLabel, 1, 0);
	layout->addWidget(d->mThumbnailBar, 0, 1, 2, 1);
}


FullScreenContent::~FullScreenContent() {
	delete d;
}


ThumbnailBarView* FullScreenContent::thumbnailBar() const {
	return d->mThumbnailBar;
}


void FullScreenContent::setCurrentUrl(const KUrl& url) {
	d->mCurrentDocument = DocumentFactory::instance()->load(url);
	connect(d->mCurrentDocument.data(),SIGNAL(metaDataUpdated()),
		SLOT(updateInformationLabel()) );
	connect(d->mCurrentDocument.data(),SIGNAL(metaDataUpdated()),
		SLOT(updateMetaInfoDialog()) );
	updateInformationLabel();
	updateMetaInfoDialog();
}


void FullScreenContent::updateInformationLabel() {
	if (!d->mCurrentDocument) {
		kWarning() << "No document";
		return;
	}

	if (!d->mInformationLabel->isVisible()) {
		return;
	}

	ImageMetaInfoModel* model = d->mCurrentDocument->metaInfo();

	QStringList valueList;
	Q_FOREACH(const QString& key, GwenviewConfig::fullScreenPreferredMetaInfoKeyList()) {
		valueList << model->getValueForKey(key);
	}
	QString text = valueList.join(i18nc("@item:intext fullscreen meta info separator", ", "));

	d->mInformationLabel->setText(text);
}


bool FullScreenContent::eventFilter(QObject*, QEvent* event) {
	if (event->type() == QEvent::Show) {
		updateInformationLabel();
		updateMetaInfoDialog();
	}
	return false;
}


void FullScreenContent::configureInformationLabel() {
	if (!d->mImageMetaInfoDialog) {
		d->mImageMetaInfoDialog = new ImageMetaInfoDialog(d->mInformationLabel);
		d->mImageMetaInfoDialog->setAttribute(Qt::WA_DeleteOnClose, true);
		connect(d->mImageMetaInfoDialog, SIGNAL(preferredMetaInfoKeyListChanged(const QStringList&)),
			SLOT(slotPreferredMetaInfoKeyListChanged(const QStringList&)) );
	}
	d->mImageMetaInfoDialog->setMetaInfo(d->mCurrentDocument->metaInfo(), GwenviewConfig::fullScreenPreferredMetaInfoKeyList());
	d->mImageMetaInfoDialog->show();
}


void FullScreenContent::slotPreferredMetaInfoKeyListChanged(const QStringList& list) {
	GwenviewConfig::setFullScreenPreferredMetaInfoKeyList(list);
	GwenviewConfig::self()->writeConfig();
	updateInformationLabel();
}


void FullScreenContent::updateMetaInfoDialog() {
	if (!d->mImageMetaInfoDialog) {
		return;
	}
	ImageMetaInfoModel* model = d->mCurrentDocument ? d->mCurrentDocument->metaInfo() : 0;
	d->mImageMetaInfoDialog->setMetaInfo(model, GwenviewConfig::fullScreenPreferredMetaInfoKeyList());
}


void FullScreenContent::showFullScreenConfigDialog() {
	FullScreenConfigDialog* dialog = new FullScreenConfigDialog(d->mOptionsButton);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->addAction(d->mSlideShow->loopAction());
	dialog->addAction(d->mSlideShow->randomAction());

	QWidget* slideShowIntervalWidget = d->createSlideShowIntervalWidget();
	dialog->addWidget(slideShowIntervalWidget);

	QAction* action = new QAction(dialog);
	action->setText(i18nc("@menu:item", "Configure displayed information"));
	dialog->addAction(action);
	QObject::connect(action, SIGNAL(triggered()),
		SLOT(configureInformationLabel()) );

	QPoint pos = d->mOptionsButton->mapToGlobal(QPoint(0, d->mOptionsButton->height()));
	dialog->move(pos);
	dialog->show();
}


} // namespace
