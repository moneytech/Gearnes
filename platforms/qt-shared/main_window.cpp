/*
 * Gearnes - NES / Famicom Emulator
 * Copyright (C) 2015  Ignacio Sanchez Gines

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include <QFileDialog>
#include <QDesktopWidget>
#include <QSettings>
#include "ui_MainWindow.h"
#include "gl_frame.h"
#include "main_window.h"
#include "emulator.h"
#include "input_settings.h"
#include "sound_settings.h"
#include "video_settings.h"
#include "about.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    qApp->installEventFilter(this);
    fullscreen_ = false;
    screen_size_ = 2;

    menu_pressed_[0] = menu_pressed_[1] = menu_pressed_[2] = false;
    ui_ = new Ui::MainWindow();
    ui_->setupUi(this);

    this->addAction(ui_->actionFullscreen);
    this->addAction(ui_->actionReset);
    this->addAction(ui_->actionPause);
    this->addAction(ui_->actionSave_State);
    this->addAction(ui_->actionLoad_State);
    
    this->setWindowTitle(GEARNES_TITLE);

    exit_shortcut_ = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    exit_shortcut_->setContext(Qt::ApplicationShortcut);
    QObject::connect(exit_shortcut_, SIGNAL(activated()), this, SLOT(Exit()));


    QObject::connect(ui_->menuGame_Boy, SIGNAL(aboutToShow()), this, SLOT(MainMenuPressed()));
    QObject::connect(ui_->menuGame_Boy, SIGNAL(aboutToHide()), this, SLOT(MainMenuReleased()));
    QObject::connect(ui_->menuDebug, SIGNAL(aboutToShow()), this, SLOT(MenuDebugPressed()));
    QObject::connect(ui_->menuDebug, SIGNAL(aboutToHide()), this, SLOT(MenuDebugReleased()));
    QObject::connect(ui_->menuSettings, SIGNAL(aboutToShow()), this, SLOT(MenuSettingsPressed()));
    QObject::connect(ui_->menuSettings, SIGNAL(aboutToHide()), this, SLOT(MenuSettingsReleased()));

    ui_->actionX_1->setData(1);
    ui_->actionX_2->setData(2);
    ui_->actionX_3->setData(3);
    ui_->actionX_4->setData(4);
    ui_->actionX_5->setData(5);

    emulator_ = new Emulator();
    emulator_->Init();

    QGLFormat format;
    format.setSwapInterval(1);
    QGLFormat::setDefaultFormat(format);

    gl_frame_ = new GLFrame();
    ResizeWindow(screen_size_);
    setCentralWidget(gl_frame_);

    input_settings_ = new InputSettings(gl_frame_);

    sound_settings_ = new SoundSettings(gl_frame_, emulator_);

    video_settings_ = new VideoSettings(gl_frame_, emulator_);

    about_ = new About();

    QPalette palette = this->palette();
    palette.setColor(this->backgroundRole(), Qt::black);
    this->setPalette(palette);

    LoadSettings();

    gl_frame_->InitRenderThread(emulator_);
}

MainWindow::~MainWindow()
{
    SaveSettings();

    SafeDelete(about_);
    SafeDelete(exit_shortcut_);
    SafeDelete(emulator_);
    SafeDelete(gl_frame_);
    SafeDelete(input_settings_);
    SafeDelete(sound_settings_);
    SafeDelete(ui_);
}

void MainWindow::Exit()
{
#ifdef DEBUG_GEARNES
    emulator_->MemoryDump();
#endif
    this->close();
}

void MainWindow::MenuLoadROM()
{
    gl_frame_->PauseRenderThread();

    QString file_name = QFileDialog::getOpenFileName(
            this,
            tr("Load ROM"),
            QDir::currentPath(),
            tr("NES / Famicom ROM files (*.nes *.zip);;All files (*.*)"));

    if (!file_name.isNull())
    {
        emulator_->LoadRom(file_name.toUtf8().data());
        ui_->actionPause->setChecked(false);
    }

    setFocus();
    activateWindow();

    gl_frame_->ResumeRenderThread();
}

void MainWindow::MenuPause()
{
    if (emulator_->IsPaused())
        emulator_->Resume();
    else
        emulator_->Pause();
}

void MainWindow::MenuReset()
{
    ui_->actionPause->setChecked(false);
    emulator_->Reset();
}

void MainWindow::MenuSelectStateSlot()
{
}

void MainWindow::MenuSaveState()
{
}

void MainWindow::MenuLoadState()
{
}

void MainWindow::MenuSaveStateAs()
{
}

void MainWindow::MenuLoadStateFrom()
{
}

void MainWindow::MenuSettingsInput()
{
    gl_frame_->PauseRenderThread();
    input_settings_->show();
}

void MainWindow::MenuSettingsVideo()
{
    gl_frame_->PauseRenderThread();
    video_settings_->show();
}

void MainWindow::MenuSettingsSound()
{
    gl_frame_->PauseRenderThread();
    sound_settings_->show();
}

void MainWindow::MenuSettingsWindowSize(QAction* action)
{
    ui_->actionX_1->setChecked(false);
    ui_->actionX_2->setChecked(false);
    ui_->actionX_3->setChecked(false);
    ui_->actionX_4->setChecked(false);
    ui_->actionX_5->setChecked(false);
    action->setChecked(true);
    screen_size_ = action->data().toInt();
    ResizeWindow(screen_size_);
}

void MainWindow::MenuSettingsFullscreen()
{
    if (fullscreen_)
    {
        fullscreen_ = false;
        this->showNormal();
        ui_->menubar->show();
        ResizeWindow(screen_size_);
        gl_frame_->move(0, 0);
    }
    else
    {
        fullscreen_ = true;

        this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        this->setMinimumSize(0, 0);
        this->showFullScreen();

        ui_->menubar->hide();

        int current_width = qApp->desktop()->size().width();
        int current_height = qApp->desktop()->size().height();

        int scaling_factor = current_height / Gearnes::NES_HEIGHT;

        gl_frame_->setMaximumSize(Gearnes::NES_WIDTH * scaling_factor, Gearnes::NES_HEIGHT * scaling_factor);
        gl_frame_->setMinimumSize(Gearnes::NES_WIDTH * scaling_factor, Gearnes::NES_HEIGHT * scaling_factor);

        int offset_x = (current_width - (Gearnes::NES_WIDTH * scaling_factor)) / 2;
        int offset_y = (current_height - (Gearnes::NES_HEIGHT * scaling_factor)) / 2;
        gl_frame_->setGeometry(offset_x, offset_y, Gearnes::NES_WIDTH * scaling_factor, Gearnes::NES_HEIGHT * scaling_factor);
    }

    setFocus();
    activateWindow();
}

void MainWindow::MenuDebugDisassembler()
{
}

void MainWindow::MenuDebugOAM()
{
}

void MainWindow::MenuDebugMap()
{
}

void MainWindow::MenuDebugPalette()
{
}

void MainWindow::MenuAbout()
{
    about_->setModal(true);
    about_->show();
}

void MainWindow::MainMenuPressed()
{
    menu_pressed_[0] = true;
    gl_frame_->PauseRenderThread();
}

void MainWindow::MainMenuReleased()
{
    menu_pressed_[0] = false;
    MenuReleased();
}

void MainWindow::MenuSettingsPressed()
{
    menu_pressed_[1] = true;
    gl_frame_->PauseRenderThread();
}

void MainWindow::MenuSettingsReleased()
{
    menu_pressed_[1] = false;
    MenuReleased();
}

void MainWindow::MenuDebugPressed()
{
    menu_pressed_[2] = true;
    gl_frame_->PauseRenderThread();
}

void MainWindow::MenuDebugReleased()
{
    menu_pressed_[2] = false;
    MenuReleased();
}

void MainWindow::MenuReleased()
{
    for (int i = 0; i < 3; i++)
    {
        if (menu_pressed_[i])
            return;
    }
    gl_frame_->ResumeRenderThread();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    gl_frame_->StopRenderThread();
    QMainWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
    {
        Gearnes::NES_Joypads joypad = Gearnes::kJoypad1;
        switch (input_settings_->GetKey(event->key()))
        {
            case 0:
                emulator_->KeyPressed(joypad, Gearnes::kKeyUp);
                break;
            case 3:
                emulator_->KeyPressed(joypad, Gearnes::kKeyLeft);
                break;
            case 1:
                emulator_->KeyPressed(joypad, Gearnes::kKeyRight);
                break;
            case 2:
                emulator_->KeyPressed(joypad, Gearnes::kKeyDown);
                break;
            case 6:
                emulator_->KeyPressed(joypad, Gearnes::kKeyStart);
                break;
            case 7:
                emulator_->KeyPressed(joypad, Gearnes::kKeySelect);
                break;
            case 5:
                emulator_->KeyPressed(joypad, Gearnes::kKeyA);
                break;
            case 4:
                emulator_->KeyPressed(joypad, Gearnes::kKeyB);
                break;
            default:
                break;
        }
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat())
    {
        Gearnes::NES_Joypads joypad = Gearnes::kJoypad1;
        switch (input_settings_->GetKey(event->key()))
        {
            case 0:
                emulator_->KeyReleased(joypad, Gearnes::kKeyUp);
                break;
            case 3:
                emulator_->KeyReleased(joypad, Gearnes::kKeyLeft);
                break;
            case 1:
                emulator_->KeyReleased(joypad, Gearnes::kKeyRight);
                break;
            case 2:
                emulator_->KeyReleased(joypad, Gearnes::kKeyDown);
                break;
            case 6:
                emulator_->KeyReleased(joypad, Gearnes::kKeyStart);
                break;
            case 7:
                emulator_->KeyReleased(joypad, Gearnes::kKeySelect);
                break;
            case 5:
                emulator_->KeyReleased(joypad, Gearnes::kKeyA);
                break;
            case 4:
                emulator_->KeyReleased(joypad, Gearnes::kKeyB);
                break;
            default:
                break;
        }
    }
}

void MainWindow::ResizeWindow(int factor)
{
    screen_size_ = factor;
    gl_frame_->setMaximumSize(Gearnes::NES_WIDTH * factor, Gearnes::NES_HEIGHT * factor);
    gl_frame_->setMinimumSize(Gearnes::NES_WIDTH * factor, Gearnes::NES_HEIGHT * factor);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ApplicationActivate)
    {
        gl_frame_->ResumeRenderThread();
    }
    else if (event->type() == QEvent::ApplicationDeactivate)
    {
        gl_frame_->PauseRenderThread();
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest)
    {
        if (!fullscreen_)
        {
            this->setMaximumSize(sizeHint());
            this->setMinimumSize(sizeHint());
            this->resize(sizeHint());
        }
    }

    return QMainWindow::event(event);
}

void MainWindow::LoadSettings()
{
    QSettings settings("gearnes.ini", QSettings::IniFormat);

    settings.beginGroup("Gearnes");
    screen_size_ = settings.value("ScreenSize", 2).toInt();

    switch (screen_size_)
    {
        case 1:
            MenuSettingsWindowSize(ui_->actionX_1);
            break;
        case 2:
            MenuSettingsWindowSize(ui_->actionX_2);
            break;
        case 3:
            MenuSettingsWindowSize(ui_->actionX_3);
            break;
        case 4:
            MenuSettingsWindowSize(ui_->actionX_4);
            break;
        case 5:
            MenuSettingsWindowSize(ui_->actionX_5);
            break;
    }

    fullscreen_ = !settings.value("FullScreen", false).toBool();

    MenuSettingsFullscreen();
    settings.endGroup();

    settings.beginGroup("Input");
    input_settings_->LoadSettings(settings);
    settings.endGroup();
    settings.beginGroup("Video");
    video_settings_->LoadSettings(settings);
    settings.endGroup();
    settings.beginGroup("Sound");
    sound_settings_->LoadSettings(settings);
    settings.endGroup();
}

void MainWindow::SaveSettings()
{
    QSettings settings("gearnes.ini", QSettings::IniFormat);

    settings.beginGroup("Gearnes");
    settings.setValue("ScreenSize", screen_size_);
    settings.setValue("FullScreen", fullscreen_);
    settings.endGroup();

    settings.beginGroup("Input");
    input_settings_->SaveSettings(settings);
    settings.endGroup();
    settings.beginGroup("Video");
    video_settings_->SaveSettings(settings);
    settings.endGroup();
    settings.beginGroup("Sound");
    sound_settings_->SaveSettings(settings);
    settings.endGroup();

    emulator_->SaveRam();
}

