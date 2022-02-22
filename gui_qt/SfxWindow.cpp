/*
  sfx_gen Qt GUI

  Copyright 2022 Karl Robillard

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include <cassert>
#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSlider>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include "SfxWindow.h"
#include "version.h"
#include "audio.h"
#include "sfx_gen.h"
#include "saveWave.h"

#include "FilesModel.cpp"

#if 1
#include "well512.h"
static Well512 rng;
#define SEED_RNG(S) well512_init(&rng, S)
#else
static QRandomGenerator rng;
#define SEED_RNG(S) rng.seed(S)
#endif

// Random function required by sfx_gen.
extern "C" int sfx_random(int range)
{
#ifdef WELL512_H
    return well512_genU32(&rng) % range;
#else
    return rng.bounded(range);
#endif
}

// Audio wave data
struct Wave
{
    uint32_t frameCount;    // Total number of frames (considering channels)
    uint16_t sampleRate;    // Frequency (samples per second)
    uint16_t sampleSize;    // Bit depth (bits per sample): 8, 16, 32
    uint16_t channels;      // Number of channels (1-mono, 2-stereo, ...)
    float*   data;          // Buffer data pointer
};

#define MAX_WAVE_SLOTS  4
struct WaveTables {
    uint32_t bufId[MAX_WAVE_SLOTS];
    int srcId[MAX_WAVE_SLOTS];
    Wave wave[MAX_WAVE_SLOTS];
    SfxParams params[MAX_WAVE_SLOTS];
    SfxParams clip;
};

#define GEN_COUNT   9
static const char* genName[] = {
    "Pickup/Coin",
    "Laser/Shoot",
    "Explosion",
    "PowerUp",
    "Hit/Hurt",
    "Jump",
    "Blip/Select",
    "Mutate",
    "Randomize"
};

#define WFORM_COUNT 5
static const char* wformName[] = {
    "Square",
    "Sawtooth",
    "Sinewave",
    "Noise",
    "Triangle"
};

static const char* wformIcon[] = {
    ":/icons/wave-square.svg",
    ":/icons/wave-sawtooth.svg",
    ":/icons/wave-sine.svg",
    ":/icons/wave-noise.svg",
    ":/icons/wave-triangle.svg"
};

static void addGeneratorButton(QWidget* parent, int id, QBoxLayout* lo)
{
    QPushButton* btn = new QPushButton(genName[id]);
    btn->setProperty("gid", id);
    parent->connect(btn, SIGNAL(clicked(bool)), SLOT(generateSound()));
    lo->addWidget(btn);
}

void SfxWindow::layoutGenerators(QBoxLayout* plo)
{
    QBoxLayout* lo = new QVBoxLayout;
    int i;

    lo->setSpacing(4);

    for (i = 0; i < GEN_COUNT-2; ++i)
        addGeneratorButton(this, i, lo);
    lo->addSpacing(8);

    QButtonGroup* grp = new QButtonGroup(this);
    QPushButton* btn;
    for (i = 0; i < WFORM_COUNT; ++i) {
        btn = _waveType[i] = new QPushButton(wformName[i]);
        btn->setIcon(QIcon(wformIcon[i]));
        btn->setCheckable(true);
        btn->setFlat(true);
        grp->addButton(btn, i);
        lo->addWidget(btn);
    }
    grp->button(0)->setChecked(true);
    connect(grp, SIGNAL(idToggled(int,bool)), SLOT(chooseWaveForm(int,bool)));
    lo->addSpacing(8);

    for (i = GEN_COUNT-2; i < GEN_COUNT; ++i)
        addGeneratorButton(this, i, lo);

    lo->addStretch(1);

    plo->addLayout(lo);
}

void SfxWindow::addSlider(QGridLayout* grid, int row, const char* label,
                          int neg)
{
    QSlider* slid = _param[row] = new QSlider(Qt::Horizontal);
    slid->setProperty("pid", row);
    slid->setTracking(false);

    const char* ltext;
    if (row == PARAM_VOL) {
        slid->setRange(0, 100);
        slid->setPageStep(10);
        connect(slid, SIGNAL(valueChanged(int)), SLOT(volumeChanged(int)));
        ltext = "0.00";
    } else {
        slid->setRange(neg ? -400 : 0, 400);
        slid->setPageStep(40);
        connect(slid, SIGNAL(valueChanged(int)), SLOT(paramChanged(int)));
        ltext = "0.000";
    }

    QLabel* vlabel = _paramReadout[row] = new QLabel(ltext);
    grid->addWidget(new QLabel(label), row, 1, Qt::AlignLeft);
    grid->addWidget(slid, row, 2);
    grid->addWidget(vlabel, row, 3, Qt::AlignRight);
}

static const char* paramName[PARAM_COUNT] = {
    "Volume",
    "Attack time",      // ENVELOPE
    "Sustain time",
    "Sustain punch",
    "Decay time",
    "Start",            // FREQUENCY
    "Minimum",
    "Amount",           // SLIDE
    "Delta",
    "Depth",            // VIBRATO
    "Speed",
    "Change",           // TONE
    "Change speed",
    "Duty",             // SQUARE
    "Duty sweep",
    "Speed",            // REPEAT
    "Offset",           // PHASER
    "Sweep",
    "Cutoff",           // LPF
    "Cutoff sweep",
    "Resonance",
    "Cutoff",           // HPF
    "Cutoff sweep"
};

static uint32_t paramNegative = SFX_NEGATIVE_ONE_MASK << 1;

void SfxWindow::layoutParams(QBoxLayout* plo)
{
    QGridLayout* grid = new QGridLayout;
    grid->setSpacing(4);

    grid->addWidget(new QLabel("ENVELOPE"),  1, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("FREQUENCY"), 5, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("SLIDE"),     7, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("VIBRATO"),   9, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("TONE"),     11, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("SQUARE"),   13, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("REPEAT"),   15, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("PHASER"),   16, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("LPF"),      18, 0, Qt::AlignRight);
    grid->addWidget(new QLabel("HPF"),      21, 0, Qt::AlignRight);

    for (int i = 0; i < PARAM_COUNT; ++i)
        addSlider(grid, i, paramName[i], paramNegative & (1 << i));

    QWidget* label = grid->itemAtPosition(1, 0)->widget();
    int lwidth = label->fontMetrics().horizontalAdvance("-0.333");
    grid->setColumnMinimumWidth(3, lwidth);
    grid->setRowStretch(PARAM_COUNT, 1);

    plo->addLayout(grid, 2);
}

SfxWindow::SfxWindow()
{
    int i, vol;

    setWindowTitle(APP_NAME);

    _paramAssign = true;
    _playOnChange = true;
    _activeWav = 0;

    _synth = sfx_allocSynth(SFX_F32, 44100, 10);

    _wav = new WaveTables;
    for (i = 0; i < MAX_WAVE_SLOTS; ++i) {
        _wav->bufId[i] = 0;
        _wav->srcId[i] = 0;
        memset(_wav->wave + i, 0, sizeof(Wave));
        sfx_resetParams(_wav->params + i);
    }
    _wav->clip.waveType = -1;

    createActions();
    createMenus();
    createTools();

    QWidget* mainWid = new QWidget;
    QBoxLayout* lo = new QVBoxLayout(mainWid);
    QBoxLayout* loH = new QHBoxLayout;
    lo->addLayout(loH);
    layoutGenerators(loH);
    layoutParams(loH);

    _files = new FilesModel(this);
    QListView* flist = new QListView;
    flist->setUniformItemSizes(true);
    flist->setModel(_files);
    connect(flist, SIGNAL(activated(const QModelIndex&)),
                    SLOT(chooseFile(const QModelIndex&)));
    loH->addWidget(flist);

    _wavePix = QPixmap(640, 58);
    _wavePic = new QLabel;
    _wavePic->setPixmap(_wavePix);
    lo->addWidget(_wavePic, 0, Qt::AlignHCenter);

    loH = new QHBoxLayout;
    lo->addLayout(loH);
    for (i = 0; i < 3; ++i) {
        _stats[i] = new QLabel;
        loH->addWidget(_stats[i], 0, Qt::AlignHCenter);
    }

    setCentralWidget(mainWid);

    {
    QSettings settings;
    resize(settings.value("window-size", QSize(700, 480)).toSize());
    _prevProjPath  = settings.value("prev-project").toString();
    vol = settings.value("volume", 100).toInt();
    _actPoc->setChecked(settings.value("play-on-change", true).toBool());
    }

    if (aud_startup()) {
        aud_genBuffers(MAX_WAVE_SLOTS, _wav->bufId);
    } else {
        QMessageBox::critical(this, "Audio System", "aud_startup() failed!\n");
    }

    _param[0]->setValue(vol);
    updateParameterWidgets(_wav->params);

    SEED_RNG( QRandomGenerator::global()->generate() );
}


SfxWindow::~SfxWindow()
{
    aud_stopAll();
    aud_freeBuffers(MAX_WAVE_SLOTS, _wav->bufId);
    aud_shutdown();

    free(_synth);

    for (int i = 0; i < MAX_WAVE_SLOTS; ++i)
        free(_wav->wave[i].data);
    delete _wav;
}

void SfxWindow::closeEvent( QCloseEvent* ev )
{
    QSettings settings;
    settings.setValue("window-size", size());
    settings.setValue("prev-project", _prevProjPath);
    settings.setValue("volume", _param[0]->value());
    settings.setValue("play-on-change", _playOnChange);

    QMainWindow::closeEvent( ev );
}

void SfxWindow::showAbout()
{
    QMessageBox::information( this, "About " APP_NAME,
        "Version " APP_VERSION "\n\nCopyright (c) 2022 Karl Robillard" );
}


void SfxWindow::createActions()
{
#define STD_ICON(id)    style()->standardIcon(QStyle::id)

    _actOpen = new QAction(STD_ICON(SP_DialogOpenButton), "&Open...", this );
    _actOpen->setShortcuts(QKeySequence::Open);
    connect(_actOpen, SIGNAL(triggered()), SLOT(open()));

    _actSave = new QAction(STD_ICON(SP_DialogSaveButton), "&Save", this );
    _actSave->setShortcuts(QKeySequence::Save);
    _actSave->setEnabled(false);
    connect(_actSave, SIGNAL(triggered()), SLOT(save()));

    _actSaveAs = new QAction("Save &As", this);
    _actSaveAs->setShortcuts(QKeySequence::SaveAs);
    connect(_actSaveAs, SIGNAL(triggered()), SLOT(saveAs()));

    _actPlay = new QAction(STD_ICON(SP_MediaPlay), "&Play Sound", this );
    _actPlay->setShortcut(Qt::Key_Space);
    connect(_actPlay, SIGNAL(triggered()), SLOT(playSound()));

    _actPoc = new QAction(STD_ICON(SP_MediaVolume), "Play on change", this);
    _actPoc->setCheckable(true);
    connect(_actPoc, SIGNAL(toggled(bool)), SLOT(setPoc(bool)));
}


void SfxWindow::createMenus()
{
    QMenu* menu;
    QMenuBar* bar = menuBar();

    menu = bar->addMenu( "&File" );
    menu->addAction(_actOpen);
    menu->addAction(_actSave);
    menu->addAction(_actSaveAs);
    menu->addSeparator();
    menu->addAction("&Quit", this, SLOT(close()), QKeySequence::Quit);

    menu = bar->addMenu( "&Edit" );
    menu->addAction("&Copy",  this, SLOT(copy()),  QKeySequence::Copy);
    _actPaste =
    menu->addAction("&Paste", this, SLOT(paste()), QKeySequence::Paste);
    menu->addSeparator();
    menu->addAction("&Mutate",  this, SLOT(mutate()),
                    QKeySequence(Qt::Key_F3));
    menu->addAction("&Randomize", this, SLOT(randomize()),
                    QKeySequence(Qt::Key_F4));

    bar->addSeparator();

    menu = bar->addMenu( "&Help" );
    menu->addAction("&About", this, SLOT(showAbout()));

    _actPaste->setEnabled(false);
}


void SfxWindow::createTools()
{
    QAction* act;
    QToolButton* btn;

    _tools = addToolBar("");
    _tools->addAction(_actOpen);
    _tools->addAction(_actSave);
    _tools->addSeparator();
    _tools->addAction(_actPlay);
    _tools->addAction(_actPoc);

    btn = qobject_cast<QToolButton *>(_tools->widgetForAction(_actPlay));
    if (btn)
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    _tools->addWidget(new QLabel("  Slot:"));

    QButtonGroup* grp = new QButtonGroup(this);
    for (int i = 0; i < MAX_WAVE_SLOTS; ++i) {
        btn = new QToolButton;
        btn->setText(QString::number(i+1));
        btn->setCheckable(true);
        grp->addButton(btn, i);
        _tools->addWidget(btn);

        act = new QAction(this);
        act->setShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + i)));
        connect(act, SIGNAL(triggered(bool)), btn, SLOT(click()));
        addAction(act);
    }
    grp->button(0)->setChecked(true);
    connect(grp, SIGNAL(idToggled(int,bool)), SLOT(chooseWaveSlot(int,bool)));
}


void SfxWindow::setProjectFile( const QString& file )
{
    _prevProjPath = file;
    setWindowTitle(file + " - " APP_NAME);
    _actSave->setEnabled(true);
}


#define UTF8(str)   str.toUtf8().constData()

bool SfxWindow::open(const QString& file, bool updateList)
{
    SfxParams* sp = _wav->params + _activeWav;
    const char* err = sfx_loadParams(sp, UTF8(file), NULL);
    if (err) {
        QMessageBox::warning(this, "Load Error", file + ":\n" + err);
        return false;
    } else {
        setProjectFile(file);
        updateParameterWidgets(sp);
        regenerate(false);

        if (updateList) {
            QFileInfo info(file);
            _files->setDirectory(info.absolutePath(), "*.rfx");
        }
        return true;
    }
}


void SfxWindow::open()
{
    QString fn;
    QString path(_prevProjPath);

    fn = QFileDialog::getOpenFileName(this, "Open Parameters", path,
                                      "Parameters (*.rfx *.sfs)");
    if (! fn.isEmpty())
        open(fn, true);
}


bool SfxWindow::saveRfx(const QString& file)
{
    const char* err = sfx_saveRfx(_wav->params + _activeWav, UTF8(file));
    if (err) {
        QMessageBox::warning(this, "RFX Save Error", file + ":\n" + err);
        return false;
    }
    return true;
}


void SfxWindow::save()
{
    if (_actSave->isEnabled() && ! _prevProjPath.isEmpty())
        saveRfx(_prevProjPath);
}


bool SfxWindow::saveWaveFile(const Wave* wav, const QString& file)
{
    int16_t* pcm;
    int16_t* it;
    const char* err;
    uint32_t bytes = wav->frameCount * sizeof(int16_t);

    pcm = (int16_t*) malloc(bytes);
    if (pcm) {
        const float* samples = (const float*) wav->data;
        const float* send = samples + wav->frameCount;
        for (it = pcm; samples != send; ++samples)
            *it++ = (int16_t) (samples[0] * 32767.0f);  // -32768 to 32767.

        err = saveWave(pcm, bytes, wav->sampleRate, 16, wav->channels,
                       UTF8(file));
        free(pcm);

        if (err) {
            QMessageBox::warning(this, "WAVE Save Error", file + ":\n" + err);
            return false;
        }
        return true;
    }
    return false;
}


void SfxWindow::saveAs()
{
    QString fn;
    QString path(_prevProjPath);

    fn = QFileDialog::getSaveFileName(this, "Save Sound As", path,
                                      "Parameters (*.rfx);;Wave (*.wav)");
    if (! fn.isEmpty()) {
        if (fn.endsWith(".wav", Qt::CaseInsensitive)) {
            saveWaveFile(_wav->wave + _activeWav, fn);
        } else {
            if (saveRfx(fn)) {
                setProjectFile(fn);

                QFileInfo info(fn);
                _files->setDirectory(info.absolutePath(), "*.rfx");
            }
        }
    }
}


void SfxWindow::copy()
{
    _wav->clip = _wav->params[_activeWav];
    _actPaste->setEnabled(true);
}


void SfxWindow::paste()
{
    if (_wav->clip.waveType >= 0) {
        SfxParams* sp = _wav->params + _activeWav;
        *sp = _wav->clip;
        updateParameterWidgets(sp);
        regenerate(false);
    }
}


void SfxWindow::setPoc(bool on)
{
    _playOnChange = on;
}


void SfxWindow::playSound()
{
    int i = _activeWav;
    _wav->srcId[i] = aud_playSound(_wav->bufId[i]);
}


void SfxWindow::generateSound()
{
    int gid = sender()->property("gid").toInt();
    if (gid < 0)
        return;

    if (gid < 7) {
        SfxParams* sp = _wav->params + _activeWav;

        uint32_t seed = QRandomGenerator::global()->generate();
        SEED_RNG(seed);
        switch (gid) {
            case 0: sfx_genPickupCoin(sp);  break;
            case 1: sfx_genLaserShoot(sp);  break;
            case 2: sfx_genExplosion(sp);   break;
            case 3: sfx_genPowerup(sp);     break;
            case 4: sfx_genHitHurt(sp);     break;
            case 5: sfx_genJump(sp);        break;
            case 6: sfx_genBlipSelect(sp);  break;
        }
        sp->randSeed = seed;

        updateParameterWidgets(sp);
        regenerate(true);
    }
    else if (gid == 7)
        mutate();
    else if (gid == 8)
        randomize();
}


void SfxWindow::mutate()
{
    SfxParams* sp = _wav->params + _activeWav;
    sfx_mutate(sp, 0.1f, 0xffffdf);
    updateParameterWidgets(sp);
    regenerate(true);
}


void SfxWindow::randomize()
{
    SfxParams* sp = _wav->params + _activeWav;

    uint32_t seed = QRandomGenerator::global()->generate();
    SEED_RNG(seed);
    sfx_genRandomize(sp, sfx_random(4));
    sp->randSeed = seed;

    updateParameterWidgets(sp);
    regenerate(true);
}


void SfxWindow::updateParameterWidgets(const SfxParams* sp)
{
    const float* fval = &sp->attackTime;
    _paramAssign = false;
    _waveType[ sp->waveType ]->setChecked(true);
    for (int p = 1; p < PARAM_COUNT; ++p) {
        _param[p]->setValue(fval[0] * 400.0f);
        ++fval;
    }
    _paramAssign = true;
}


static void drawWave(QPixmap* pix, const Wave* wave)
{
    int width = pix->width();
    int halfH = pix->height() / 2;
    int sy0, sy1, sy2;
    float samplePos = 0.0f;
    float sampleInc = float(wave->frameCount * wave->channels) / float(width);
    float sampleLast = float(wave->frameCount - 1);
    float oneThirdInc = sampleInc / 3.0f;
    float twoThirdInc = 2.0f * oneThirdInc;
    float sampleScale = float(halfH);
    const float* wdata = (const float*) wave->data;

    pix->fill(QColor(0,0x22,0x2b));

    QPainter p(pix);
    p.setPen(QColor(255,165,60,100));

#define SAMPLE_Y(pos)   halfH + int(wdata[int(pos)] * sampleScale)

    sy0 = SAMPLE_Y(0);

    for (int x = 0; x < width; x++) {
        sy1 = SAMPLE_Y(samplePos + oneThirdInc);
        sy2 = SAMPLE_Y(samplePos + twoThirdInc);

        p.drawLine(x, sy0, x, sy1);
        p.drawLine(x, sy1, x, sy2);

        samplePos += sampleInc;
        if (samplePos > sampleLast)
            samplePos = sampleLast;

        sy0 = SAMPLE_Y(samplePos);
        p.drawLine(x, sy2, x, sy0);
    }

    p.setPen(QColor(0x81,0xa0,0xb0, 0x90));
    p.drawLine(0, halfH, width-1, halfH);
}


void SfxWindow::updateStats(const Wave* wdat)
{
    drawWave(&_wavePix, wdat);
    _wavePic->setPixmap(_wavePix);

    _stats[0]->setText(QString::asprintf("Frames: %d", wdat->frameCount));
    _stats[1]->setText(QString::asprintf("Duration: %d ms",
                                wdat->frameCount * 1000 / wdat->sampleRate));
    _stats[2]->setText(QString::asprintf("Size: %d bytes", wdat->frameCount*2));
}


// Update audio buffer.
void SfxWindow::regenerate(bool play)
{
    int i = _activeWav;
    Wave* wdat = _wav->wave + i;

    int scount = sfx_generateWave(_synth, _wav->params + i);

    // Copy sample data to Wave struct.
    size_t bytes = scount * sizeof(float);
    if (uint32_t(scount) > wdat->frameCount) {
        free(wdat->data);
        wdat->data = (float*) malloc(bytes);
    }
    memcpy(wdat->data, _synth->samples.f, bytes);

    wdat->frameCount = scount;
    wdat->sampleRate = _synth->sampleRate;
    wdat->sampleSize = 32;
    wdat->channels   = 1;

    // Copy sample data to audio system.
    if (_wav->srcId[i]) {
        // Must stop all as multiple sources may be attached to our buffer.
        aud_stopAll();
    }
    aud_loadBufferF32(_wav->bufId[i], _synth->samples.f, scount, 0,
                      _synth->sampleRate);
    if (play)
        _wav->srcId[i] = aud_playSound(_wav->bufId[i]);

    updateStats(wdat);
}


void SfxWindow::chooseWaveSlot(int i, bool checked)
{
    if (checked) {
        _activeWav = i & 3;

        updateParameterWidgets(_wav->params + _activeWav);

        const Wave* wave = _wav->wave + _activeWav;
        if (wave->data) {
            playSound();
            updateStats(wave);
        } else
            regenerate(true);
    }
}


void SfxWindow::chooseWaveForm(int wform, bool checked)
{
    if (checked) {
        //printf("KR wform %d\n", wform);
        if (_paramAssign) {
            _wav->params[_activeWav].waveType = wform;
            if (_playOnChange)
                regenerate(true);
        }
    }
}


void SfxWindow::chooseFile(const QModelIndex& mi)
{
    QString fn = _files->filePath(mi);
    if (! fn.isEmpty()) {
        if (open(fn, false) && _playOnChange)
            playSound();
    }
}


void SfxWindow::volumeChanged(int value)
{
    float fv = float(value) * 0.01f;
    _paramReadout[PARAM_VOL]->setText(QString::number(fv, 'f', 2));
    aud_setSoundVolume(fv);
    if (_playOnChange)
        playSound();
}


void SfxWindow::paramChanged(int value)
{
    int pid = sender()->property("pid").toInt();
    float fv = float(value) * 0.0025f;
    _paramReadout[pid]->setText(QString::number(fv, 'f', 3));

    if (_paramAssign) {
        float* fval = &_wav->params[_activeWav].attackTime;
        fval[pid - 1] = fv;
        //printf( "KR pc %d %d\n", pid, value);

        if (_playOnChange)
            regenerate(true);
    }
}


//----------------------------------------------------------------------------


int main( int argc, char **argv )
{
    QApplication app( argc, argv );
    app.setOrganizationName( APP_NAME );
    app.setApplicationName( APP_NAME );

    SfxWindow w;
    w.show();

    if( argc > 1 )
        w.open(argv[1], true);
    else
        w.regenerate(false);

    return app.exec();
}


//EOF
