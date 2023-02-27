#ifndef MRI_DATA_PROVIDER_H
#define MRI_DATA_PROVIDER_H


#include "QVTKPlaneViewer.h"
#include "QVTKModelViewer.h"
#include <map>
#include <string>
#include <QObject>
#include <QString>
#include <vtkDICOMImageReader.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkKdTreePointLocator.h>
#include <vtkOBBTree.h>


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
    void buildPoints10_20();

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
    // Получение точек 10-20 по индексу
    void getPoint10_20(int index, double point[3]);
    // Получение точек 10-20 по названию через мапу
    void getPoint10_20(const std::string& name, double point[3]);

private:
    // Чтение директории с исследованием с преобразованием координат
    bool readDirectoryVtk();
    // Задание номера среза в объектах, использующих провайдера
    void setSlice(int i, int slice);
    // Проверка, задана точка или нет
    bool pointIsInitialized(double* point);
    // Создает мапу для удобного доступа к точкам
    void initMap10_20();

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

    // Деревья для построения точек
    vtkSmartPointer<vtkKdTreePointLocator> kd_tree;
    vtkSmartPointer<vtkOBBTree> obb_tree;

    // Координаты базовых точек (инион, насион, козелок левый, правый, центр)
    double base_points[5][3] = {0};
    // Актеры базовых точек, отображаемых на 3д виде
    vtkSmartPointer<vtkActor> base_points_actors[4];
    // Номер точки, выбор которой происходит на данный момент
    int picking_base_point = -1;

    // Точки 10-20
    vtkSmartPointer<vtkPoints> points10_20;
    vtkSmartPointer<vtkActor> points10_20actors[21];
    std::map<std::string, int> points10_20map;
};

#endif // MRI_DATA_PROVIDER_H
