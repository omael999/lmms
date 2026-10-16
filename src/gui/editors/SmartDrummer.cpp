/*
 * SmartDrummer.cpp - GarageBand-style "Drummer" for the Channel Rack (Pattern Editor)
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

#include "SmartDrummer.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <random>

#include "ConfigManager.h"
#include "DeprecationHelper.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "PatternEditor.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "MidiClip.h"
#include "Note.h"
#include "PatternStore.h"
#include "Song.h"
#include "TimePos.h"

namespace lmms::gui
{

namespace
{

using Role = SmartDrummerDialog::Role;
constexpr int RoleCount = static_cast<int>(Role::Count);

// One bar of 16 steps per role.  'x' = accent, 'o' = normal hit, '.' = ghost note, '-' = rest.
// "base" hits are always played; "extra" hits fade in as complexity goes up.
struct Part { const char* base; const char* extra; };

struct Style
{
	const char* name;        // translated via QCoreApplication::translate
	const char* description;
	int tempo;
	std::array<Part, RoleCount> parts; // Kick, Snare, Clap, Hat, OpenHat, Perc
};

const char* const R = "----------------";

const std::array<Style, 8> Styles = {{
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Pop"), QT_TRANSLATE_NOOP("SmartDrummer", "Steady, radio-friendly groove"), 110, {{
		{ "x-------x-------", "-------o--o---o-" },
		{ "----x-------x---", "-------.-.----.." },
		{ R,                  "----o-------o---" },
		{ "o-o-o-o-o-o-o-o-", "-.-.-.-.-.-.-.-." },
		{ R,                  "--------------o-" },
		{ R,                  "--o---o---o---o-" } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Rock"), QT_TRANSLATE_NOOP("SmartDrummer", "Driving straight eighths"), 120, {{
		{ "x-----o-x-o-----", "---o-------o--o-" },
		{ "----x-------x---", "-------.------.." },
		{ R,                  R },
		{ "x-o-x-o-x-o-x-o-", "-.---.---.---.--" },
		{ R,                  "--------------o-" },
		{ R,                  R } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Hip-Hop"), QT_TRANSLATE_NOOP("SmartDrummer", "Boom-bap with a lazy snare"), 90, {{
		{ "x------o--x-----", "--o--------o---o" },
		{ "----x-------x---", ".------.--.----." },
		{ R,                  "------------o---" },
		{ "o-o-o-o-o-o-o-o-", "-.-.-.-.-.-.-.-." },
		{ R,                  "------o---------" },
		{ R,                  "---o-----o-----o" } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Trap"), QT_TRANSLATE_NOOP("SmartDrummer", "Half-time clap and busy hi-hats"), 140, {{
		{ "x------x--x-----", "-----o-----o--o-" },
		{ R,                  "-----------.---o" },
		{ "--------x-------", "---------------o" },
		{ "o-o-o-o-o-o-o-o-", "-.-o-.-o-.-o-o.o" },
		{ R,                  "------o-------o-" },
		{ R,                  "---.-----.------" } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Reggaeton"), QT_TRANSLATE_NOOP("SmartDrummer", "Classic dembow"), 95, {{
		{ "x---x---x---x---", R },
		{ "---o--o----o--o-", "-------.-------." },
		{ R,                  "------o-------o-" },
		{ "o-o-o-o-o-o-o-o-", "-.-.-.-.-.-.-.-." },
		{ R,                  "--o-------o-----" },
		{ R,                  "o.o.o.o.o.o.o.o." } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "House"), QT_TRANSLATE_NOOP("SmartDrummer", "Four on the floor, open hats on the offbeat"), 124, {{
		{ "x---x---x---x---", R },
		{ R,                  "-------.------.-" },
		{ "----x-------x---", R },
		{ R,                  "o.-.o.-.o.-.o.-." },
		{ "--o---o---o---o-", R },
		{ R,                  "---.---.---.---." } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Funk"), QT_TRANSLATE_NOOP("SmartDrummer", "Syncopated kick and ghost notes"), 105, {{
		{ "x-----o---o--o--", "--o------x------" },
		{ "----x-------x---", ".--.-.-..--.-..." },
		{ R,                  R },
		{ "xoxoxoxoxoxoxoxo", R },
		{ R,                  "--------------o-" },
		{ R,                  "---o-----o------" } }} },
	{ QT_TRANSLATE_NOOP("SmartDrummer", "Lo-Fi"), QT_TRANSLATE_NOOP("SmartDrummer", "Soft, laid-back beat"), 80, {{
		{ "x------o--o-----", "---o------------" },
		{ "----o-------o---", "-------.-------." },
		{ R,                  R },
		{ "o-.-o-.-o-.-o-.-", "-.-.-.-.-.-.-.-." },
		{ R,                  R },
		{ R,                  "--.-----.---.---" } }} },
}};

struct RoleInfo
{
	const char* trackName;
	const char* sample;      // relative to the factory samples dir
	QStringList keywords;    // lower-case substrings that identify an existing channel
};

const std::array<RoleInfo, RoleCount>& roleInfo()
{
	static const std::array<RoleInfo, RoleCount> info = {{
		{ "Kick",     "drums/kick01.ogg",          { "kick", "bombo", "bassdrum", "bass drum", "bd" } },
		{ "Snare",    "drums/snare01.ogg",         { "snare", "caja", "snr", "sd", "rim" } },
		{ "Clap",     "drums/clap01.ogg",          { "clap", "palma" } },
		{ "Hat",      "drums/hihat_closed01.ogg",  { "hat", "hh", "charles" } },
		{ "Open Hat", "drums/hihat_opened01.ogg",  { "open", "hat_o", "oh" } },
		{ "Shaker",   "drums/shaker01.ogg",        { "shaker", "perc", "conga", "tom", "cowbell", "clave", "guiro", "tamb" } },
	}};
	return info;
}

bool styleUsesRole(const Style& s, int role)
{
	return QString(s.parts[role].base) != R || QString(s.parts[role].extra) != R;
}

int baseVolume(char c)
{
	switch (c)
	{
		case 'x': return 118;
		case 'o': return 92;
		case '.': return 50;
		default: return 0;
	}
}

} // namespace




// ---------------------------------------------------------------------------
// DrummerPad
// ---------------------------------------------------------------------------

DrummerPad::DrummerPad(QWidget* parent) : QWidget(parent)
{
	setMinimumSize(220, 190);
	setCursor(Qt::PointingHandCursor);
}

void DrummerPad::setValues(float complexity, float intensity)
{
	m_pos = QPointF(std::clamp(complexity, 0.f, 1.f), std::clamp(1.f - intensity, 0.f, 1.f));
	update();
}

QRectF DrummerPad::padRect() const
{
	return QRectF(rect()).adjusted(24, 18, -10, -24);
}

void DrummerPad::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r = padRect();

	QLinearGradient bg(r.topLeft(), r.bottomRight());
	bg.setColorAt(0, QColor("#3a4449"));
	bg.setColorAt(1, QColor("#262d31"));
	p.setPen(QPen(QColor("#1b2023"), 1));
	p.setBrush(bg);
	p.drawRoundedRect(r, 8, 8);

	p.setPen(QPen(QColor(255, 255, 255, 22), 1));
	for (int i = 1; i < 4; ++i)
	{
		const qreal x = r.left() + r.width() * i / 4.0;
		const qreal y = r.top() + r.height() * i / 4.0;
		p.drawLine(QPointF(x, r.top() + 4), QPointF(x, r.bottom() - 4));
		p.drawLine(QPointF(r.left() + 4, y), QPointF(r.right() - 4, y));
	}

	p.setPen(QColor("#9aa5ad"));
	QFont f = font();
	f.setPointSizeF(std::max(7.0, f.pointSizeF() - 1));
	p.setFont(f);
	p.drawText(QRectF(r.left(), 0, r.width(), r.top()), Qt::AlignCenter, tr("Loud"));
	p.drawText(QRectF(r.left(), r.bottom(), r.width(), height() - r.bottom()), Qt::AlignCenter, tr("Soft"));
	p.save();
	p.translate(12, r.center().y());
	p.rotate(-90);
	p.drawText(QRectF(-r.height() / 2, -10, r.height(), 20), Qt::AlignCenter, tr("Simple"));
	p.restore();
	p.drawText(QRectF(r.right() - 70, r.bottom() - 18, 66, 16), Qt::AlignRight | Qt::AlignVCenter, tr("Complex"));

	const QPointF c(r.left() + m_pos.x() * r.width(), r.top() + m_pos.y() * r.height());
	QRadialGradient glow(c, 26);
	glow.setColorAt(0, QColor(255, 159, 46, 120));
	glow.setColorAt(1, QColor(255, 159, 46, 0));
	p.setPen(Qt::NoPen);
	p.setBrush(glow);
	p.drawEllipse(c, 26, 26);
	p.setBrush(QColor("#ffb347"));
	p.setPen(QPen(QColor("#5a3208"), 1.5));
	p.drawEllipse(c, 9, 9);
}

void DrummerPad::setFromMouse(const QPointF& pt)
{
	const QRectF r = padRect();
	m_pos = QPointF(std::clamp((pt.x() - r.left()) / r.width(), 0.0, 1.0),
		std::clamp((pt.y() - r.top()) / r.height(), 0.0, 1.0));
	update();
	emit changed();
}

void DrummerPad::mousePressEvent(QMouseEvent* e) { setFromMouse(position(e)); }
void DrummerPad::mouseMoveEvent(QMouseEvent* e) { if (e->buttons() & Qt::LeftButton) { setFromMouse(position(e)); } }
void DrummerPad::mouseReleaseEvent(QMouseEvent*) { emit released(); }




// ---------------------------------------------------------------------------
// SmartDrummerDialog
// ---------------------------------------------------------------------------

SmartDrummerDialog::SmartDrummerDialog(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Smart Drummer"));
	setObjectName("smartDrummer");

	auto intro = new QLabel(tr("Pick a style and drag the puck: left/right for simple/complex, "
		"up/down for loud/soft. The beat is written into the current pattern of the Channel Rack."), this);
	intro->setWordWrap(true);

	m_styleList = new QListWidget(this);
	for (const auto& s : Styles)
	{
		auto item = new QListWidgetItem(QCoreApplication::translate("SmartDrummer", s.name), m_styleList);
		item->setToolTip(QCoreApplication::translate("SmartDrummer", s.description));
	}
	m_styleList->setCurrentRow(std::clamp(ConfigManager::inst()->value("smartdrummer", "style").toInt(), 0,
		static_cast<int>(Styles.size()) - 1));
	m_styleList->setMinimumWidth(150);

	m_pad = new DrummerPad(this);

	auto rolesBox = new QWidget(this);
	auto rolesLayout = new QGridLayout(rolesBox);
	rolesLayout->setContentsMargins(0, 0, 0, 0);
	const QString roleLabels[RoleCount] = { tr("Kick"), tr("Snare"), tr("Clap"), tr("Hi-hat"), tr("Open hat"), tr("Percussion") };
	for (int i = 0; i < RoleCount; ++i)
	{
		m_roleChecks[i] = new QCheckBox(roleLabels[i], rolesBox);
		m_roleChecks[i]->setChecked(true);
		rolesLayout->addWidget(m_roleChecks[i], i / 3, i % 3);
		connect(m_roleChecks[i], &QCheckBox::toggled, this, &SmartDrummerDialog::generate);
	}

	m_addMissing = new QCheckBox(tr("Add missing drum channels automatically"), this);
	m_addMissing->setChecked(true);
	m_setTempo = new QCheckBox(tr("Use the style's typical tempo"), this);
	m_setTempo->setChecked(false);
	m_status = new QLabel(this);
	m_status->setWordWrap(true);

	auto playButton = new QPushButton(tr("Play / Stop"), this);
	connect(playButton, &QPushButton::clicked, this, []() { getGUI()->patternEditor()->togglePlayStop(); });
	auto varyButton = new QPushButton(tr("New variation"), this);
	auto generateButton = new QPushButton(tr("Write beat"), this);
	generateButton->setDefault(true);
	auto closeButton = new QPushButton(tr("Close"), this);
	connect(varyButton, &QPushButton::clicked, this, &SmartDrummerDialog::vary);
	connect(generateButton, &QPushButton::clicked, this, &SmartDrummerDialog::generate);
	connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

	connect(m_styleList, &QListWidget::currentRowChanged, this, &SmartDrummerDialog::styleChanged);
	connect(m_pad, &DrummerPad::released, this, &SmartDrummerDialog::generate);

	auto top = new QHBoxLayout;
	top->addWidget(m_styleList);
	top->addWidget(m_pad, 1);

	auto buttons = new QHBoxLayout;
	buttons->addWidget(playButton);
	buttons->addWidget(varyButton);
	buttons->addStretch(1);
	buttons->addWidget(generateButton);
	buttons->addWidget(closeButton);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(intro);
	layout->addLayout(top, 1);
	layout->addWidget(rolesBox);
	layout->addWidget(m_addMissing);
	layout->addWidget(m_setTempo);
	layout->addWidget(m_status);
	layout->addLayout(buttons);

	m_pad->setValues(ConfigManager::inst()->value("smartdrummer", "complexity", "0.4").toFloat(),
		ConfigManager::inst()->value("smartdrummer", "intensity", "0.6").toFloat());
	resize(560, 460);
}




void SmartDrummerDialog::styleChanged()
{
	ConfigManager::inst()->setValue("smartdrummer", "style", QString::number(m_styleList->currentRow()));
	generate();
}




void SmartDrummerDialog::vary()
{
	++m_seed;
	generate();
}




InstrumentTrack* SmartDrummerDialog::findOrCreateTrack(Role role, bool create, QList<InstrumentTrack*>& used)
{
	const auto& info = roleInfo()[static_cast<int>(role)];
	PatternStore* store = Engine::patternStore();

	for (Track* t : store->tracks())
	{
		auto it = dynamic_cast<InstrumentTrack*>(t);
		if (!it || used.contains(it)) { continue; }
		const QString name = it->name().toLower();
		// "Open hat" must not be taken by the closed hi-hat role and vice versa
		if (role == Role::Hat && (name.contains("open") || name.contains("hat_o"))) { continue; }
		for (const QString& kw : info.keywords)
		{
			const bool shortKw = kw.size() <= 2;
			const bool match = shortKw ? name.split(QRegularExpression("[^a-z0-9]+")).contains(kw) : name.contains(kw);
			if (match)
			{
				used.append(it);
				return it;
			}
		}
	}

	if (!create) { return nullptr; }

	auto it = dynamic_cast<InstrumentTrack*>(Track::create(Track::Type::Instrument, store));
	if (!it) { return nullptr; }
	if (Instrument* inst = it->loadInstrument("audiofileprocessor"))
	{
		inst->loadFile(ConfigManager::inst()->factorySamplesDir() + info.sample);
	}
	it->setName(info.trackName);
	used.append(it);
	return it;
}




void SmartDrummerDialog::generate()
{
	const int styleIndex = std::max(0, m_styleList->currentRow());
	const Style& style = Styles[styleIndex];
	const float complexity = m_pad->complexity();
	const float intensity = m_pad->intensity();
	ConfigManager::inst()->setValue("smartdrummer", "complexity", QString::number(complexity));
	ConfigManager::inst()->setValue("smartdrummer", "intensity", QString::number(intensity));

	PatternStore* store = Engine::patternStore();
	const int pattern = store->currentPattern();
	const float volScale = 0.55f + 0.75f * intensity;

	QList<InstrumentTrack*> used;
	int written = 0;
	QStringList created;
	const int tracksBefore = static_cast<int>(store->tracks().size());

	for (int role = 0; role < RoleCount; ++role)
	{
		if (!m_roleChecks[role]->isChecked() || !styleUsesRole(style, role)) { continue; }
		InstrumentTrack* track = findOrCreateTrack(static_cast<Role>(role), m_addMissing->isChecked(), used);
		if (!track) { continue; }

		auto clip = dynamic_cast<MidiClip*>(track->getClip(pattern));
		if (!clip) { continue; }

		// Deterministic randomness per (seed, style, role) so "New variation" changes the groove
		std::mt19937 rng(m_seed * 7919u + styleIndex * 131u + role * 17u);
		std::uniform_real_distribution<float> chance(0.f, 1.f);
		std::uniform_int_distribution<int> humanize(-6, 6);

		const int steps = std::max(16, clip->steps());
		const Part& part = style.parts[role];

		clip->clearNotes();
		for (int s = 0; s < steps; ++s)
		{
			const int i = s % 16;
			char c = part.base[i];
			const float roll = chance(rng);
			if (c == '-' && part.extra[i] != '-' && roll < complexity) { c = part.extra[i]; }

			// Fill on the last beat of a multi-bar pattern
			const bool lastBeat = steps > 16 && s >= steps - 4;
			if (lastBeat && complexity > 0.45f)
			{
				const int k = s - (steps - 4);
				if (role == static_cast<int>(Role::Snare)) { c = (k == 0 && complexity < 0.7f) ? '-' : (k == 3 ? 'x' : 'o'); }
				if (role == static_cast<int>(Role::Hat) || role == static_cast<int>(Role::OpenHat)) { c = '-'; }
			}

			const int base = baseVolume(c);
			if (base == 0) { continue; }
			Note* n = clip->addStepNote(s);
			if (n)
			{
				const int vol = std::clamp(static_cast<int>(base * volScale) + humanize(rng), 10, 200);
				n->setVolume(static_cast<volume_t>(vol));
				++written;
			}
		}
		emit clip->dataChanged();
	}

	for (int i = tracksBefore; i < static_cast<int>(store->tracks().size()); ++i)
	{
		created << store->tracks()[i]->name();
	}

	if (m_setTempo->isChecked())
	{
		Engine::getSong()->tempoModel().setValue(style.tempo);
	}
	Engine::getSong()->setModified();

	QString msg = tr("%1: %2 hits written into pattern %3.")
		.arg(QCoreApplication::translate("SmartDrummer", style.name)).arg(written).arg(pattern + 1);
	if (!created.isEmpty()) { msg += " " + tr("New channels: %1.").arg(created.join(", ")); }
	m_status->setText(msg);
}


} // namespace lmms::gui
