#ifndef AWINDOW_H
#define AWINDOW_H
//============================================================================
//
// AWindow
//
//============================================================================


#include <QMainWindow>


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

class AWindow : public QMainWindow
{
    Q_OBJECT

public:

    AWindow();
    ~AWindow();
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
    QPushButton* _waveType[4];
    QSlider* _param[23];
    QLabel*  _paramReadout[23];
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
    AWindow( const AWindow & ) : QMainWindow( 0 ) {}
    AWindow &operator=( const AWindow & ) { return *this; }
};


#endif  //AWINDOW_H
