#include <QAbstractItemModel>
#include <QDir>

class FilesModel : public QAbstractItemModel
{
public:

    FilesModel(QObject* parent = nullptr) : QAbstractItemModel(parent) {}
    void setDirectory(const QString& path, const QString& filter = QString());

    QString filePath(const QModelIndex& index) {
        quintptr id = index.internalId();
        if (id < (unsigned long) _files.size())
            return _directory + _files[id];
        return QString();
    }

    int columnCount(const QModelIndex& /*parent*/) const {
        return 1;
    }

    int rowCount(const QModelIndex& parent) const {
        return parent.isValid() ? 0 : _files.size();
    }

    QVariant data(const QModelIndex& index, int role) const {
        if (role == Qt::DisplayRole) {
            quintptr id = index.internalId();
            if (index.column() == 0 && id < (unsigned long) _files.size())
                return _files[id];
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation, int role) const {
        if (section == 0 && role == Qt::DisplayRole)
            return "File Name";
        return QVariant();
    }

    QModelIndex index(int row, int column, const QModelIndex& parent) const {
        if (parent.isValid())
            return QModelIndex();
        return createIndex(row, column, quintptr(row));
    }

    QModelIndex parent(const QModelIndex& /*index*/) const {
        return QModelIndex();
    }

    //Qt::ItemFlags flags(const QModelIndex& index ) const;
    //bool removeRows( int row, int count, const QModelIndex& parent );

protected:
    QString _directory;
    QStringList _files;
};


void FilesModel::setDirectory(const QString& path, const QString& filter)
{
    _directory = path;
    if (! path.isEmpty() && path.back() != QDir::separator())
        _directory.push_back(QDir::separator());

    QDir dir(path, filter, QDir::Name, QDir::Files);
    QFileInfoList list = dir.entryInfoList();
    if (list.empty() && _files.empty())
        return;

    beginResetModel();
    _files.clear();
    for (int i = 0; i < list.size(); ++i) {
        _files.push_back( list.at(i).fileName() );
    }
    endResetModel();
}


#ifdef UNIT_TEST
// g++ -DUNIT_TEST FilesModel.cpp -lQt5Core -lQt5Gui -lQt5Widgets
// -I/usr/include/qt5 -I/usr/include/qt5/QtCore -I/usr/include/qt5/QtWidgets
#include <QApplication>
#include <QListView>
int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QListView w;
    FilesModel* files = new FilesModel(&w);
    w.setModel(files);
    w.show();
    files->setDirectory("/tmp", "*.wav");

    return app.exec();
}
#endif
