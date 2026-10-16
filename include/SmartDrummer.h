/*
 * SmartDrummer.h - GarageBand-style "Drummer" for the Channel Rack (Pattern Editor)
 *
 * Part of NovaBeat Studio, a fork of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#ifndef LMMS_GUI_SMART_DRUMMER_H
#define LMMS_GUI_SMART_DRUMMER_H

#include <QDialog>
#include <QPointF>
#include <QWidget>

class QCheckBox;
class QLabel;
class QListWidget;

namespace lmms
{

class InstrumentTrack;

namespace gui
{

//! Square pad like GarageBand's Drummer: x = simple..complex, y = soft..loud
class DrummerPad : public QWidget
{
	Q_OBJECT
public:
	explicit DrummerPad(QWidget* parent = nullptr);

	float complexity() const { return m_pos.x(); }
	float intensity() const { return 1.0f - m_pos.y(); }
	void setValues(float complexity, float intensity);

	QSize sizeHint() const override { return {260, 220}; }

signals:
	void changed();
	void released();

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseMoveEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;

private:
	QRectF padRect() const;
	void setFromMouse(const QPointF& p);

	QPointF m_pos{0.4, 0.4}; // normalised: x right = complex, y down = soft
};


class SmartDrummerDialog : public QDialog
{
	Q_OBJECT
public:
	explicit SmartDrummerDialog(QWidget* parent = nullptr);

	enum class Role { Kick, Snare, Clap, Hat, OpenHat, Perc, Count };

private slots:
	void generate();
	void vary();
	void styleChanged();

private:
	InstrumentTrack* findOrCreateTrack(Role role, bool create, QList<InstrumentTrack*>& used);

	QListWidget* m_styleList;
	DrummerPad* m_pad;
	QCheckBox* m_roleChecks[static_cast<int>(Role::Count)];
	QCheckBox* m_addMissing;
	QCheckBox* m_setTempo;
	QLabel* m_status;
	unsigned m_seed = 1;
};


} // namespace gui
} // namespace lmms

#endif // LMMS_GUI_SMART_DRUMMER_H
