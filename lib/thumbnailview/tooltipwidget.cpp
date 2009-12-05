// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2009 Aurélien Gâteau <agateau@kde.org>

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
#include "tooltipwidget.moc"

// Qt
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

// KDE
#include <kdebug.h>
#include <kcolorscheme.h>

// Local
#include <lib/paintutils.h>

static const int RADIUS = 5;
static const int HMARGIN = 2;

namespace Gwenview {


struct ToolTipWidgetPrivate {
	qreal mOpacity;
};


ToolTipWidget::ToolTipWidget(QWidget* parent)
: QLabel(parent)
, d(new ToolTipWidgetPrivate) {
	d->mOpacity = 1.;
	setAutoFillBackground(true);
}


ToolTipWidget::~ToolTipWidget() {
	delete d;
}


qreal ToolTipWidget::opacity() const {
	return d->mOpacity;
}


void ToolTipWidget::setOpacity(qreal opacity) {
	d->mOpacity = opacity;
	update();
}


QSize ToolTipWidget::sizeHint() const {
	QSize sh = QLabel::sizeHint();
	return QSize(sh.width() + 2 * HMARGIN, sh.height());
}


void ToolTipWidget::paintEvent(QPaintEvent*) {
	QColor bg2Color = QToolTip::palette().color(QPalette::ToolTipBase);
	QColor bg1Color = KColorScheme::shade(bg2Color, KColorScheme::LightShade, 0.2);

	QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(0.0, height()));
	gradient.setColorAt(0.0, bg1Color);
	gradient.setColorAt(1.0, bg2Color);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setOpacity(d->mOpacity);
	QPainterPath path = PaintUtils::roundedRectangle(rect(), RADIUS);
	painter.fillPath(path, gradient);
	painter.setPen(palette().color(QPalette::ToolTipText));
	painter.drawText(rect().adjusted(HMARGIN, 0, -HMARGIN, 0), text());
}


} // namespace
