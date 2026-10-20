/*
 * PatternEditor.cpp - basic main-window for editing patterns
 *
 * Copyright (c) 2004-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
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

#include "PatternEditor.h"
#include "MixerView.h"
#include "Mixer.h"
#include "Knob.h"
#include "Instrument.h"
#include "ConfigManager.h"
#include <QLabel>
#include <QFileInfo>
#include <QFileDialog>
#include "SmartDrummer.h"
#include "PluginFactory.h"
#include "InstrumentTrack.h"
#include <QToolButton>
#include <QMenu>
#include <QCoreApplication>
#include <algorithm>

#include <QAction>
#include <QVBoxLayout>

#include "ClipView.h"
#include "ComboBox.h"
#include "DataFile.h"
#include "embed.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "PatternStore.h"
#include "PatternTrack.h"
#include "Song.h"
#include "StringPairDrag.h"
#include "TimeLineWidget.h"
#include "TrackView.h"

#include "MidiClip.h"


namespace lmms::gui
{


PatternEditor::PatternEditor(PatternStore* ps) :
	TrackContainerView(ps),
	m_ps(ps),
	m_trackHeadWidth(ConfigManager::inst()->value("ui", "compacttrackbuttons").toInt() == 1
		? DEFAULT_SETTINGS_WIDGET_WIDTH_COMPACT + TRACK_OP_WIDTH_COMPACT
		: DEFAULT_SETTINGS_WIDGET_WIDTH + TRACK_OP_WIDTH),
	m_maxClipLength(TimePos::ticksPerBar())
{
	setModel(ps);

	m_timeLine = new TimeLineWidget(m_trackHeadWidth, 32, pixelsPerBar(),
		Engine::getSong()->getTimeline(Song::PlayMode::Pattern),
		m_currentPosition, this
	);
	connect(m_timeLine->timeline(), &Timeline::positionChanged, this, &PatternEditor::updatePosition);
	static_cast<QVBoxLayout*>(layout())->insertWidget(0, m_timeLine);

	connect(m_ps, &PatternStore::trackUpdated,
		this, &PatternEditor::updateMaxSteps);

	setFocusPolicy(Qt::StrongFocus);
	setFocus();
}




void PatternEditor::addSteps()
{
	makeSteps( false );
}

void PatternEditor::cloneSteps()
{
	makeSteps( true );
}




void PatternEditor::removeSteps()
{
	const TrackContainer::TrackList& tl = model()->tracks();

	for (const auto& track : tl)
	{
		if (track->type() == Track::Type::Instrument)
		{
			auto p = static_cast<MidiClip*>(track->getClip(m_ps->currentPattern()));
			p->removeSteps();
		}
	}
	updateMaxSteps();
}




void PatternEditor::routeToNewMixerChannel(InstrumentTrack* track)
{
	if (!track || !getGUI() || !getGUI()->mixerView()) { return; }
	const int index = getGUI()->mixerView()->addNewChannel();
	if (index <= 0) { return; }
	Engine::mixer()->mixerChannel(index)->m_name = track->name();
	track->mixerChannelModel()->setValue(index);
	getGUI()->mixerView()->updateMixerChannel(index);
}




int PatternEditor::currentSteps() const
{
	const tick_t ticksPerStep = TimePos::stepPosition(1);
	const tick_t length = m_maxClipLength > 0 ? m_maxClipLength : TimePos::ticksPerBar();
	return static_cast<int>(length / std::max<tick_t>(1, ticksPerStep));
}




void PatternEditor::clearCurrentPattern()
{
	for (const auto& track : model()->tracks())
	{
		if (track->type() != Track::Type::Instrument) { continue; }
		auto clip = dynamic_cast<MidiClip*>(track->getClip(m_ps->currentPattern()));
		if (clip && !clip->notes().empty()) { clip->clear(); }
	}
	Engine::getSong()->setModified();
}




void PatternEditor::addSampleTrack()
{
	(void) Track::create( Track::Type::Sample, model() );
}




void PatternEditor::addAutomationTrack()
{
	(void) Track::create( Track::Type::Automation, model() );
}




void PatternEditor::removeViewsForPattern(int pattern)
{
	for( TrackView* view : trackViews() )
	{
		view->getTrackContentWidget()->removeClipView(pattern);
	}
}



void PatternEditor::saveSettings(QDomDocument& doc, QDomElement& element)
{
	MainWindow::saveWidgetState( parentWidget(), element );
}

void PatternEditor::loadSettings(const QDomElement& element)
{
	MainWindow::restoreWidgetState(parentWidget(), element);
	updateMaxSteps();
}




void PatternEditor::dropEvent(QDropEvent* de)
{
	QString type = StringPairDrag::decodeKey( de );
	QString value = StringPairDrag::decodeValue( de );

	if( type.left( 6 ) == "track_" )
	{
		DataFile dataFile( value.toUtf8() );
		Track * t = Track::create( dataFile.content().firstChild().toElement(), model() );

		// Ensure pattern clips exist
		bool hasValidPatternClips = false;
		if (t->getClips().size() == static_cast<std::size_t>(m_ps->numOfPatterns()))
		{
			hasValidPatternClips = true;
			for (auto i = std::size_t{0}; i < t->getClips().size(); ++i)
			{
				if (t->getClips()[i]->startPosition() != TimePos(i, 0))
				{
					hasValidPatternClips = false;
					break;
				}
			}
		}
		if (!hasValidPatternClips)
		{
			t->deleteClips();
			t->createClipsForPattern(m_ps->numOfPatterns() - 1);
		}
		m_ps->updateAfterTrackAdd();

		de->accept();
	}
	else
	{
		TrackContainerView::dropEvent( de );
	}
	updateMaxSteps();
}


void PatternEditor::resizeEvent(QResizeEvent* re)
{
	updatePixelsPerBar();
}


void PatternEditor::updatePosition()
{
	//realignTracks();
	for (const auto& trackView : trackViews())
	{
		trackView->update();
	}
	emit positionChanged(m_currentPosition);
}

void PatternEditor::updatePixelsPerBar()
{
	setPixelsPerBar(m_maxClipLength != 0
		? (width() - m_trackHeadWidth) * TimePos::ticksPerBar() / m_maxClipLength
		: (width() - m_trackHeadWidth));
	m_timeLine->setPixelsPerBar(pixelsPerBar());
}

void PatternEditor::updateMaxSteps()
{
	const TrackContainer::TrackList& tl = model()->tracks();

	m_maxClipLength = 0;
	for (const auto& track : tl)
	{
		auto clip = track->getClip(m_ps->currentPattern());
		if (track->type() == Track::Type::Automation || track->type() == Track::Type::Sample)
		{
			// The length of automation and sample clips is updated to match the pattern length.
			clip->updateLength();
		}
		m_maxClipLength = std::max(m_maxClipLength, static_cast<tick_t>(clip->length()));
	}
	updatePixelsPerBar();
	emit stepsChanged(currentSteps());
}


void PatternEditor::makeSteps( bool clone )
{
	const TrackContainer::TrackList& tl = model()->tracks();

	for (const auto& track : tl)
	{
		if (track->type() == Track::Type::Instrument)
		{
			auto p = static_cast<MidiClip*>(track->getClip(m_ps->currentPattern()));
			if( clone )
			{
				p->cloneSteps();
			} else
			{
				p->addSteps();
			}
		}
	}
	updateMaxSteps();
}

// Creates a clone of the current pattern track with the same content, but no clips in the song editor
// TODO: Avoid repeated code from cloneTrack and clearTrack in TrackOperationsWidget somehow
void PatternEditor::cloneClip()
{
	// Get the current PatternTrack id
	auto ps = static_cast<PatternStore*>(model());
	const int currentPattern = ps->currentPattern();

	PatternTrack* pt = PatternTrack::findPatternTrack(currentPattern);

	if (pt)
	{
		// Clone the track
		Track* newTrack = pt->clone();
		ps->setCurrentPattern(static_cast<PatternTrack*>(newTrack)->patternIndex());

		// Track still have the clips which is undesirable in this case, clear the track
		newTrack->lock();
		newTrack->deleteClips();
		newTrack->unlock();
	}
}




PatternEditorWindow::PatternEditorWindow(PatternStore* ps) :
	Editor(false),
	m_editor(new PatternEditor(ps))
{
	setWindowIcon(embed::getIconPixmap("pattern_track_btn"));
	setWindowTitle(tr("Pattern Editor"));
	setCentralWidget(m_editor);

	setAcceptDrops(true);
	m_toolBar->setAcceptDrops(true);
	connect(m_toolBar, SIGNAL(dragEntered(QDragEnterEvent*)), m_editor, SLOT(dragEnterEvent(QDragEnterEvent*)));
	connect(m_toolBar, SIGNAL(dropped(QDropEvent*)), m_editor, SLOT(dropEvent(QDropEvent*)));

	// TODO: Use style sheet
	if (ConfigManager::inst()->value("ui", "compacttrackbuttons").toInt())
	{
		setMinimumWidth(TRACK_OP_WIDTH_COMPACT + DEFAULT_SETTINGS_WIDGET_WIDTH_COMPACT + 2 * ClipView::BORDER_WIDTH + 384);
	}
	else
	{
		setMinimumWidth(TRACK_OP_WIDTH + DEFAULT_SETTINGS_WIDGET_WIDTH + 2 * ClipView::BORDER_WIDTH + 384);
	}

	m_playAction->setToolTip(tr("Play/pause current pattern (Space)"));
	m_stopAction->setToolTip(tr("Stop playback of current pattern (Space)"));


	// Pattern selector
	DropToolBar* patternSelectionToolBar = addDropToolBarToTop(tr("Pattern selector"));

	m_patternComboBox = new ComboBox(m_toolBar);
	m_patternComboBox->setFixedSize(170, ComboBox::DEFAULT_HEIGHT);
	m_patternComboBox->setModel(&ps->m_patternComboBoxModel);

	patternSelectionToolBar->addWidget(m_patternComboBox);


	// Track actions
	DropToolBar *trackAndStepActionsToolBar = addDropToolBarToTop(tr("Track and step actions"));


	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("add_pattern_track"), tr("New pattern"),
						Engine::getSong(), SLOT(addPatternTrack()));
	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("clone_pattern_track_clip"), tr("Clone pattern"),
						m_editor, SLOT(cloneClip()));
	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("add_sample_track"),	tr("Add sample-track"),
						m_editor, SLOT(addSampleTrack()));
	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("add_automation"), tr("Add automation-track"),
						m_editor, SLOT(addAutomationTrack()));

	// FL-style "+" button: add any instrument as a new channel
	trackAndStepActionsToolBar->addSeparator();
	auto addChannelMenu = new QMenu(this);
	QAction* sampleAction = addChannelMenu->addAction(embed::getIconPixmap("sample_file"), tr("Audio sample..."));
	sampleAction->setToolTip(tr("Pick an audio file (WAV, OGG, MP3, FLAC...) and play it from a new channel"));
	connect(sampleAction, &QAction::triggered, this, [this]() {
		const QString file = QFileDialog::getOpenFileName(this, tr("Add audio sample"),
			ConfigManager::inst()->userSamplesDir(),
			tr("Audio files (*.wav *.ogg *.mp3 *.flac *.aif *.aiff *.ds *.au *.voc *.raw)"));
		if (file.isEmpty()) { return; }
		auto it = dynamic_cast<InstrumentTrack*>(Track::create(Track::Type::Instrument, Engine::patternStore()));
		if (!it) { return; }
		if (Instrument* inst = it->loadInstrument("audiofileprocessor"))
		{
			inst->loadFile(file);
			it->setName(QFileInfo(file).completeBaseName());
		}
		PatternEditor::routeToNewMixerChannel(it);
	});
	addChannelMenu->addSeparator();
	auto instruments = getPluginFactory()->descriptors(Plugin::Type::Instrument);
	std::sort(instruments.begin(), instruments.end(),
		[](const Plugin::Descriptor* a, const Plugin::Descriptor* b) { return qstricmp(a->displayName, b->displayName) < 0; });
	for (const Plugin::Descriptor* desc : instruments)
	{
		if (desc->subPluginFeatures) { continue; } // needs a sub-plugin key (LV2, ...): use the plugin browser
		QAction* a = addChannelMenu->addAction(desc->logo->pixmap(16, 16), QString(desc->displayName));
		a->setToolTip(QCoreApplication::translate("lmms::gui::PluginDescWidget", desc->description));
		const QString pluginName = desc->name;
		connect(a, &QAction::triggered, this, [pluginName]() {
			auto it = dynamic_cast<InstrumentTrack*>(Track::create(Track::Type::Instrument, Engine::patternStore()));
			if (it)
			{
				it->loadInstrument(pluginName);
				PatternEditor::routeToNewMixerChannel(it);
			}
		});
	}
	auto addChannelButton = new QToolButton(trackAndStepActionsToolBar);
	addChannelButton->setIcon(embed::getIconPixmap("add"));
	addChannelButton->setText(tr("Add channel"));
	addChannelButton->setToolTip(tr("Add a new channel (instrument) to the Channel Rack"));
	addChannelButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	addChannelButton->setMenu(addChannelMenu);
	addChannelButton->setPopupMode(QToolButton::InstantPopup);
	trackAndStepActionsToolBar->addWidget(addChannelButton);

	// GarageBand-style Smart Drummer
	QAction* drummerAction = trackAndStepActionsToolBar->addAction(embed::getIconPixmap("drummer"), tr("Smart Drummer"));
	drummerAction->setToolTip(tr("Generate a drum beat for the current pattern (like GarageBand's Drummer)"));
	connect(drummerAction, &QAction::triggered, this, [this]() {
		auto dlg = new SmartDrummerDialog(this);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->show();
	});
	if (auto b = qobject_cast<QToolButton*>(trackAndStepActionsToolBar->widgetForAction(drummerAction)))
	{
		b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		b->setObjectName("smartDrummerButton");
	}

	// FL-style swing for the whole Channel Rack
	trackAndStepActionsToolBar->addSeparator();
	auto swingKnob = new Knob(KnobType::Small17, tr("SWING"), trackAndStepActionsToolBar,
		Knob::LabelRendering::LegacyFixedFontSize, tr("Swing"));
	swingKnob->setModel(&Engine::getSong()->swingModel());
	swingKnob->setHintText(tr("Swing:"), "%");
	swingKnob->setToolTip(tr("Swing: delays every second 16th step for a shuffled groove"));
	trackAndStepActionsToolBar->addWidget(swingKnob);

	auto stretch = new QWidget(m_toolBar);
	stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	trackAndStepActionsToolBar->addWidget(stretch);

	auto stepsLabel = new QLabel(trackAndStepActionsToolBar);
	stepsLabel->setObjectName("channelRackStepsLabel");
	stepsLabel->setToolTip(tr("Length of the current pattern"));
	trackAndStepActionsToolBar->addWidget(stepsLabel);
	connect(m_editor, &PatternEditor::stepsChanged, stepsLabel, [stepsLabel](int steps) {
		stepsLabel->setText(tr("%1 steps").arg(steps) + "  ");
	});
	stepsLabel->setText(tr("%1 steps").arg(m_editor->currentSteps()) + "  ");


	// Step actions
	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("step_btn_remove"), tr("Remove steps"),
						m_editor, SLOT(removeSteps()));
	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("step_btn_add"), tr("Add steps"),
						m_editor, SLOT(addSteps()));
	trackAndStepActionsToolBar->addAction( embed::getIconPixmap("step_btn_duplicate"), tr("Clone Steps"),
						m_editor, SLOT(cloneSteps()));
	trackAndStepActionsToolBar->addAction(embed::getIconPixmap("edit_erase"), tr("Clear pattern (all steps of every channel)"),
						m_editor, SLOT(clearCurrentPattern()));

	connect(&ps->m_patternComboBoxModel, SIGNAL(dataChanged()),
			m_editor, SLOT(updatePosition()));
	connect(&ps->m_patternComboBoxModel, &ComboBoxModel::dataChanged, m_editor, &PatternEditor::updateMaxSteps);

	auto viewNext = new QAction(this);
	connect(viewNext, SIGNAL(triggered()), m_patternComboBox, SLOT(selectNext()));
	viewNext->setShortcut(Qt::Key_Plus);
	addAction(viewNext);

	auto viewPrevious = new QAction(this);
	connect(viewPrevious, SIGNAL(triggered()), m_patternComboBox, SLOT(selectPrevious()));
	viewPrevious->setShortcut(Qt::Key_Minus);
	addAction(viewPrevious);
}


QSize PatternEditorWindow::sizeHint() const
{
	return {minimumWidth() + 10, 300};
}


void PatternEditorWindow::play()
{
	getGUI()->mainWindow()->setTransportMode(false, false);
	if (Engine::getSong()->playMode() != Song::PlayMode::Pattern)
	{
		Engine::getSong()->playPattern();
	}
	else
	{
		Engine::getSong()->togglePause();
	}
}


void PatternEditorWindow::stop()
{
	Engine::getSong()->stop();
}


} // namespace lmms::gui
