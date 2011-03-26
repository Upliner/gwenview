/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <agateau@kde.org>

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
#include "documentpanel.moc"

// Qt
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QLabel>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

// KDE
#include <kactioncollection.h>
#include <kactioncategory.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <ktoggleaction.h>

// Local
#include "fileoperations.h"
#include "splitter.h"
#include <lib/binder.h>
#include <lib/document/document.h>
#include <lib/documentview/abstractdocumentviewadapter.h>
#include <lib/documentview/documentview.h>
#include <lib/documentview/documentviewcontroller.h>
#include <lib/documentview/documentviewsynchronizer.h>
#include <lib/gwenviewconfig.h>
#include <lib/hudwidget.h>
#include <lib/paintutils.h>
#include <lib/semanticinfo/sorteddirmodel.h>
#include <lib/slideshow.h>
#include <lib/statusbartoolbutton.h>
#include <lib/thumbnailview/thumbnailbarview.h>
#include <lib/widgetfloater.h>
#include <lib/zoomwidget.h>


namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif


const int DocumentPanel::MaxViewCount = 6;


static QString rgba(const QColor &color) {
	return QString::fromAscii("rgba(%1, %2, %3, %4)")
		.arg(color.red())
		.arg(color.green())
		.arg(color.blue())
		.arg(color.alpha());
}


static QString gradient(Qt::Orientation orientation, const QColor &color, int value) {
	int x2, y2;
	if (orientation == Qt::Horizontal) {
		x2 = 0;
		y2 = 1;
	} else {
		x2 = 1;
		y2 = 0;
	}
	QString grad =
		"qlineargradient(x1:0, y1:0, x2:%1, y2:%2,"
		"stop:0 %3, stop: 1 %4)";
	return grad
		.arg(x2)
		.arg(y2)
		.arg(rgba(PaintUtils::adjustedHsv(color, 0, 0, qMin(255 - color.value(), value/2))))
		.arg(rgba(PaintUtils::adjustedHsv(color, 0, 0, -qMin(color.value(), value/2))))
		;
}


/*
 * Layout of mThumbnailSplitter is:
 *
 * +-mThumbnailSplitter--------------------------------+
 * |+-mAdapterContainer-------------------------------+|
 * ||+-mDocumentViews-------++-----------------------+||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * |||                      ||                       |||
 * ||+----------------------++-----------------------+||
 * ||+-mStatusBarContainer---------------------------+||
 * |||[mToggleThumbnailBarButton]       [mZoomWidget]|||
 * ||+-----------------------------------------------+||
 * |+-------------------------------------------------+|
 * |===================================================|
 * |+-mThumbnailBar-----------------------------------+|
 * ||                                                 ||
 * ||                                                 ||
 * |+-------------------------------------------------+|
 * +---------------------------------------------------+
 */
struct DocumentPanelPrivate {
	DocumentPanel* that;
	KActionCollection* mActionCollection;
	QSplitter *mThumbnailSplitter;
	QWidget* mAdapterContainer;
	DocumentViewController* mDocumentViewController;
	QList<DocumentView*> mDocumentViews;
	QList<HudWidget*> mHuds;
	DocumentViewSynchronizer* mSynchronizer;
	QToolButton* mToggleThumbnailBarButton;
	QWidget* mDocumentViewContainer;
	QWidget* mStatusBarContainer;
	ThumbnailBarView* mThumbnailBar;
	KToggleAction* mToggleThumbnailBarAction;
    QCheckBox* mSynchronizeCheckBox;

	bool mFullScreenMode;
	QPalette mNormalPalette;
	QPalette mFullScreenPalette;
	bool mThumbnailBarVisibleBeforeFullScreen;

	void setupThumbnailBar() {
		mThumbnailBar = new ThumbnailBarView;
		ThumbnailBarItemDelegate* delegate = new ThumbnailBarItemDelegate(mThumbnailBar);
		mThumbnailBar->setItemDelegate(delegate);
		mThumbnailBar->setVisible(GwenviewConfig::thumbnailBarIsVisible());
		mThumbnailBar->setSelectionMode(QAbstractItemView::ExtendedSelection);
	}

	void setupThumbnailBarStyleSheet() {
		Qt::Orientation orientation = mThumbnailBar->orientation();
		QColor bgColor = mNormalPalette.color(QPalette::Normal, QPalette::Window);
		QColor bgSelColor = mNormalPalette.color(QPalette::Normal, QPalette::Highlight);

		// Avoid dark and bright colors
		bgColor.setHsv(bgColor.hue(), bgColor.saturation(), (127 + 3 * bgColor.value()) / 4);

		QColor leftBorderColor = PaintUtils::adjustedHsv(bgColor, 0, 0, qMin(20, 255 - bgColor.value()));
		QColor rightBorderColor = PaintUtils::adjustedHsv(bgColor, 0, 0, -qMin(40, bgColor.value()));
		QColor borderSelColor = PaintUtils::adjustedHsv(bgSelColor, 0, 0, -qMin(60, bgSelColor.value()));

		QString viewCss =
			"#thumbnailBarView {"
			"	background-color: rgba(0, 0, 0, 10%);"
			"}";

		QString itemCss =
			"QListView::item {"
			"	background-color: %1;"
			"	border-left: 1px solid %2;"
			"	border-right: 1px solid %3;"
			"}";
		itemCss = itemCss.arg(
			gradient(orientation, bgColor, 46),
			gradient(orientation, leftBorderColor, 36),
			gradient(orientation, rightBorderColor, 26));

		QString itemSelCss =
			"QListView::item:selected {"
			"	background-color: %1;"
			"	border-left: 1px solid %2;"
			"	border-right: 1px solid %2;"
			"}";
		itemSelCss = itemSelCss.arg(
			gradient(orientation, bgSelColor, 56),
			rgba(borderSelColor));

		QString css = viewCss + itemCss + itemSelCss;
		if (orientation == Qt::Vertical) {
			css.replace("left", "top").replace("right", "bottom");
		}

		mThumbnailBar->setStyleSheet(css);
	}

	void setupAdapterContainer() {
		mAdapterContainer = new QWidget;

		QVBoxLayout* layout = new QVBoxLayout(mAdapterContainer);
		layout->setMargin(0);
		layout->setSpacing(0);
		mDocumentViewContainer = new QWidget;
		mDocumentViewContainer->setAutoFillBackground(true);
		mDocumentViewContainer->setBackgroundRole(QPalette::Base);
		QHBoxLayout* viewLayout = new QHBoxLayout(mDocumentViewContainer);
		viewLayout->setMargin(0);
		viewLayout->setSpacing(0);
		Q_FOREACH(DocumentView* view, mDocumentViews) {
			viewLayout->addWidget(view);
		}
		layout->addWidget(mDocumentViewContainer);
		layout->addWidget(mStatusBarContainer);
	}

	void setupDocumentView(SlideShow* slideShow) {
		mDocumentViewController = new DocumentViewController(mActionCollection, that);

		ZoomWidget* zoomWidget = new ZoomWidget(that);
		mDocumentViewController->setZoomWidget(zoomWidget);

		for (int idx=0; idx < DocumentPanel::MaxViewCount; ++idx) {
			DocumentView* view = new DocumentView(0, mActionCollection);

			// Connect context menu
			view->setContextMenuPolicy(Qt::CustomContextMenu);
			QObject::connect(view, SIGNAL(customContextMenuRequested(const QPoint&)),
				that, SLOT(showContextMenu()) );

			QObject::connect(view, SIGNAL(completed()),
				that, SIGNAL(completed()) );
			QObject::connect(view, SIGNAL(previousImageRequested()),
				that, SIGNAL(previousImageRequested()) );
			QObject::connect(view, SIGNAL(nextImageRequested()),
				that, SIGNAL(nextImageRequested()) );
			QObject::connect(view, SIGNAL(captionUpdateRequested(const QString&)),
				that, SIGNAL(captionUpdateRequested(const QString&)) );
			QObject::connect(view, SIGNAL(toggleFullScreenRequested()),
				that, SIGNAL(toggleFullScreenRequested()) );
			QObject::connect(view, SIGNAL(clicked(DocumentView*)),
				that, SLOT(slotViewClicked(DocumentView*)) );

			QObject::connect(view, SIGNAL(videoFinished()),
				slideShow, SLOT(resumeAndGoToNextUrl()));

			mDocumentViews << view;
		}

		mSynchronizer = new DocumentViewSynchronizer(that);
	}

	QToolButton* createHudButton(const QString& text, const char* iconName, bool showText) {
		QToolButton* button = new QToolButton;
		if (showText) {
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setText(text);
		} else {
			button->setToolTip(text);
		}
		button->setIcon(SmallIcon(iconName));
		return button;
	}

	void setupViewHud(DocumentView* view) {
		QToolButton* trashButton = createHudButton(i18n("Trash"), "user-trash", true);

		QWidget* content = new QWidget;
		QHBoxLayout* layout = new QHBoxLayout(content);
		const int space = 4;
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(trashButton);
		layout->addSpacing(space);

		HudWidget* hud = new HudWidget;
		hud->init(content, HudWidget::OptionCloseButton);
		WidgetFloater* floater = new WidgetFloater(view);
		floater->setChildWidget(hud);
		floater->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);

		Binder<DocumentPanel, DocumentView*>::bind(trashButton, SIGNAL(clicked()), that, &DocumentPanel::trashView, view);
		Binder<DocumentPanel, DocumentView*>::bind(hud, SIGNAL(closed()), that, &DocumentPanel::deselectView, view);

		mHuds.append(hud);
	}

	void setupHuds() {
		Q_FOREACH(DocumentView* view, mDocumentViews) {
			setupViewHud(view);
		}
	}

	void setupStatusBar() {
		mStatusBarContainer = new QWidget;
		mToggleThumbnailBarButton = new StatusBarToolButton;
		mSynchronizeCheckBox = new QCheckBox(i18n("Synchronize"));
		mSynchronizeCheckBox->hide();

		QHBoxLayout* layout = new QHBoxLayout(mStatusBarContainer);
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(mToggleThumbnailBarButton);
		layout->addStretch();
		layout->addWidget(mSynchronizeCheckBox);
		layout->addStretch();
		layout->addWidget(mDocumentViewController->zoomWidget());

		QObject::connect(mSynchronizeCheckBox, SIGNAL(toggled(bool)), mSynchronizer, SLOT(setActive(bool)));
	}

	void setupSplitter() {
		Qt::Orientation orientation = GwenviewConfig::thumbnailBarOrientation();
		mThumbnailSplitter = new Splitter(orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal, that);
		mThumbnailBar->setOrientation(orientation);
		mThumbnailBar->setRowCount(GwenviewConfig::thumbnailBarRowCount());
		mThumbnailSplitter->addWidget(mAdapterContainer);
		mThumbnailSplitter->addWidget(mThumbnailBar);
		mThumbnailSplitter->setSizes(GwenviewConfig::thumbnailSplitterSizes());

		QVBoxLayout* layout = new QVBoxLayout(that);
		layout->setMargin(0);
		layout->addWidget(mThumbnailSplitter);
	}

	void applyPalette() {
		QPalette palette = mFullScreenMode ? mFullScreenPalette : mNormalPalette;
		mDocumentViewContainer->setPalette(palette);
	}

	void saveSplitterConfig() {
		if (mThumbnailBar->isVisible()) {
			GwenviewConfig::setThumbnailSplitterSizes(mThumbnailSplitter->sizes());
		}
	}

	DocumentView* currentView() const {
		return mDocumentViewController->view();
	}

	void setCurrentView(DocumentView* view) {
		DocumentView* oldView = currentView();
		if (view == oldView) {
			return;
		}
		if (oldView) {
			oldView->setCurrent(false);
		}
		view->setCurrent(true);
		mDocumentViewController->setView(view);
		mSynchronizer->setCurrentView(view);

		QModelIndex index = indexForView(view);
		if (!index.isValid()) {
			kWarning() << "No index found for current view";
			return;
		}
		mThumbnailBar->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Current);
	}

	QModelIndex indexForView(DocumentView* view) const {
		Document::Ptr doc = view->adapter()->document();
		Q_ASSERT(doc);
		if (!doc) {
			kWarning() << "No document!";
			return QModelIndex();
		}

		// FIXME: Ugly coupling!
		SortedDirModel* model = static_cast<SortedDirModel*>(mThumbnailBar->model());
		return model->indexForUrl(doc->url());
	}
};


DocumentPanel::DocumentPanel(QWidget* parent, SlideShow* slideShow, KActionCollection* actionCollection)
: QWidget(parent)
, d(new DocumentPanelPrivate)
{
	d->that = this;
	d->mActionCollection = actionCollection;
	d->mFullScreenMode = false;
	d->mThumbnailBarVisibleBeforeFullScreen = false;
	d->mFullScreenPalette = QPalette(palette());
	d->mFullScreenPalette.setColor(QPalette::Base, Qt::black);
	d->mFullScreenPalette.setColor(QPalette::Text, Qt::white);

	QShortcut* toggleFullScreenShortcut = new QShortcut(this);
	toggleFullScreenShortcut->setKey(Qt::Key_Return);
	connect(toggleFullScreenShortcut, SIGNAL(activated()), SIGNAL(toggleFullScreenRequested()) );

	d->setupDocumentView(slideShow);

	d->setupHuds();

	d->setupStatusBar();

	d->setupAdapterContainer();

	d->setupThumbnailBar();

	d->setupSplitter();

	KActionCategory* view = new KActionCategory(i18nc("@title actions category - means actions changing smth in interface","View"), actionCollection);

	d->mToggleThumbnailBarAction = view->add<KToggleAction>(QString("toggle_thumbnailbar"));
	d->mToggleThumbnailBarAction->setText(i18n("Thumbnail Bar"));
	d->mToggleThumbnailBarAction->setIcon(KIcon("folder-image"));
	d->mToggleThumbnailBarAction->setShortcut(Qt::CTRL | Qt::Key_B);
	d->mToggleThumbnailBarAction->setChecked(GwenviewConfig::thumbnailBarIsVisible());
	connect(d->mToggleThumbnailBarAction, SIGNAL(triggered(bool)),
		this, SLOT(setThumbnailBarVisibility(bool)) );
	d->mToggleThumbnailBarButton->setDefaultAction(d->mToggleThumbnailBarAction);
}


DocumentPanel::~DocumentPanel() {
	delete d;
}


void DocumentPanel::loadConfig() {
	// FIXME: Not symetric with saveConfig(). Check if it matters.
	Q_FOREACH(DocumentView* view, d->mDocumentViews) {
		if (view->adapter()) {
			view->adapter()->loadConfig();
		}
	}

	Qt::Orientation orientation = GwenviewConfig::thumbnailBarOrientation();
	d->mThumbnailSplitter->setOrientation(orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
	d->mThumbnailBar->setOrientation(orientation);
	d->setupThumbnailBarStyleSheet();

	int oldRowCount = d->mThumbnailBar->rowCount();
	int newRowCount = GwenviewConfig::thumbnailBarRowCount();
	if (oldRowCount != newRowCount) {
		d->mThumbnailBar->setUpdatesEnabled(false);
		int gridSize = d->mThumbnailBar->gridSize().width();

		d->mThumbnailBar->setRowCount(newRowCount);

		// Adjust splitter to ensure thumbnail size remains the same
		int delta = (newRowCount - oldRowCount) * gridSize;
		QList<int> sizes = d->mThumbnailSplitter->sizes();
		Q_ASSERT(sizes.count() == 2);
		sizes[0] -= delta;
		sizes[1] += delta;
		d->mThumbnailSplitter->setSizes(sizes);

		d->mThumbnailBar->setUpdatesEnabled(true);
	}
}


void DocumentPanel::saveConfig() {
	d->saveSplitterConfig();
	GwenviewConfig::setThumbnailBarIsVisible(d->mToggleThumbnailBarAction->isChecked());
}


void DocumentPanel::setThumbnailBarVisibility(bool visible) {
	d->saveSplitterConfig();
	d->mThumbnailBar->setVisible(visible);
}


int DocumentPanel::statusBarHeight() const {
	return d->mStatusBarContainer->height();
}


void DocumentPanel::setFullScreenMode(bool fullScreenMode) {
	d->mFullScreenMode = fullScreenMode;
	d->mStatusBarContainer->setVisible(!fullScreenMode);
	d->applyPalette();
	if (fullScreenMode) {
		d->mThumbnailBarVisibleBeforeFullScreen = d->mToggleThumbnailBarAction->isChecked();
		if (d->mThumbnailBarVisibleBeforeFullScreen) {
			d->mToggleThumbnailBarAction->trigger();
		}
	} else if (d->mThumbnailBarVisibleBeforeFullScreen) {
		d->mToggleThumbnailBarAction->trigger();
	}
	d->mToggleThumbnailBarAction->setEnabled(!fullScreenMode);
}


bool DocumentPanel::isFullScreenMode() const {
	return d->mFullScreenMode;
}


ThumbnailBarView* DocumentPanel::thumbnailBar() const {
	return d->mThumbnailBar;
}


inline void addActionToMenu(KMenu* menu, KActionCollection* actionCollection, const char* name) {
	QAction* action = actionCollection->action(name);
	if (action) {
		menu->addAction(action);
	}
}


void DocumentPanel::showContextMenu() {
	KMenu menu(this);
	addActionToMenu(&menu, d->mActionCollection, "fullscreen");
	menu.addSeparator();
	addActionToMenu(&menu, d->mActionCollection, "go_previous");
	addActionToMenu(&menu, d->mActionCollection, "go_next");
	if (d->currentView()->adapter()->canZoom()) {
		menu.addSeparator();
		addActionToMenu(&menu, d->mActionCollection, "view_actual_size");
		addActionToMenu(&menu, d->mActionCollection, "view_zoom_to_fit");
		addActionToMenu(&menu, d->mActionCollection, "view_zoom_in");
		addActionToMenu(&menu, d->mActionCollection, "view_zoom_out");
	}
	menu.addSeparator();
	addActionToMenu(&menu, d->mActionCollection, "file_copy_to");
	addActionToMenu(&menu, d->mActionCollection, "file_move_to");
	addActionToMenu(&menu, d->mActionCollection, "file_link_to");
	menu.addSeparator();
	addActionToMenu(&menu, d->mActionCollection, "file_open_with");
	menu.exec(QCursor::pos());
}


QSize DocumentPanel::sizeHint() const {
	return QSize(400, 300);
}


KUrl DocumentPanel::url() const {
	if (!d->currentView()) {
		LOG("!d->documentView()");
		return KUrl();
	}

	if (!d->currentView()->adapter()->document()) {
		LOG("!d->documentView()->adapter()->document()");
		return KUrl();
	}

	return d->currentView()->adapter()->document()->url();
}


Document::Ptr DocumentPanel::currentDocument() const {
	if (!d->currentView()) {
		return Document::Ptr();
	}

	return d->currentView()->adapter()->document();
}


bool DocumentPanel::isEmpty() const {
	if (!d->currentView()) {
		return true;
	}
	return d->currentView()->isEmpty();
}


ImageView* DocumentPanel::imageView() const {
	return d->currentView()->adapter()->imageView();
}


DocumentView* DocumentPanel::documentView() const {
	return d->currentView();
}


void DocumentPanel::setNormalPalette(const QPalette& palette) {
	d->mNormalPalette = palette;
	d->applyPalette();
	d->setupThumbnailBarStyleSheet();
}


void DocumentPanel::openUrl(const KUrl& url) {
	openUrls(KUrl::List() << url, url);
}


void DocumentPanel::openUrls(const KUrl::List& urls, const KUrl& currentUrl) {
	bool compareMode = urls.count() > 1;
	QList<DocumentView*> visibleViews;

	// Get a list of available views and urls we are not already displaying
	QSet<KUrl> notDisplayedUrls = urls.toSet();
	QList<DocumentView*> availableViews;
	Q_FOREACH(DocumentView* view, d->mDocumentViews) {
		KUrl url = view->url();
		if (notDisplayedUrls.contains(url)) {
			notDisplayedUrls.remove(url);
			view->setCompareMode(compareMode);
			if (url == currentUrl) {
				d->setCurrentView(view);
			} else {
				view->setCurrent(false);
			}
			visibleViews.append(view);
		} else {
			view->reset();
			view->hide();
			availableViews.append(view);
		}
	}

	// Show urls to display in available views
	Q_FOREACH(const KUrl& url, notDisplayedUrls) {
		if (availableViews.isEmpty()) {
			kWarning() << "No room to load" << url << ". This should not happen";
			break;
		}
		DocumentView* view = availableViews.takeFirst();
		view->openUrl(url);
		view->setCompareMode(compareMode);
		if (url == currentUrl) {
			d->setCurrentView(view);
		} else {
			view->setCurrent(false);
		}
		visibleViews.append(view);
		view->show();
	}

	Q_FOREACH(HudWidget* hud, d->mHuds) {
		hud->setVisible(compareMode);
		if (compareMode) {
			hud->raise();
		}
	}
	d->mSynchronizeCheckBox->setVisible(compareMode);
	if (compareMode) {
		d->mSynchronizer->setDocumentViews(visibleViews);
		d->mSynchronizer->setActive(d->mSynchronizeCheckBox->isChecked());
	} else {
		d->mSynchronizer->setActive(false);
	}
}


void DocumentPanel::reload() {
	if (!d->currentView()->adapter()) {
		return;
	}
	Document::Ptr doc = d->currentView()->adapter()->document();
	if (!doc) {
		kWarning() << "!doc";
		return;
	}
	if (doc->isModified()) {
		KGuiItem cont = KStandardGuiItem::cont();
		cont.setText(i18nc("@action:button", "Discard Changes and Reload"));
		int answer = KMessageBox::warningContinueCancel(this,
			i18nc("@info", "This image has been modified. Reloading it will discard all your changes."),
			QString() /* caption */,
			cont);
		if (answer != KMessageBox::Continue) {
			return;
		}
	}
	doc->reload();
	// Call openUrl again because DocumentView may need to switch to a new
	// adapter (for example because document was broken and it is not anymore)
	d->currentView()->openUrl(doc->url());
}


void DocumentPanel::reset() {
	int idx = 0;
	Q_FOREACH(DocumentView* view, d->mDocumentViews) {
		view->reset();
		if (idx > 0) {
			view->hide();
		}
		++idx;
	}
}

void DocumentPanel::slotViewClicked(DocumentView* view) {
	d->setCurrentView(view);
}


void DocumentPanel::trashView(DocumentView* view) {
	KUrl url = view->url();
	deselectView(view);
	FileOperations::trash(KUrl::List() << url, this);
}


void DocumentPanel::deselectView(DocumentView* view) {
	DocumentView* newCurrentView = 0;
	if (view == d->currentView()) {
		int idx = d->mDocumentViews.indexOf(view);
		if (idx < d->mDocumentViews.count() - 1 && d->mDocumentViews.at(idx + 1)->isVisible()) {
			++idx;
		} else {
			--idx;
		}
		newCurrentView = d->mDocumentViews.at(idx);
	}

	QModelIndex index = d->indexForView(view);
	QItemSelectionModel* selectionModel = d->mThumbnailBar->selectionModel();
	selectionModel->select(index, QItemSelectionModel::Deselect);

	if (newCurrentView) {
		d->setCurrentView(newCurrentView);
	}
}


} // namespace
