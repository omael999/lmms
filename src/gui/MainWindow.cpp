/*
 * MainWindow.cpp - implementation of LMMS-main-window
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "MainWindow.h"

#include <algorithm>

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDomElement>
#include <QFileInfo>
#include <QMdiArea>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include "AboutDialog.h"
#include "AutomationEditor.h"
#include "ControllerRackView.h"
#include "DeprecationHelper.h"
#include "embed.h"
#include "Engine.h"
#include "ExportProjectDialog.h"
#include "FileBrowser.h"
#include "FileDialog.h"
#include "Metronome.h"
#include "MixerView.h"
#include "GuiApplication.h"
#include "ImportFilter.h"
#include "InstrumentTrackView.h"
#include "InstrumentTrackWindow.h"
#include "MicrotunerConfig.h"
#include "PathUtil.h"
#include "PatternEditor.h"
#include "PatternStore.h"
#include "PianoRoll.h"
#include "PianoView.h"
#include "PluginBrowser.h"
#include "PluginFactory.h"
#include "PluginView.h"
#include "ProjectJournal.h"
#include "ProjectNotes.h"
#include "ProjectRenderer.h"
#include "RecentProjectsMenu.h"
#include "RemotePluginBase.h"
#include "SetupDialog.h"
#include "SideBar.h"
#include "SongEditor.h"
#include "SubWindow.h"
#include "TemplatesMenu.h"
#include "TextFloat.h"
#include "ToolButton.h"
#include "ToolPlugin.h"
#include "VersionedSaveDialog.h"

#include "lmmsversion.h"


namespace lmms::gui
{

namespace
{

bool isAttachedSubWindow(const QWidget* w)
{
	auto sw = qobject_cast<const SubWindow*>(w);
	return sw && !sw->isDetached();
}

QWidget* subWindowOf(QWidget* w) { return w ? w->parentWidget() : nullptr; }

} // namespace






MainWindow::MainWindow() :
	m_workspace( nullptr ),
	m_toolsMenu( nullptr ),
	m_autoSaveTimer( this ),
	m_viewMenu( nullptr ),
	m_metronomeToggle( 0 ),
	m_session( SessionState::Normal )
{
	setAttribute( Qt::WA_DeleteOnClose );

	auto main_widget = new QWidget(this);
	auto vbox = new QVBoxLayout(main_widget);
	vbox->setSpacing( 0 );
	vbox->setContentsMargins(0, 0, 0, 0);

	auto w = new QWidget(main_widget);
	auto hbox = new QHBoxLayout(w);
	hbox->setSpacing( 0 );
	hbox->setContentsMargins(0, 0, 0, 0);

	auto sideBar = new SideBar(Qt::Vertical, w);
	m_sideBar = sideBar;

	auto splitter = new QSplitter(Qt::Horizontal, w);
	splitter->setChildrenCollapsible( false );

	ConfigManager* confMgr = ConfigManager::inst();
	bool sideBarOnRight = confMgr->value("ui", "sidebaronright").toInt();

	emit initProgress(tr("Preparing plugin browser"));
	sideBar->appendTab( new PluginBrowser( splitter ) );
	emit initProgress(tr("Preparing file browsers"));

	sideBar->appendTab(new FileBrowser(FileBrowser::Type::Favorites, ConfigManager::inst()->favoriteItems().join("*"), FileItem::defaultFilters(), "My Favorites",
		embed::getIconPixmap("star").transformed(QTransform().rotate(90)), splitter, false, "", ""));

	sideBar->appendTab(new FileBrowser(FileBrowser::Type::Normal,
		confMgr->userProjectsDir() + "*" + confMgr->factoryProjectsDir(), "*.mmp *.mmpz *.xml *.mid *.mpt",
		tr("My Projects"), embed::getIconPixmap("project_file").transformed(QTransform().rotate(90)), splitter, false,
		confMgr->userProjectsDir(), confMgr->factoryProjectsDir()));

	sideBar->appendTab(new FileBrowser(FileBrowser::Type::Normal,
		confMgr->userSamplesDir() + "*" + confMgr->factorySamplesDir(), FileItem::defaultFilters(), tr("My Samples"),
		embed::getIconPixmap("sample_file").transformed(QTransform().rotate(90)), splitter, false,
		confMgr->userSamplesDir(), confMgr->factorySamplesDir()));

	sideBar->appendTab(new FileBrowser(FileBrowser::Type::Normal,
		confMgr->userPresetsDir() + "*" + confMgr->factoryPresetsDir(), "*.xpf *.cs.xml *.xiz *.lv2", tr("My Presets"),
		embed::getIconPixmap("preset_file").transformed(QTransform().rotate(90)), splitter, false,
		confMgr->userPresetsDir(), confMgr->factoryPresetsDir()));

	sideBar->appendTab(new FileBrowser(FileBrowser::Type::Normal, QDir::homePath(), FileItem::defaultFilters(),
		tr("My Home"), embed::getIconPixmap("home").transformed(QTransform().rotate(90)), splitter, false));

	QStringList root_paths;
	QString title = tr("Root Directory");
	bool dirs_as_items = false;

#ifdef LMMS_BUILD_APPLE
	title = tr( "Volumes" );
	root_paths += "/Volumes";
#elif defined(LMMS_BUILD_WIN32)
	title = tr( "My Computer" );
	dirs_as_items = true;
#endif

#if ! defined(LMMS_BUILD_APPLE)
	QFileInfoList drives = QDir::drives();
	for( const QFileInfo & drive : drives )
	{
		root_paths += drive.absolutePath();
	}
#endif

	sideBar->appendTab(new FileBrowser(FileBrowser::Type::Normal, root_paths.join("*"), FileItem::defaultFilters(), title,
		embed::getIconPixmap("computer").transformed(QTransform().rotate(90)), splitter, dirs_as_items));

	m_workspace = new MovableQMdiArea(splitter);

	// Load background
	emit initProgress(tr("Loading background picture"));
	QString backgroundPicFile = ConfigManager::inst()->backgroundPicFile();
	QImage backgroundPic;
	if( !backgroundPicFile.isEmpty() )
	{
		backgroundPic = QImage( backgroundPicFile );
	}
	if( !backgroundPicFile.isNull() )
	{
		m_workspace->setBackground( backgroundPic );
	}
	else
	{
		m_workspace->setBackground( Qt::NoBrush );
	}

	m_workspace->setOption( QMdiArea::DontMaximizeSubWindowOnActivation );
	m_workspace->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_workspace->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	hbox->addWidget(sideBar);
	hbox->addWidget(splitter);
	// If the user wants the sidebar on the right, we move the workspace and
	// the splitter to the "left" side, or the first widgets in their list
	if (sideBarOnRight)
	{
		splitter->insertWidget(0, m_workspace);
		hbox->insertWidget(0, splitter);
	}

	// create global-toolbar at the top of our window
	m_toolBar = new QWidget( main_widget );
	m_toolBar->setObjectName( "mainToolbar" );
	m_toolBar->setFixedHeight( 64 );
	m_toolBar->move( 0, 0 );

	// add layout for organizing quite complex toolbar-layouting
	m_toolBarLayout = new QGridLayout( m_toolBar/*, 2, 1*/ );
	m_toolBarLayout->setContentsMargins(0, 0, 0, 0);
	m_toolBarLayout->setSpacing( 0 );

	// NovaBeat defaults: a single channel-settings window at a time (less clutter, like FL)
	if (ConfigManager::inst()->value("ui", "oneinstrumenttrackwindow").isEmpty())
	{
		ConfigManager::inst()->setValue("ui", "oneinstrumenttrackwindow", "1");
	}

	// FL-style PAT/SONG transport, placed right after the project/window buttons
	m_songMode = ConfigManager::inst()->value("ui", "transportsongmode").toInt() != 0;
	createTransport();

	vbox->addWidget( m_toolBar );
	vbox->addWidget( w );
	setCentralWidget( main_widget );

	m_updateTimer.start( 1000 / 60, this );  // 60 fps

	if( ConfigManager::inst()->value( "ui", "enableautosave" ).toInt() )
	{
		// connect auto save
		connect(&m_autoSaveTimer, SIGNAL(timeout()), this, SLOT(autoSave()));
		m_autoSaveInterval = ConfigManager::inst()->value(
					"ui", "saveinterval" ).toInt() < 1 ?
						DEFAULT_AUTO_SAVE_INTERVAL :
				ConfigManager::inst()->value(
					"ui", "saveinterval" ).toInt();

		// The auto save function mustn't run until there is a project
		// to save or it will run over recover.mmp if you hesitate at the
		// recover messagebox for a minute. It is now started in main.
		// See autoSaveTimerReset() in MainWindow.h
	}

	connect( Engine::getSong(), SIGNAL(playbackStateChanged()),
				this, SLOT(updatePlayPauseIcons()));

	connect(Engine::getSong(), SIGNAL(modified()), SLOT(onSongModified()));
	connect(Engine::getSong(), SIGNAL(projectFileNameChanged()), SLOT(onProjectFileNameChanged()));

	maximized = isMaximized();
	new QShortcut(QKeySequence(Qt::Key_F11), this, SLOT(toggleFullscreen()));

	if (ConfigManager::inst()->value("tooltips", "disabled").toInt())
	{
		qApp->installEventFilter(this);
	}

	installEventFilter(this);
}




MainWindow::~MainWindow()
{
	for( PluginView *view : m_tools )
	{
		delete view->model();
		delete view;
	}
	// TODO: Close tools
	// dependencies are such that the editors must be destroyed BEFORE Song is deletect in Engine::destroy
	//   see issue #2015 on github
	delete getGUI()->automationEditor();
	delete getGUI()->pianoRoll();
	delete getGUI()->songEditor();
	// destroy engine which will do further cleanups etc.
	Engine::destroy();
}




void MainWindow::finalize()
{
	resetWindowTitle();
	setWindowIcon( embed::getIconPixmap( "icon_small" ) );

	auto addAction = [this](QMenu* menu, std::string_view icon, const QString& text,
		const QKeySequence& shortcut, auto(MainWindow::* slot)()) -> QAction*
	{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
		return menu->addAction(embed::getIconPixmap(icon), text, shortcut, this, slot);
#else
		return menu->addAction(embed::getIconPixmap(icon), text, this, slot, shortcut);
#endif
	};

	// project-popup-menu
	auto project_menu = new QMenu(this);
	menuBar()->addMenu( project_menu )->setText( tr( "&File" ) );

	addAction(project_menu, "project_new", tr("&New"),
		QKeySequence::New, &MainWindow::createNewProject);

	auto templates_menu = new TemplatesMenu( this );
	project_menu->addMenu(templates_menu);

	addAction(project_menu, "project_open", tr("&Open..."),
		QKeySequence::Open, &MainWindow::openProject);

	project_menu->addMenu(new RecentProjectsMenu(this));

	addAction(project_menu, "project_save", tr("&Save"),
		QKeySequence::Save, &MainWindow::saveProject);

	addAction(project_menu, "project_save", tr("Save &As..."),
		keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_S), &MainWindow::saveProjectAs);

	addAction(project_menu, "project_save", tr("Save as New &Version"),
		keySequence(Qt::CTRL, Qt::ALT, Qt::Key_S), &MainWindow::saveProjectAsNewVersion);

	project_menu->addAction(embed::getIconPixmap("project_save"), tr("Save as default template"),
		this, &MainWindow::saveProjectAsDefaultTemplate);

	project_menu->addSeparator();

	project_menu->addAction(embed::getIconPixmap("project_import"), tr("Import..."),
		this, &MainWindow::onImportProject);

	addAction(project_menu, "project_export", tr("E&xport..."),
		keySequence(Qt::CTRL, Qt::Key_E), &MainWindow::onExportProject);

	addAction(project_menu, "project_export", tr("Export &Tracks..."),
		keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_E), &MainWindow::onExportProjectTracks);

	addAction(project_menu, "midi_file", tr("Export &MIDI..."),
		keySequence(Qt::CTRL, Qt::Key_M), &MainWindow::onExportProjectMidi);

	project_menu->addSeparator();

	project_menu->addAction(embed::getIconPixmap("exit"), tr("&Quit"),
		qApp, SLOT(closeAllWindows()))->setShortcut(keySequence(Qt::CTRL, Qt::Key_Q));

	auto edit_menu = new QMenu(this);
	menuBar()->addMenu( edit_menu )->setText( tr( "&Edit" ) );

	m_undoAction = addAction(edit_menu, "edit_undo", tr("Undo"),
		QKeySequence::Undo, &MainWindow::undo);

	m_redoAction = addAction(edit_menu, "edit_redo", tr("Redo"),
		QKeySequence::Redo, &MainWindow::redo);

	m_undoAction->setShortcutContext(Qt::ApplicationShortcut);
	m_redoAction->setShortcutContext(Qt::ApplicationShortcut);

	edit_menu->addSeparator();
	edit_menu->addAction(embed::getIconPixmap("microtuner"), tr("Scales and keymaps"),
		this, SLOT(toggleMicrotunerWin()));
	edit_menu->addAction(embed::getIconPixmap("setup_general"), tr("Settings"),
		this, SLOT(showSettingsDialog()));

	connect(edit_menu, SIGNAL(aboutToShow()), this, SLOT(updateUndoRedoButtons()));

	m_viewMenu = new QMenu( this );
	menuBar()->addMenu( m_viewMenu )->setText( tr( "&View" ) );
	connect( m_viewMenu, SIGNAL(aboutToShow()),
		 this, SLOT(updateViewMenu()));
	connect( m_viewMenu, SIGNAL(triggered(QAction*)), this,
		SLOT(updateConfig(QAction*)));
	updateViewMenu();


	m_toolsMenu = new QMenu( this );
	for( const Plugin::Descriptor* desc : getPluginFactory()->descriptors(Plugin::Type::Tool) )
	{
		m_toolsMenu->addAction( desc->logo->pixmap(), desc->displayName );
		m_tools.push_back( ToolPlugin::instantiate( desc->name, /*this*/nullptr )
						   ->createView(this) );
	}
	if( !m_toolsMenu->isEmpty() )
	{
		menuBar()->addMenu( m_toolsMenu )->setText( tr( "&Tools" ) );
		connect( m_toolsMenu, SIGNAL(triggered(QAction*)),
					this, SLOT(showTool(QAction*)));
	}


	// help-popup-menu
	auto help_menu = new QMenu(this);
	menuBar()->addMenu( help_menu )->setText( tr( "&Help" ) );
	// May use offline help
	if( true )
	{
		help_menu->addAction( embed::getIconPixmap( "help" ),
						tr( "Online Help" ),
						this, SLOT(browseHelp()));
	}
	else
	{
		help_menu->addAction( embed::getIconPixmap( "help" ),
							tr( "Help" ),
							this, SLOT(help()));
	}

	help_menu->addSeparator();
	help_menu->addAction( embed::getIconPixmap( "icon_small" ), tr( "About" ),
				  this, SLOT(aboutLMMS()));

	// create tool-buttons
	auto project_new = new ToolButton(
		embed::getIconPixmap("project_new"), tr("Create new project"), this, SLOT(createNewProject()), m_toolBar);

	auto project_new_from_template = new ToolButton(embed::getIconPixmap("project_new_from_template"),
		tr("Create new project from template"), this, SLOT(emptySlot()), m_toolBar);
	project_new_from_template->setMenu( templates_menu );
	project_new_from_template->setPopupMode( ToolButton::InstantPopup );

	auto project_open = new ToolButton(
		embed::getIconPixmap("project_open"), tr("Open existing project"), this, SLOT(openProject()), m_toolBar);

	auto project_open_recent = new ToolButton(embed::getIconPixmap("project_open_recent"),
		tr("Recently opened projects"), this, SLOT(emptySlot()), m_toolBar);
	project_open_recent->setMenu( new RecentProjectsMenu(this) );
	project_open_recent->setPopupMode( ToolButton::InstantPopup );

	auto project_save = new ToolButton(
		embed::getIconPixmap("project_save"), tr("Save current project"), this, SLOT(saveProject()), m_toolBar);

	auto project_export = new ToolButton(
		embed::getIconPixmap("project_export"), tr("Export current project"), this, SLOT(onExportProject()), m_toolBar);

	m_metronomeToggle = new ToolButton(
				embed::getIconPixmap( "metronome" ),
				tr( "Metronome" ),
				this, SLOT(onToggleMetronome()),
							m_toolBar );
	m_metronomeToggle->setCheckable(true);
	m_metronomeToggle->setChecked(Engine::getSong()->metronome().active());

	m_toolBarLayout->setColumnMinimumWidth( 0, 5 );
	m_toolBarLayout->addWidget( project_new, 0, 1 );
	m_toolBarLayout->addWidget( project_new_from_template, 0, 2 );
	m_toolBarLayout->addWidget( project_open, 0, 3 );
	m_toolBarLayout->addWidget( project_open_recent, 0, 4 );
	m_toolBarLayout->addWidget( project_save, 0, 5 );
	m_toolBarLayout->addWidget( project_export, 0, 6 );
	m_toolBarLayout->addWidget( m_metronomeToggle, 0, 7 );


	// window-toolbar
	auto song_editor_window = new ToolButton(embed::getIconPixmap("songeditor"), tr("Song Editor") + " (F5)", this,
		SLOT(toggleSongEditorWin()), m_toolBar);
	song_editor_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_1));

	auto pattern_editor_window = new ToolButton(embed::getIconPixmap("pattern_track_btn"),
		tr("Pattern Editor") + " (F6)", this, SLOT(togglePatternEditorWin()), m_toolBar);
	pattern_editor_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_2));

	auto piano_roll_window = new ToolButton(
		embed::getIconPixmap("piano"), tr("Piano Roll") + " (F7)", this, SLOT(togglePianoRollWin()), m_toolBar);
	piano_roll_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_3));

	auto automation_editor_window = new ToolButton(embed::getIconPixmap("automation"),
		tr("Automation Editor") + " (F8)", this, SLOT(toggleAutomationEditorWin()), m_toolBar);
	automation_editor_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_4));

	auto mixer_window = new ToolButton(
		embed::getIconPixmap("mixer"), tr("Mixer") + " (F9)", this, SLOT(toggleMixerWin()), m_toolBar);
	mixer_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_5));

	auto controllers_window = new ToolButton(embed::getIconPixmap("controller"),
		tr("Show/hide controller rack") + " (Ctrl+6)", this, SLOT(toggleControllerRack()), m_toolBar);
	controllers_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_6));

	auto project_notes_window = new ToolButton(embed::getIconPixmap("project_notes"),
		tr("Show/hide project notes") + " (Ctrl+7)", this, SLOT(toggleProjectNotesWin()), m_toolBar);
	project_notes_window->setShortcut(keySequence(Qt::CTRL, Qt::Key_7));

	// FL Studio function-key layout (the Ctrl+number shortcuts above keep working too)
	new QShortcut(QKeySequence(Qt::Key_F5), this, SLOT(toggleSongEditorWin()));
	new QShortcut(QKeySequence(Qt::Key_F6), this, SLOT(togglePatternEditorWin()));
	new QShortcut(QKeySequence(Qt::Key_F7), this, SLOT(togglePianoRollWin()));
	new QShortcut(QKeySequence(Qt::Key_F8), this, SLOT(toggleAutomationEditorWin()));
	new QShortcut(QKeySequence(Qt::Key_F9), this, SLOT(toggleMixerWin()));
	new QShortcut(QKeySequence(Qt::Key_F10), this, SLOT(showSettingsDialog()));
	new QShortcut(QKeySequence(Qt::Key_F12), this, SLOT(hideAllEditorWindows()));

	m_toolBarLayout->addWidget( song_editor_window, 1, 1 );
	m_toolBarLayout->addWidget( pattern_editor_window, 1, 2 );
	m_toolBarLayout->addWidget( piano_roll_window, 1, 3 );
	m_toolBarLayout->addWidget( automation_editor_window, 1, 4 );
	m_toolBarLayout->addWidget( mixer_window, 1, 5 );
	m_toolBarLayout->addWidget( controllers_window, 1, 6 );
	m_toolBarLayout->addWidget( project_notes_window, 1, 7 );

	auto organize_windows = new ToolButton(embed::getIconPixmap("organize"),
		tr("Organize windows") + " (" + keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_W).toString(QKeySequence::NativeText) + ")",
		this, SLOT(organizeWindows()), m_toolBar);
	m_toolBarLayout->addWidget(organize_windows, 1, 8);
	m_toolBarLayout->setColumnStretch( 100, 1 );

	// setup-dialog opened before?
	if( !ConfigManager::inst()->value( "app", "configured" ).toInt() )
	{
		ConfigManager::inst()->setValue( "app", "configured", "1" );
		// no, so show it that user can setup everything
		SetupDialog sd;
		sd.exec();
	}
	// look whether the audio engine failed to start the audio device selected by the
	// user and is using AudioDummy as a fallback
	// or the audio device is set to invalid one
	else if( Engine::audioEngine()->audioDevStartFailed() || !AudioEngine::isAudioDevNameValid(
		ConfigManager::inst()->value( "audioengine", "audiodev" ) ) )
	{
		QMessageBox::critical(nullptr, "Audio device setup failed",
			tr("Failed to setup audio device for playback. Try adjusting your audio device settings (e.g. the sample rate), then restart LMMS."));

		// if so, offer the audio settings section of the setup dialog
		SetupDialog sd( SetupDialog::ConfigTab::AudioSettings );
		sd.exec();
	}

	// Add editor subwindows
	addWindowedWidget(getGUI()->automationEditor());
	addWindowedWidget(getGUI()->patternEditor());
	addWindowedWidget(getGUI()->pianoRoll());
	addWindowedWidget(getGUI()->songEditor());

	getGUI()->automationEditor()->parentWidget()->hide();
	getGUI()->patternEditor()->parentWidget()->move(610, 5);
	getGUI()->patternEditor()->parentWidget()->hide();
	getGUI()->pianoRoll()->parentWidget()->move(5, 5);
	getGUI()->pianoRoll()->parentWidget()->hide();
	getGUI()->songEditor()->parentWidget()->move(5, 5);
	getGUI()->songEditor()->parentWidget()->show();

	// FL-style docked workspace (can be turned off in View > "Docked FL layout")
	m_flLayout = ConfigManager::inst()->value("ui", "fllayout", "1").toInt() != 0;
	for (QWidget* w : {subWindowOf(getGUI()->songEditor()), subWindowOf(getGUI()->pianoRoll()),
		subWindowOf(getGUI()->automationEditor()), subWindowOf(getGUI()->mixerView())})
	{
		if (w)
		{
			m_dockedWindows.push_back(w);
			w->installEventFilter(this);
		}
	}
	m_workspace->viewport()->installEventFilter(this);

	// Every project (new or loaded) gets the tidy layout once its windows have been restored
	connect(Engine::getSong(), &Song::projectLoaded, this, [this]() {
		QTimer::singleShot(0, this, [this]() { applyProjectLayout(); });
	});
	connect(Engine::patternStore(), &TrackContainer::trackAdded, this, [this]() {
		QTimer::singleShot(0, this, [this]() { fitChannelRackHeight(); });
	});

	// The Playlist is the background: it stays behind the floating windows even when clicked
	connect(m_workspace, &QMdiArea::subWindowActivated, this, [this](QMdiSubWindow* sw) {
		QWidget* songWin = subWindowOf(getGUI()->songEditor());
		if (sw == nullptr || sw != songWin) { return; }
		const QSize area = m_workspace->viewport()->size();
		const bool fillsWorkspace = m_flLayout || sw->isMaximized()
			|| (sw->width() >= area.width() - 8 && sw->height() >= area.height() - 8);
		if (fillsWorkspace)
		{
			QTimer::singleShot(0, sw, [sw]() { sw->lower(); });
		}
	});

	auto organizeShortcut = new QShortcut(keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_W), this);
	connect(organizeShortcut, &QShortcut::activated, this, &MainWindow::organizeWindows);

	// Open the sample browser on the left, like FL's browser
	if (m_sideBar) { m_sideBar->openTab(3); }
	updateTransportButtons();

	// reset window title every time we change the state of a subwindow to show the correct title
	for( const QMdiSubWindow * subWindow : workspace()->subWindowList() )
	{
		connect( subWindow, SIGNAL(windowStateChanged(Qt::WindowStates,Qt::WindowStates)), this, SLOT(resetWindowTitle()));
	}
}


int MainWindow::addWidgetToToolBar( QWidget * _w, int _row, int _col )
{
	int col = ( _col == -1 ) ? m_toolBarLayout->columnCount() + 7 : _col;
	if( _w->height() > 32 || _row == -1 )
	{
		m_toolBarLayout->addWidget( _w, 0, col, 2, 1 );
	}
	else
	{
		m_toolBarLayout->addWidget( _w, _row, col );
	}
	return( col );
}




void MainWindow::addSpacingToToolBar( int _size )
{
	m_toolBarLayout->setColumnMinimumWidth( m_toolBarLayout->columnCount() +
								7, _size );
}




SubWindow* MainWindow::addWindowedWidget(QWidget *w, Qt::WindowFlags windowFlags)
{
	// wrap the widget in our own *custom* window that patches some errors in QMdiSubWindow
	auto win = new SubWindow(m_workspace->viewport(), windowFlags);
	connect(this, &MainWindow::detachAllSubWindows, win, &SubWindow::setDetached);
	win->setWidget(w);

	if (w)
	{
		// TODO: somehow make this work on any setWidget
		connect(w, &QWidget::destroyed, win, &SubWindow::deleteLater);

		if (w->sizeHint().isValid())
		{
			auto titleBarHeight = win->titleBarHeight();
			auto frameWidth = win->frameWidth();
			QSize delta(2* frameWidth, titleBarHeight + frameWidth);
			win->resize(delta + w->sizeHint());
		}
	}

	m_workspace->addSubWindow(win);
	return win;
}



void MainWindow::setAllSubWindowsDetached(bool detached)
{
	emit detachAllSubWindows(detached);
}


void MainWindow::resetWindowTitle()
{
	QString title(tr( "Untitled" ));

	if( Engine::getSong()->projectFileName() != "" )
	{
		title = QFileInfo( Engine::getSong()->projectFileName()
							).completeBaseName();
	}

	if( Engine::getSong()->isModified() )
	{
		title += '*';
	}

	if( getSession() == SessionState::Recover )
	{
		title += " - " + tr( "Recover session. Please save your work!" );
	}

	setWindowTitle(title + " - " + QString("%1 %2").arg(LMMS_APP_NAME, LMMS_VERSION));
}




bool MainWindow::mayChangeProject(bool stopPlayback)
{
	if( stopPlayback )
	{
		Engine::getSong()->stop();
	}

	if( !Engine::getSong()->isModified() && getSession() != SessionState::Recover )
	{
		return( true );
	}

	// Separate message strings for modified and recovered files
	QString messageTitleRecovered = tr( "Recovered project not saved" );
	QString messageRecovered = tr( "This project was recovered from the "
					"previous session. It is currently "
					"unsaved and will be lost if you don't "
					"save it. Do you want to save it now?" );

	QString messageTitleUnsaved = tr( "Project not saved" );
	QString messageUnsaved = tr( "The current project was modified since "
					"last saving. Do you want to save it "
								"now?" );

	const auto& title = getSession() == SessionState::Recover
		? messageTitleRecovered
		: messageTitleUnsaved;

	const auto& text = getSession() == SessionState::Recover
		? messageRecovered
		: messageUnsaved;

	const auto buttons = QMessageBox::Save
		| QMessageBox::Discard
		| QMessageBox::Cancel;

	auto mb = QMessageBox{QMessageBox::Question, title, text, buttons, this};
	int answer = mb.exec();

	if( answer == QMessageBox::Save )
	{
		return( saveProject() );
	}
	else if( answer == QMessageBox::Discard )
	{
		if( getSession() == SessionState::Recover )
		{
			sessionCleanup();
		}
		return( true );
	}

	return( false );
}




void MainWindow::clearKeyModifiers()
{
	m_keyMods.m_ctrl = false;
	m_keyMods.m_shift = false;
	m_keyMods.m_alt = false;
}




void MainWindow::saveWidgetState(QWidget* w, QDomElement& de)
{
	// TODO: Only use one of these
	auto win = qobject_cast<SubWindow*>(w);
	if (!win)
	{
		// Fall back on parent
		win = qobject_cast<SubWindow*>(w->parentWidget());
		if (!win)
		{
			// Still could not find the window - soft fail
			return;
		}
	}

	de.setAttribute("visible", bool{win->widget() && win->widget()->isVisible()});
	de.setAttribute("maximized", win->isMaximized());

	QRect normalGeometry = win->getTrueNormalGeometry();
	de.setAttribute("x", normalGeometry.x());
	de.setAttribute("y", normalGeometry.y() );
	de.setAttribute("width", normalGeometry.width());
	de.setAttribute("height", normalGeometry.height());
}

void MainWindow::restoreWidgetState(QWidget* w, const QDomElement& de)
{
	// TODO: Only use one of these
	auto win = qobject_cast<SubWindow*>(w);
	if (!win)
	{
		// Fall back on parent
		win = qobject_cast<SubWindow*>(w->parentWidget());

		// Still could not find the window - soft fail
		if (!win) { return; }
	}

	const auto normalGeometry = QRect{
		de.attribute("x").toInt(),
		de.attribute("y").toInt(),
		de.attribute("width").toInt(),
		de.attribute("height").toInt(),
	};

	// First restore the window, as attempting to resize a maximized window can cause graphical glitches.
	win->setWindowState(win->windowState() & ~(Qt::WindowMaximized | Qt::WindowMinimized));

	// Then resize it. `QWidget::setGeometry` adjusts the size if it is invalid / under the minimum size.
	win->setGeometry(normalGeometry);

	// Set the window to its correct "maximized?" state.
	auto winState = win->windowState();
	winState.setFlag(Qt::WindowMaximized, de.attribute("maximized").toInt());
	win->setWindowState(winState);

	if (const auto visible = de.attribute("visible"); !visible.isEmpty())
	{
		win->setVisible(visible.toInt());
	}
}

void MainWindow::emptySlot()
{
}



void MainWindow::createNewProject()
{
	if( mayChangeProject(true) )
	{
		Engine::getSong()->createNewProject();
	}
}




void MainWindow::openProject()
{
	if( mayChangeProject(false) )
	{
		FileDialog ofd( this, tr( "Open Project" ), "", tr( "LMMS (*.mmp *.mmpz)" ) );

		ofd.setDirectory( ConfigManager::inst()->userProjectsDir() );
		ofd.setFileMode( FileDialog::ExistingFiles );
		if( ofd.exec () == QDialog::Accepted &&
						!ofd.selectedFiles().isEmpty() )
		{
			Song *song = Engine::getSong();

			song->stop();
			setCursor( Qt::WaitCursor );
			song->loadProject( ofd.selectedFiles()[0] );
			setCursor( Qt::ArrowCursor );
		}
	}
}




bool MainWindow::saveProject()
{
	if( Engine::getSong()->projectFileName() == "" )
	{
		return( saveProjectAs() );
	}
	else if( this->guiSaveProject() )
	{
		if( getSession() == SessionState::Recover )
		{
			sessionCleanup();
		}
		return true;
	}
	return false;
}




bool MainWindow::saveProjectAs()
{
	auto optionsWidget = new SaveOptionsWidget(Engine::getSong()->getSaveOptions());
	VersionedSaveDialog sfd( this, optionsWidget, tr( "Save Project" ), "",
			tr( "LMMS Project" ) + " (*.mmpz *.mmp);;" +
				tr( "LMMS Project Template" ) + " (*.mpt)" );
	QString f = Engine::getSong()->projectFileName();
	if( f != "" )
	{
		sfd.setDirectory( QFileInfo( f ).absolutePath() );
		sfd.selectFile( QFileInfo( f ).fileName() );
	}
	else
	{
		sfd.setDirectory( ConfigManager::inst()->userProjectsDir() );
	}

	// Don't write over file with suffix if no suffix is provided.
	QString suffix = ConfigManager::inst()->value( "app",
							"nommpz" ).toInt() == 0
						? "mmpz"
						: "mmp" ;
	sfd.setDefaultSuffix( suffix );

	if( sfd.exec () == FileDialog::Accepted &&
		!sfd.selectedFiles().isEmpty() && sfd.selectedFiles()[0] != "" )
	{
		QString fname = sfd.selectedFiles()[0] ;
		if( sfd.selectedNameFilter().contains( "(*.mpt)" ) )
		{
			// Remove the default suffix
			fname.remove( "." + suffix );
			if( !sfd.selectedFiles()[0].endsWith( ".mpt" ) )
			{
				if( VersionedSaveDialog::fileExistsQuery( fname + ".mpt",
						tr( "Save project template" ) ) )
				{
					fname += ".mpt";
				}
			}
		}
		if( this->guiSaveProjectAs( fname ) )
		{
			if( getSession() == SessionState::Recover )
			{
				sessionCleanup();
			}
			return true;
		}
	}
	return false;
}




bool MainWindow::saveProjectAsNewVersion()
{
	QString fileName = Engine::getSong()->projectFileName();
	if( fileName == "" )
	{
		return saveProjectAs();
	}
	else
	{
		do 		VersionedSaveDialog::changeFileNameVersion( fileName, true );
		while 	( QFile( fileName ).exists() );

		return this->guiSaveProjectAs( fileName );
	}
}




void MainWindow::saveProjectAsDefaultTemplate()
{
	QString defaultTemplate = ConfigManager::inst()->userTemplateDir() + "default.mpt";

	QFileInfo fileInfo(defaultTemplate);
	if (fileInfo.exists())
	{
		if (QMessageBox::warning(this,
					 tr("Overwrite default template?"),
					 tr("This will overwrite your current default template."),
					 QMessageBox::Ok,
					 QMessageBox::Cancel) != QMessageBox::Ok)
		{
			return;
		}
	}

	Engine::getSong()->saveProjectFile( defaultTemplate );
}




void MainWindow::showSettingsDialog()
{
	SetupDialog sd;
	sd.exec();
}




void MainWindow::aboutLMMS()
{
	AboutDialog(this).exec();
}




void MainWindow::help()
{
	QMessageBox::information( this, tr( "Help not available" ),
				  tr( "Currently there's no help "
						  "available in LMMS.\n"
						  "Please visit "
						  "http://lmms.sf.net/wiki "
						  "for documentation on LMMS." ),
				  QMessageBox::Ok );
}




void MainWindow::toggleWindow( QWidget *window, bool forceShow )
{
	// All "windows" should be inside a SubWindow, because the use of activeSubWindow() depends on it
	auto parent = dynamic_cast<QMdiSubWindow*>(window->parentWidget());
	if (parent == nullptr) { return; }

	if( forceShow ||
		m_workspace->activeSubWindow() != parent ||
		parent->isHidden() )
	{
		parent->show();
		window->show();
		if (window->isEnabled()) { window->setFocus(); }
		else { m_workspace->setActiveSubWindow(parent); }
	}
	else
	{
		parent->hide();
		refocus();
	}
}



void MainWindow::toggleFullscreen()
{
	if ( !isFullScreen() )
	{
		maximized = isMaximized();
		showFullScreen();
	}
	else
	{
		maximized ? showMaximized() : showNormal();
	}
}



/*
 * When an editor window with focus is toggled off, attempt to set focus
 * to the next visible editor window, or if none are visible, set focus
 * to the parent window.
 */
void MainWindow::refocus()
{
	const auto gui = getGUI();

	// Attempt to set the focus on the first of these editors that is not hidden...
	for (auto editorParent : { gui->songEditor()->parentWidget(), gui->patternEditor()->parentWidget(),
		gui->pianoRoll()->parentWidget(), gui->automationEditor()->parentWidget() })
	{
		if (!editorParent->isHidden())
		{
			editorParent->setFocus();
			return;
		}
	}

	// ... otherwise set the focus on the main window.
	this->setFocus();
}




void MainWindow::togglePatternEditorWin( bool forceShow )
{
	toggleWindow( getGUI()->patternEditor(), forceShow );
}




void MainWindow::toggleSongEditorWin()
{
	if (m_flLayout)
	{
		// Piano Roll / Automation Editor sit on top of the Playlist: F5 brings the Playlist back
		bool covered = false;
		for (QWidget* w : {subWindowOf(getGUI()->pianoRoll()), subWindowOf(getGUI()->automationEditor())})
		{
			if (isAttachedSubWindow(w) && w->isVisible()) { w->hide(); covered = true; }
		}
		if (covered)
		{
			toggleWindow(getGUI()->songEditor(), true);
			return;
		}
	}
	toggleWindow( getGUI()->songEditor() );
}




void MainWindow::toggleProjectNotesWin()
{
	toggleWindow( getGUI()->getProjectNotes() );
}




void MainWindow::togglePianoRollWin()
{
	layoutDockedWindows();
	toggleWindow( getGUI()->pianoRoll() );
}




void MainWindow::toggleAutomationEditorWin()
{
	layoutDockedWindows();
	toggleWindow( getGUI()->automationEditor() );
}




void MainWindow::toggleMixerWin()
{
	toggleWindow( getGUI()->mixerView() );
	scheduleLayout();
}



void MainWindow::toggleMicrotunerWin()
{
	toggleWindow( getGUI()->getMicrotunerConfig() );
}




void MainWindow::updateViewMenu()
{
	m_viewMenu->clear();
	// TODO: get current visibility for these and indicate in menu?
	// Not that it's straight visible <-> invisible, more like
	// not on top -> top <-> invisible
	m_viewMenu->addAction(embed::getIconPixmap( "songeditor" ),
			      tr( "Song Editor" ) + "\tF5",
			      this, SLOT(toggleSongEditorWin())
		);
	m_viewMenu->addAction(embed::getIconPixmap("pattern_track"),
					tr("Pattern Editor") + "\tF6",
					this, SLOT(togglePatternEditorWin())
		);
	m_viewMenu->addAction(embed::getIconPixmap( "piano" ),
			      tr( "Piano Roll" ) + "\tF7",
			      this, SLOT(togglePianoRollWin())
		);
	m_viewMenu->addAction(embed::getIconPixmap( "automation" ),
			      tr( "Automation Editor" ) + "\tF8",
			      this,
			      SLOT(toggleAutomationEditorWin())
		);
	m_viewMenu->addAction(embed::getIconPixmap( "mixer" ),
			      tr( "Mixer" ) + "\tF9",
			      this, SLOT(toggleMixerWin())
		);
	m_viewMenu->addAction(embed::getIconPixmap( "controller" ),
			      tr( "Controller Rack" ) + "\tCtrl+6",
			      this, SLOT(toggleControllerRack())
		);
	m_viewMenu->addAction(embed::getIconPixmap( "project_notes" ),
			      tr( "Project Notes" ) + "\tCtrl+7",
			      this, SLOT(toggleProjectNotesWin())
		);

	m_viewMenu->addSeparator();

	m_viewMenu->addAction(embed::getIconPixmap("organize"),
		tr("Organize windows") + "\t" + keySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_W).toString(QKeySequence::NativeText),
		this, SLOT(organizeWindows()));
	auto flLayoutAction = new QAction(tr("Docked FL layout"), this);
	flLayoutAction->setData("fllayout");
	flLayoutAction->setCheckable(true);
	flLayoutAction->setChecked(m_flLayout);
	m_viewMenu->addAction(flLayoutAction);
	auto oneWindowAction = new QAction(tr("One channel window at a time"), this);
	oneWindowAction->setData("oneinstrument");
	oneWindowAction->setCheckable(true);
	oneWindowAction->setChecked(ConfigManager::inst()->value("ui", "oneinstrumenttrackwindow").toInt());
	m_viewMenu->addAction(oneWindowAction);

	m_viewMenu->addSeparator();
	
	m_viewMenu->addAction(embed::getIconPixmap( "fullscreen" ),
				tr( "Fullscreen" ) + "\tF11",
				this, SLOT(toggleFullscreen())
		);

	m_viewMenu->addSeparator();

	auto detachAllAction = m_viewMenu->addAction(embed::getIconPixmap("detach"),
		tr("Detach all subwindows"),
		this, [this](){ setAllSubWindowsDetached(true); },
		QKeySequence{Qt::CTRL | Qt::SHIFT | Qt::Key_D}
	);
	auto attachAllAction = m_viewMenu->addAction(embed::getIconPixmap("detach"),
		tr("Attach all subwindows"),
		this, [this](){ setAllSubWindowsDetached(false); },
		QKeySequence{Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_D}
	);

	detachAllAction->setShortcutContext(Qt::ApplicationShortcut);
	attachAllAction->setShortcutContext(Qt::ApplicationShortcut);

	m_viewMenu->addSeparator();

	// Here we should put all look&feel -stuff from configmanager
	// that is safe to change on the fly. There is probably some
	// more elegant way to do this.
	auto qa = new QAction(tr("Smooth scroll"), this);
	qa->setData("smoothscroll");
	qa->setCheckable( true );
	qa->setChecked( ConfigManager::inst()->value( "ui", "smoothscroll" ).toInt() );
	m_viewMenu->addAction(qa);

	// Not yet.
	/* qa = new QAction(tr( "One instrument track window" ), this);
	qa->setData("oneinstrument");
	qa->setCheckable( true );
	qa->setChecked( ConfigManager::inst()->value( "ui", "oneinstrumenttrackwindow" ).toInt() );
	m_viewMenu->addAction(qa);
	*/

	qa = new QAction(tr( "Enable note labels in piano roll" ), this);
	qa->setData("printnotelabels");
	qa->setCheckable( true );
	qa->setChecked( ConfigManager::inst()->value( "ui", "printnotelabels" ).toInt() );
	m_viewMenu->addAction(qa);

}




void MainWindow::updateConfig( QAction * _who )
{
	QString tag = _who->data().toString();
	bool checked = _who->isChecked();

	if (tag == "tooltips")
	{
		ConfigManager::inst()->setValue( "tooltips", "disabled",
						 QString::number(!checked) );

		if (checked) { qApp->removeEventFilter(this); }
		else { qApp->installEventFilter(this); }

	}
	else if (tag == "fllayout")
	{
		setFlLayout(checked);
	}
	else if ( tag == "smoothscroll" )
	{
		ConfigManager::inst()->setValue( "ui", "smoothscroll",
						 QString::number(checked) );
	}
	else if ( tag == "oneinstrument" )
	{
		ConfigManager::inst()->setValue( "ui", "oneinstrumenttrackwindow",
						 QString::number(checked) );
	}
	else if ( tag == "printnotelabels" )
	{
		ConfigManager::inst()->setValue( "ui", "printnotelabels",
						 QString::number(checked) );
	}
}



void MainWindow::onToggleMetronome()
{
	Engine::getSong()->metronome().setActive(m_metronomeToggle->isChecked());
}




void MainWindow::toggleControllerRack()
{
	toggleWindow( getGUI()->getControllerRackView() );
}




void MainWindow::updatePlayPauseIcons()
{
	getGUI()->songEditor()->setPauseIcon( false );
	getGUI()->automationEditor()->setPauseIcon( false );
	getGUI()->patternEditor()->setPauseIcon( false );
	getGUI()->pianoRoll()->setPauseIcon( false );
	updateTransportButtons();

	if( Engine::getSong()->isPlaying() )
	{
		switch( Engine::getSong()->playMode() )
		{
			case Song::PlayMode::Song:
				getGUI()->songEditor()->setPauseIcon( true );
				break;

			case Song::PlayMode::AutomationClip:
				getGUI()->automationEditor()->setPauseIcon( true );
				break;

			case Song::PlayMode::Pattern:
				getGUI()->patternEditor()->setPauseIcon( true );
				break;

			case Song::PlayMode::MidiClip:
				getGUI()->pianoRoll()->setPauseIcon( true );
				break;

			default:
				break;
		}
	}
}


// ---------------------------------------------------------------------------
// FL-style global transport (PAT / SONG)
// ---------------------------------------------------------------------------

void MainWindow::createTransport()
{
	auto makeModeButton = [this](const QString& text, const QString& tip) {
		auto b = new QToolButton(m_toolBar);
		b->setText(text);
		b->setToolTip(tip);
		b->setCheckable(true);
		b->setObjectName("transportModeButton");
		b->setFixedSize(70, 28);
		b->setFocusPolicy(Qt::NoFocus);
		return b;
	};
	m_patModeButton = makeModeButton(tr("PAT"), tr("Pattern mode: Play plays the current pattern of the Channel Rack"));
	m_songModeButton = makeModeButton(tr("SONG"), tr("Song mode: Play plays the whole Playlist"));
	connect(m_patModeButton, &QToolButton::clicked, this, [this]() { setTransportMode(false); });
	connect(m_songModeButton, &QToolButton::clicked, this, [this]() { setTransportMode(true); });

	auto makeTransportButton = [this](const char* icon, const QString& tip, const char* name) {
		auto b = new QToolButton(m_toolBar);
		b->setIcon(embed::getIconPixmap(icon));
		b->setIconSize(QSize(22, 22));
		b->setToolTip(tip);
		b->setObjectName(name);
		b->setFixedSize(46, 56);
		b->setFocusPolicy(Qt::NoFocus);
		return b;
	};
	m_transportPlayButton = makeTransportButton("play", tr("Play / Stop (Space)"), "transportPlayButton");
	m_transportStopButton = makeTransportButton("stop", tr("Stop"), "transportStopButton");
	m_transportRecordButton = makeTransportButton("record", tr("Record"), "transportRecordButton");
	connect(m_transportPlayButton, &QToolButton::clicked, this, &MainWindow::transportPlayStop);
	connect(m_transportStopButton, &QToolButton::clicked, this, &MainWindow::transportStop);
	connect(m_transportRecordButton, &QToolButton::clicked, this, &MainWindow::transportRecord);

	// column 8 holds the "organize windows" button (added in finalize())
	m_toolBarLayout->setColumnMinimumWidth(9, 14);
	m_toolBarLayout->addWidget(m_patModeButton, 0, 10, Qt::AlignBottom);
	m_toolBarLayout->addWidget(m_songModeButton, 1, 10, Qt::AlignTop);
	m_toolBarLayout->setColumnMinimumWidth(11, 6);
	m_toolBarLayout->addWidget(m_transportPlayButton, 0, 12, 2, 1);
	m_toolBarLayout->addWidget(m_transportStopButton, 0, 13, 2, 1);
	m_toolBarLayout->addWidget(m_transportRecordButton, 0, 14, 2, 1);
	m_toolBarLayout->setColumnMinimumWidth(15, 8);
}




void MainWindow::updateTransportButtons()
{
	if (!m_patModeButton) { return; }
	m_patModeButton->setChecked(!m_songMode);
	m_songModeButton->setChecked(m_songMode);
	const bool playing = Engine::getSong()->isPlaying();
	m_transportPlayButton->setIcon(embed::getIconPixmap(playing ? "pause" : "play"));
	m_transportPlayButton->setProperty("playing", playing);
	m_transportPlayButton->style()->unpolish(m_transportPlayButton);
	m_transportPlayButton->style()->polish(m_transportPlayButton);
}




void MainWindow::setTransportMode(bool songMode, bool restartPlayback)
{
	if (m_songMode != songMode)
	{
		m_songMode = songMode;
		ConfigManager::inst()->setValue("ui", "transportsongmode", QString::number(songMode ? 1 : 0));
		const auto mode = Engine::getSong()->playMode();
		const bool playingOther = Engine::getSong()->isPlaying()
			&& (mode == Song::PlayMode::Song || mode == Song::PlayMode::Pattern);
		if (restartPlayback && playingOther)
		{
			Engine::getSong()->stop();
			transportPlayStop();
		}
		emit transportModeChanged(songMode);
	}
	updateTransportButtons();
}




void MainWindow::transportPlayStop()
{
	Editor* editor = m_songMode
		? static_cast<Editor*>(getGUI()->songEditor())
		: static_cast<Editor*>(getGUI()->patternEditor());
	editor->togglePlayStop();
}




void MainWindow::transportPause()
{
	Editor* editor = m_songMode
		? static_cast<Editor*>(getGUI()->songEditor())
		: static_cast<Editor*>(getGUI()->patternEditor());
	editor->togglePause();
}




void MainWindow::transportStop()
{
	Engine::getSong()->stop();
	getGUI()->pianoRoll()->stopRecording();
}




void MainWindow::transportRecord()
{
	// Recording happens on the timeline, so switch to SONG mode like FL does
	setTransportMode(true, false);
	if (QAction* rec = getGUI()->songEditor()->recordAction()) { rec->trigger(); }
}




void MainWindow::hideAllEditorWindows()
{
	for (QMdiSubWindow* sw : m_workspace->subWindowList())
	{
		sw->hide();
	}
	refocus();
}




// ---------------------------------------------------------------------------
// FL-style docked workspace
// ---------------------------------------------------------------------------

bool MainWindow::isDockedSubWindow(const QObject* o) const
{
	// Filled in finalize(), once all editors exist (the event filter runs much earlier)
	return o && std::find(m_dockedWindows.begin(), m_dockedWindows.end(), o) != m_dockedWindows.end();
}




QRect MainWindow::flMainArea() const
{
	const QSize area = m_workspace->viewport()->size();
	int mixerHeight = 0;
	QWidget* mixWin = subWindowOf(getGUI()->mixerView());
	if (m_flLayout && mixWin && mixWin->isVisible() && isAttachedSubWindow(mixWin) && !mixWin->isMaximized())
	{
		const int wanted = std::max({mixWin->minimumSizeHint().height(), mixWin->minimumHeight(), 300});
		mixerHeight = std::min(wanted, area.height() * 55 / 100);
	}
	return QRect(0, 0, area.width(), std::max(120, area.height() - mixerHeight));
}




void MainWindow::scheduleLayout()
{
	if (m_layoutScheduled || !m_flLayout) { return; }
	m_layoutScheduled = true;
	QTimer::singleShot(0, this, [this]() {
		m_layoutScheduled = false;
		layoutDockedWindows();
	});
}




void MainWindow::layoutDockedWindows()
{
	const auto gui = getGUI();
	if (!m_flLayout || !gui || !gui->mixerView()) { return; }
	const QSize area = m_workspace->viewport()->size();
	if (area.width() < 300 || area.height() < 200) { return; }

	const QRect main = flMainArea();

	// Mixer: docked along the bottom edge, full width
	QWidget* mixWin = subWindowOf(gui->mixerView());
	if (isAttachedSubWindow(mixWin) && mixWin->isVisible() && !mixWin->isMaximized())
	{
		mixWin->setGeometry(0, main.height(), area.width(), area.height() - main.height());
	}

	// Playlist, Piano Roll and Automation Editor share the main area (like tabs, switched with F5/F7/F8)
	for (QWidget* w : {subWindowOf(gui->songEditor()), subWindowOf(gui->pianoRoll()),
		subWindowOf(gui->automationEditor())})
	{
		if (isAttachedSubWindow(w) && !w->isMaximized() && w->geometry() != main) { w->setGeometry(main); }
	}

	// Floating windows must stay reachable after the workspace shrinks
	for (QMdiSubWindow* sw : m_workspace->subWindowList())
	{
		if (isDockedSubWindow(sw) || !sw->isVisible() || sw->isMaximized()) { continue; }
		const QRect g = sw->geometry();
		const int x = std::clamp(g.x(), 0, std::max(0, area.width() - std::min(g.width(), area.width())));
		const int y = std::clamp(g.y(), 0, std::max(0, area.height() - 40));
		if (x != g.x() || y != g.y()) { sw->move(x, y); }
	}
}




int MainWindow::channelRackAutoHeight() const
{
	const int rows = std::max<int>(4, static_cast<int>(Engine::patternStore()->tracks().size()));
	// title bar + two toolbars + ruler + one 32 px row per channel
	return 122 + 32 * rows;
}




void MainWindow::placeChannelRack()
{
	QWidget* rackWin = subWindowOf(getGUI()->patternEditor());
	if (!isAttachedSubWindow(rackWin)) { return; }
	const QRect main = flMainArea();
	const int w = std::min(900, main.width() - 40);
	const int h = std::min(channelRackAutoHeight(), main.height() - 56);
	const int x = std::max(12, (main.width() - w) / 2 - main.width() / 12);
	rackWin->setGeometry(x, 44, w, h);
	m_rackAutoHeight = rackWin->height();
}




void MainWindow::fitChannelRackHeight()
{
	// Grow/shrink the Channel Rack with its channels, unless the user resized it by hand
	QWidget* rackWin = subWindowOf(getGUI()->patternEditor());
	if (!m_flLayout || !isAttachedSubWindow(rackWin) || rackWin->isMaximized()) { return; }
	if (m_rackAutoHeight > 0 && rackWin->height() != m_rackAutoHeight) { return; }
	const QRect main = flMainArea();
	const int h = std::min(channelRackAutoHeight(), main.height() - rackWin->y() - 8);
	if (h > 150 && h != rackWin->height())
	{
		rackWin->resize(rackWin->width(), h);
	}
	m_rackAutoHeight = rackWin->height();
}




void MainWindow::placeNextToChannelRack(QWidget* w)
{
	if (!w) { return; }
	const QRect main = flMainArea();
	QSize s = w->size();
	if (s.width() < 100 || s.height() < 100) { s = w->sizeHint(); }
	QWidget* rackWin = subWindowOf(getGUI()->patternEditor());
	int x = main.width() - s.width() - 12;
	int y = 44;
	if (rackWin && rackWin->isVisible() && isAttachedSubWindow(rackWin))
	{
		x = rackWin->geometry().right() + 8;
		y = rackWin->geometry().top();
	}
	if (x + s.width() > main.width()) { x = std::max(0, main.width() - s.width() - 8); }
	y = std::clamp(y, 0, std::max(0, main.height() - s.height()));
	w->move(x, y);
}




void MainWindow::applyProjectLayout()
{
	if (!m_flLayout) { return; }
	const auto gui = getGUI();

	// Every project opens tidy: Playlist + Channel Rack, everything else closed
	for (QWidget* w : {subWindowOf(gui->pianoRoll()), subWindowOf(gui->automationEditor()),
		subWindowOf(gui->mixerView()), subWindowOf(gui->getControllerRackView()),
		subWindowOf(gui->getProjectNotes())})
	{
		if (w && isAttachedSubWindow(w)) { w->hide(); }
	}

	QWidget* songWin = subWindowOf(gui->songEditor());
	if (songWin->isMaximized()) { songWin->showNormal(); }
	songWin->show();
	gui->songEditor()->show();
	layoutDockedWindows();

	QWidget* rackWin = subWindowOf(gui->patternEditor());
	if (rackWin->isMaximized()) { rackWin->showNormal(); }
	placeChannelRack();
	rackWin->show();
	gui->patternEditor()->show();
	rackWin->raise();
	m_workspace->setActiveSubWindow(qobject_cast<QMdiSubWindow*>(rackWin));
}




void MainWindow::organizeWindows()
{
	if (!m_flLayout) { setFlLayout(true); return; }
	const auto gui = getGUI();
	QWidget* songWin = subWindowOf(gui->songEditor());
	if (isAttachedSubWindow(songWin))
	{
		if (songWin->isMaximized()) { songWin->showNormal(); }
		songWin->show();
		gui->songEditor()->show();
	}
	for (QWidget* w : {subWindowOf(gui->pianoRoll()), subWindowOf(gui->automationEditor()), subWindowOf(gui->mixerView())})
	{
		if (w && w->isMaximized()) { w->showNormal(); }
	}
	layoutDockedWindows();

	QWidget* rackWin = subWindowOf(gui->patternEditor());
	if (rackWin->isMaximized()) { rackWin->showNormal(); }
	placeChannelRack();
	rackWin->show();
	gui->patternEditor()->show();
	rackWin->raise();

	// Channel settings / effect windows: stack them neatly next to the Channel Rack
	int offset = 0;
	for (QMdiSubWindow* sw : m_workspace->subWindowList(QMdiArea::StackingOrder))
	{
		if (sw == rackWin || isDockedSubWindow(sw) || !sw->isVisible() || !isAttachedSubWindow(sw)) { continue; }
		if (sw->isMaximized()) { sw->showNormal(); }
		placeNextToChannelRack(sw);
		sw->move(sw->x() + offset, sw->y() + offset);
		offset += 24;
		sw->raise();
	}
}




void MainWindow::setFlLayout(bool on)
{
	m_flLayout = on;
	ConfigManager::inst()->setValue("ui", "fllayout", QString::number(on ? 1 : 0));
	if (on) { organizeWindows(); }
}



void MainWindow::updateUndoRedoButtons()
{
	// when the edit menu is shown, grey out the undo/redo buttons if there's nothing to undo/redo
	// else, un-grey them
	m_undoAction->setEnabled(Engine::projectJournal()->canUndo());
	m_redoAction->setEnabled(Engine::projectJournal()->canRedo());
}



void MainWindow::undo()
{
	Engine::projectJournal()->undo();
}




void MainWindow::redo()
{
	Engine::projectJournal()->redo();
}




void MainWindow::closeEvent( QCloseEvent * _ce )
{
	if( mayChangeProject(true) )
	{
		// delete recovery file
		if( ConfigManager::inst()->
				value( "ui", "enableautosave" ).toInt() )
		{
			sessionCleanup();
			_ce->accept();
		}
	}
	else
	{
		_ce->ignore();
	}
}




void MainWindow::sessionCleanup()
{
	// delete recover session files
	QFile::remove( ConfigManager::inst()->recoveryFile() );
	setSession( SessionState::Normal );
}




bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
	const auto isWinDeactivate = [](QEvent* event) -> bool
	{
		if (event->type() == QEvent::WindowDeactivate) { return true; }

		if (event->type() == QEvent::FocusOut)
		{
			auto* fe = static_cast<QFocusEvent*>(event);
			switch (fe->reason())
			{
			case Qt::ActiveWindowFocusReason: [[fallthrough]];
			case Qt::PopupFocusReason: [[fallthrough]];
			case Qt::OtherFocusReason:
				return true;
			default:
				break;
			}
		}

		return false;
	};

	// FL-style docked workspace: follow workspace resizes and keep docked windows in place
	if (m_workspace && watched == m_workspace->viewport())
	{
		if (event->type() == QEvent::Resize) { scheduleLayout(); }
		return QObject::eventFilter(watched, event);
	}
	if (isDockedSubWindow(watched))
	{
		auto sw = static_cast<QMdiSubWindow*>(watched);
		switch (event->type())
		{
			case QEvent::Show:
			case QEvent::Hide:
				scheduleLayout();
				break;
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonDblClick:
			case QEvent::MouseMove:
			case QEvent::MouseButtonRelease:
				// Docked windows can't be dragged or resized by their frame (use View > "Docked FL layout" to unlock)
				if (m_flLayout && isAttachedSubWindow(sw) && !sw->isMaximized())
				{
					if (event->type() == QEvent::MouseButtonPress) { m_workspace->setActiveSubWindow(sw); }
					sw->unsetCursor();
					return true;
				}
				break;
			default:
				break;
		}
		return QObject::eventFilter(watched, event);
	}

	// Clear modifiers when the window has been unfocused (install event filter on the window itself)
	if (dynamic_cast<MainWindow*>(watched) != nullptr && isWinDeactivate(event))
	{
		clearKeyModifiers();
		return QObject::eventFilter(watched, event);
	}

	// Block tooltips globally (install event filter on QApplication instance)
	if (event->type() == QEvent::ToolTip) { return true; }

	return QObject::eventFilter(watched, event);
}




void MainWindow::keyPressEvent( QKeyEvent * _ke )
{
	switch( _ke->key() )
	{
		case Qt::Key_Control: m_keyMods.m_ctrl = true; break;
		case Qt::Key_Shift: m_keyMods.m_shift = true; break;
		case Qt::Key_Alt: m_keyMods.m_alt = true; break;
		case Qt::Key_Space:
		{
			if (m_keyMods.m_shift) { transportPause(); }
			else { transportPlayStop(); }
			break;
		}
		default:
		{
			InstrumentTrackWindow * w =
						InstrumentTrackView::topLevelInstrumentTrackWindow();
			if( w )
			{
				w->pianoView()->keyPressEvent( _ke );
			}
			if( !_ke->isAccepted() )
			{
				QMainWindow::keyPressEvent( _ke );
			}
		}
	}
}




void MainWindow::keyReleaseEvent( QKeyEvent * _ke )
{
	switch( _ke->key() )
	{
		case Qt::Key_Control: m_keyMods.m_ctrl = false; break;
		case Qt::Key_Shift: m_keyMods.m_shift = false; break;
		case Qt::Key_Alt: m_keyMods.m_alt = false; break;
		default:
			if( InstrumentTrackView::topLevelInstrumentTrackWindow() )
			{
				InstrumentTrackView::topLevelInstrumentTrackWindow()->
					pianoView()->keyReleaseEvent( _ke );
			}
			if( !_ke->isAccepted() )
			{
				QMainWindow::keyReleaseEvent( _ke );
			}
	}
}




void MainWindow::timerEvent( QTimerEvent * _te)
{
	emit periodicUpdate();
}





void MainWindow::showTool( QAction * _idx )
{
	PluginView * p = m_tools[m_toolsMenu->actions().indexOf( _idx )];
	p->show();
	p->parentWidget()->show();
	p->setFocus();
}




void MainWindow::browseHelp()
{
	// file:// alternative for offline help
	QString url = "https://lmms.io/documentation/";
	QDesktopServices::openUrl( url );
	// TODO: Handle error
}




void MainWindow::autoSave()
{
	if( !Engine::getSong()->isExporting() &&
		!Engine::getSong()->isLoadingProject() &&
		!RemotePluginBase::isMainThreadWaiting() &&
		!QApplication::mouseButtons() &&
		( ConfigManager::inst()->value( "ui",
				"enablerunningautosave" ).toInt() ||
			! Engine::getSong()->isPlaying() ) )
	{
		Engine::getSong()->saveProjectFile(ConfigManager::inst()->recoveryFile());
		autoSaveTimerReset();  // Reset timer
	}
	else
	{
		// try again in 10 seconds
		if( getAutoSaveTimerInterval() != m_autoSaveShortTime )
		{
			autoSaveTimerReset( m_autoSaveShortTime );
		}
	}
}

void MainWindow::onExportProjectMidi()
{
	FileDialog efd( this );

	efd.setFileMode( FileDialog::AnyFile );

	QStringList types;
	types << tr("MIDI File (*.mid)");
	efd.setNameFilters( types );
	QString base_filename;
	QString const & projectFileName = Engine::getSong()->projectFileName();
	if( !projectFileName.isEmpty() )
	{
		efd.setDirectory( QFileInfo( projectFileName ).absolutePath() );
		base_filename = QFileInfo( projectFileName ).completeBaseName();
	}
	else
	{
		efd.setDirectory( ConfigManager::inst()->userProjectsDir() );
		base_filename = tr( "untitled" );
	}
	efd.selectFile( base_filename + ".mid" );
	efd.setDefaultSuffix( "mid");
	efd.setWindowTitle( tr( "Select file for project-export..." ) );

	efd.setAcceptMode( FileDialog::AcceptSave );


	if( efd.exec() == QDialog::Accepted && !efd.selectedFiles().isEmpty() && !efd.selectedFiles()[0].isEmpty() )
	{
		const QString suffix = ".mid";

		QString export_filename = efd.selectedFiles()[0];
		if (!export_filename.endsWith(suffix)) export_filename += suffix;

		Engine::getSong()->exportProjectMidi(PathUtil::stringToPath(export_filename));
	}
}

void MainWindow::exportProject(bool multiExport)
{
	QString const & projectFileName = Engine::getSong()->projectFileName();

	FileDialog efd( getGUI()->mainWindow() );

	if ( multiExport )
	{
		efd.setFileMode( FileDialog::Directory);
		efd.setWindowTitle( tr( "Select directory for writing exported tracks..." ) );
		if( !projectFileName.isEmpty() )
		{
			efd.setDirectory( QFileInfo( projectFileName ).absolutePath() );
		}
	}
	else
	{
		efd.setFileMode( FileDialog::AnyFile );
		int idx = 0;
		QStringList types;
		while( ProjectRenderer::fileEncodeDevices[idx].m_fileFormat != ProjectRenderer::ExportFileFormat::Count)
		{
			if(ProjectRenderer::fileEncodeDevices[idx].isAvailable()) {
				types << tr(ProjectRenderer::fileEncodeDevices[idx].m_description);
			}
			++idx;
		}
		efd.setNameFilters( types );
		QString baseFilename;
		if( !projectFileName.isEmpty() )
		{
			efd.setDirectory( QFileInfo( projectFileName ).absolutePath() );
			baseFilename = QFileInfo( projectFileName ).completeBaseName();
		}
		else
		{
			efd.setDirectory( ConfigManager::inst()->userProjectsDir() );
			baseFilename = tr( "untitled" );
		}
		efd.selectFile( baseFilename + ProjectRenderer::fileEncodeDevices[0].m_extension );
		efd.setWindowTitle( tr( "Select file for project-export..." ) );
	}

	QString suffix = "wav";
	efd.setDefaultSuffix( suffix );
	efd.setAcceptMode( FileDialog::AcceptSave );

	if( efd.exec() == QDialog::Accepted && !efd.selectedFiles().isEmpty() &&
					 !efd.selectedFiles()[0].isEmpty() )
	{

		QString exportFileName = efd.selectedFiles()[0];
		if ( !multiExport )
		{
			int stx = efd.selectedNameFilter().indexOf( "(*." );
			int etx = efd.selectedNameFilter().indexOf( ")" );

			if ( stx > 0 && etx > stx )
			{
				// Get first extension from selected dropdown.
				// i.e. ".wav" from "WAV-File (*.wav), Dummy-File (*.dum)"
				suffix = efd.selectedNameFilter().mid( stx + 2, etx - stx - 2 ).split( " " )[0].trimmed();

				Qt::CaseSensitivity cs = Qt::CaseSensitive;
#if defined(LMMS_BUILD_APPLE) || defined(LMMS_BUILD_WIN32)
				cs = Qt::CaseInsensitive;
#endif
				exportFileName.remove( "." + suffix, cs );
				if ( efd.selectedFiles()[0].endsWith( suffix ) )
				{
					if( VersionedSaveDialog::fileExistsQuery( exportFileName + suffix,
							tr( "Save project" ) ) )
					{
						exportFileName += suffix;
					}
				}
			}
		}

		ExportProjectDialog epd(exportFileName,
			multiExport ? ExportProjectDialog::Mode::ExportTracks : ExportProjectDialog::Mode::ExportProject,
			getGUI()->mainWindow());
		epd.exec();
	}
}

void MainWindow::handleSaveResult(QString const & filename, bool songSavedSuccessfully)
{
	if (songSavedSuccessfully)
	{
		TextFloat::displayMessage( tr( "Project saved" ), tr( "The project %1 is now saved.").arg( filename ),
				embed::getIconPixmap( "project_save", 24, 24 ), 2000 );
		ConfigManager::inst()->addRecentlyOpenedProject(filename);
		resetWindowTitle();
	}
	else
	{
		TextFloat::displayMessage( tr( "Project NOT saved." ), tr( "The project %1 was not saved!" ).arg(filename),
				embed::getIconPixmap( "error" ), 4000 );
	}
}

bool MainWindow::guiSaveProject()
{
	Song * song = Engine::getSong();
	bool const songSaveResult = song->guiSaveProject();
	handleSaveResult(song->projectFileName(), songSaveResult);

	return songSaveResult;
}

bool MainWindow::guiSaveProjectAs( const QString & filename )
{
	Song * song = Engine::getSong();
	bool const songSaveResult = song->guiSaveProjectAs(filename);
	handleSaveResult(filename, songSaveResult);

	return songSaveResult;
}

void MainWindow::onExportProject()
{
	this->exportProject();
}

void MainWindow::onExportProjectTracks()
{
	this->exportProject(true);
}

void MainWindow::onImportProject()
{
	Song * song = Engine::getSong();

	if (song)
	{
		FileDialog ofd( nullptr, tr( "Import file" ),
				ConfigManager::inst()->userProjectsDir(),
				tr("MIDI sequences") +
				" (*.mid *.midi *.rmi);;" +
				tr("Hydrogen projects") +
				" (*.h2song);;" +
				tr("All file types") +
				" (*.*)");

		ofd.setFileMode( FileDialog::ExistingFiles );
		if( ofd.exec () == QDialog::Accepted && !ofd.selectedFiles().isEmpty() )
		{
			ImportFilter::import( ofd.selectedFiles()[0], song );
		}

		song->setLoadOnLaunch(false);
	}
}

void MainWindow::onSongModified()
{
	// Only update the window title if the code is executed from the GUI main thread.
	// The assumption seems to be that the Song can also be set as modified from other
	// threads. This is not a good design! Copied from the original implementation of
	// Song::setModified.
	if(QThread::currentThread() == this->thread())
	{
		this->resetWindowTitle();
	}
}

void MainWindow::onProjectFileNameChanged()
{
	this->resetWindowTitle();
}


MainWindow::MovableQMdiArea::MovableQMdiArea(QWidget* parent) :
	QMdiArea(parent),
	m_isBeingMoved(false),
	m_lastX(0),
	m_lastY(0)
{}

void MainWindow::MovableQMdiArea::mousePressEvent(QMouseEvent* event)
{
	const auto pos = position(event);
	m_lastX = pos.x();
	m_lastY = pos.y();
	m_isBeingMoved = true;
	setCursor(Qt::ClosedHandCursor);
}

void MainWindow::MovableQMdiArea::mouseMoveEvent(QMouseEvent* event)
{
	if (m_isBeingMoved == false) { return; }

	int minXBoundary = window()->width() - 100;
	int maxXBoundary = 100;
	int minYBoundary = window()->height() - 100;
	int maxYBoundary = 100;

	int minX = minXBoundary;
	int maxX = maxXBoundary;
	int minY = minYBoundary;
	int maxY = maxYBoundary;

	auto subWindows = subWindowList();
	for (auto* curWindow : subWindows)
	{
		if (curWindow->isVisible())
		{
			minX = std::min(minX, curWindow->x());
			maxX = std::max(maxX, curWindow->x() + curWindow->width());
			minY = std::min(minY, curWindow->y());
			maxY = std::max(maxY, curWindow->y() + curWindow->height());
		}
	}

	const auto pos = position(event);
	int scrollX = m_lastX - pos.x();
	int scrollY = m_lastY - pos.y();

	scrollX = scrollX < 0 && minX >= minXBoundary ? 0 : scrollX;
	scrollX = scrollX > 0 && maxX <= maxXBoundary ? 0 : scrollX;
	scrollY = scrollY < 0 && minY >= minYBoundary ? 0 : scrollY;
	scrollY = scrollY > 0 && maxY <= maxYBoundary ? 0 : scrollY;

	for (auto* curWindow : subWindows)
	{
		// if widgets are maximized, then they shouldn't be moved
		// moving a maximized window's normalGeometry is not implemented because of difficulties
		if (curWindow->isMaximized() == false)
		{
			curWindow->move(curWindow->x() - scrollX, curWindow->y() - scrollY);
		}
	}

	m_lastX = pos.x();
	m_lastY = pos.y();
}

void MainWindow::MovableQMdiArea::mouseReleaseEvent(QMouseEvent* event)
{
	setCursor(Qt::ArrowCursor);
	m_isBeingMoved = false;
}

} // namespace lmms::gui
