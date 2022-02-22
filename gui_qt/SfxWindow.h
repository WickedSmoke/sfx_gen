#ifndef SFX_WINDOW_H
#define SFX_WINDOW_H
/*
  sfx_gen Qt GUI

  Copyright 2022 Karl Robillard

  This program may be used under the terms of the GPLv3 license
  (see SfxWindow.cpp).
*/


#include <QMainWindow>

#define PARAM_VOL   0
#define PARAM_COUNT 23

struct SfxParams;
struct SfxSynth;
struct Wave;
struct WaveTables;
class FilesModel;
class QBoxLayout;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;

class SfxWindow : public QMainWindow
{
    Q_OBJECT

public:

    SfxWindow();
    ~SfxWindow();
    bool open(const QString& file, bool updateList);

public slots:

    void regenerate(bool play);
    void showAbout();

protected:

    virtual void closeEvent( QCloseEvent* );

private slots:

    void open();
    void save();
    void saveAs();
    void copy();
    void paste();
    void setPoc(bool on);
    void playSound();
    void generateSound();
    void mutate();
    void randomize();
    void chooseWaveSlot(int, bool checked);
    void chooseWaveForm(int, bool checked);
    void chooseFile(const QModelIndex&);
    void volumeChanged(int);
    void paramChanged(int);

private:

    void createActions();
    void createMenus();
    void createTools();
    void addSlider(QGridLayout* grid, int row, const char* label, int low);
    void layoutGenerators(QBoxLayout*);
    void layoutParams(QBoxLayout*);
    void updateParameterWidgets(const SfxParams*);
    void updateStats(const Wave*);
    void setProjectFile(const QString&);
    bool saveWaveFile(const Wave*, const QString&);
    bool saveRfx(const QString&);

    QAction* _actOpen;
    QAction* _actSave;
    QAction* _actSaveAs;
    QAction* _actPaste;
    QAction* _actPlay;
    QAction* _actPoc;

    QToolBar* _tools;
    QPushButton* _waveType[6];
    QSlider* _param[PARAM_COUNT];
    QLabel*  _paramReadout[PARAM_COUNT];
    QLabel*  _wavePic;
    QPixmap  _wavePix;
    QLabel*  _stats[3];
    FilesModel* _files;

    QString _prevProjPath;
    SfxSynth* _synth;
    WaveTables* _wav;
    int _activeWav;
    bool _paramAssign;
    bool _playOnChange;

    // Disabled copy constructor and operator=
    SfxWindow( const SfxWindow & ) : QMainWindow( 0 ) {}
    SfxWindow &operator=( const SfxWindow & ) { return *this; }
};


#endif  //AWINDOW_H
