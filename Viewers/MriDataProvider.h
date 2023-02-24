#ifndef MRI_DATA_PROVIDER_H
#define MRI_DATA_PROVIDER_H


#include "QVTKPlaneViewer.h"
#include "QVTKModelViewer.h"
#include <string>
#include <QObject>
#include <QString>
#include <vtkDICOMImageReader.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>


class MriDataProvider: 
    public QObject,
    private vtkDICOMImageReader {

    Q_OBJECT
    Q_PROPERTY(int slices_0 READ getSlices_0 WRITE setSlices_0 NOTIFY changedSlices_0)
    Q_PROPERTY(int slices_1 READ getSlices_1 WRITE setSlices_1 NOTIFY changedSlices_1)
    Q_PROPERTY(int slices_2 READ getSlices_2 WRITE setSlices_2 NOTIFY changedSlices_2)
    Q_PROPERTY(int windowRange READ getWindowRange WRITE setWindowRange NOTIFY windowRangeChanged)
public:
    int slices_0 = 100;
    int slices_1 = 100;
    int slices_2 = 100;
    int windowRange = 1000;
    void setSlices_0(const int &s) {slices_0 = s;}
    void setSlices_1(const int &s) {slices_1 = s;}
    void setSlices_2(const int &s) {slices_2 = s;}
    void setWindowRange(const int &w) {windowRange = w;}
    int getSlices_0() const {return slices_0;}
    int getSlices_1() const {return slices_1;}
    int getSlices_2() const {return slices_2;}
    int getWindowRange() const {return windowRange;}
signals:
    void changedSlices_0();
    void changedSlices_1();
    void changedSlices_2();
    void windowRangeChanged();

public slots:
    bool setDirectory(QString directory);
    void setSlice_0(const int&s) {setSlice(0, s);}
    void setSlice_1(const int&s) {setSlice(1, s);}
    void setSlice_2(const int&s) {setSlice(2, s);}
    void setWindow(int value);
    void setLevel(int level);
    void pickBasePoint(int point);

private:
    MriDataProvider();

public:
    static MriDataProvider& getInstance();
    MriDataProvider(MriDataProvider const&) = delete;
    void operator= (MriDataProvider const&) = delete;
    ~MriDataProvider() = default;

public:
    // Добавление ссылок на объекты, использующие провайдера
    void addPlaneViewer(QVTKPlaneViewerItem* plane_viewer);
    void addModelViewer(QVTKModelViewerItem* item);
    // Выполнение построения актера модели
    void buildModel();
    // Дергается из plane_viewer'a
    // Сохраняет значение пикнутой точки
    void setBasePoint(double* point);

private:
    // Чтение директории с исследованием с преобразованием координат
    bool readDirectoryVtk();
    // Задание номера среза в объектах, использующих провайдера
    void setSlice(int i, int slice);

private:
    // Директория с исследованием
    std::string directory;
    // Объемные данные (volume data)
    vtkSmartPointer<vtkImageData> data;

    // Ссылки на объекты, использующие провайдера
    QVTKModelViewerItem* model_viewer;
    QVTKPlaneViewerItem* plane_viewer[3];

    // Директория и имя сохраняемой модели
    std::string model_directory = "/home/boiii/models";
    std::string model_filename = "model";
    // Модель и актер головы в vtk
    vtkSmartPointer<vtkPolyData> model;
    vtkSmartPointer<vtkActor> model_actor;

    // Координаты базовых точек (инион, насион, козелок левый и правый)
    double base_points[4][3] = {0};
    // Номер точки, выбор которой происходит на данный момент
    int picking_base_point = -1;
};

#endif // MRI_DATA_PROVIDER_H
