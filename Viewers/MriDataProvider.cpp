#include "MriDataProvider.h"
#include "Model/model_builder.hpp"
#include "Points/layout_10_20.hpp"

#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkMath.h>
#include <vtkTransform.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <iostream>
#include <thread>


static void buildModelInThread() {
    MriDataProvider::getInstance().buildModel();
}


bool MriDataProvider::setDirectory(QString dir) {
    directory = dir.toStdString();
    // Убираем "file://"
    directory.erase(directory.begin(), directory.begin() + 7);

    if(model_actor)
        model_viewer->getRenderer()->removeActor(model_actor);

    // Запускаем построение модели в отдельном потоке
    std::thread t1(buildModelInThread);

    // Сохраняем данные исследования для vtk
    if(!this->readDirectoryVtk()) {
        t1.join();
        return false;
    }

    // Обновление данных в просмотрщиках проекций (plane_viewer)
    for(int i = 0; i != 3; ++i)
        plane_viewer[i]->getRenderer()->setData(data);

    // Обновление данных в просмотрщиках проекций (plane_widget)
    model_viewer->getRenderer()->setData(data);

    // Обновляем размерность для слайдеров
    slices_0 = data->GetDimensions()[0];
    slices_1 = data->GetDimensions()[1];
    slices_2 = data->GetDimensions()[2];
    emit changedSlices_0();
    emit changedSlices_1();
    emit changedSlices_2();

    // Обновляем занчение ширины полосы интенсивностей
    double range[2];
    data->GetPointData()->GetScalars()->GetRange(range);
    windowRange = range[1] - range[0];
    emit windowRangeChanged();

    // Задаем значения window-level в plane_widget
    model_viewer->getRenderer()->setWindow(windowRange);
    model_viewer->getRenderer()->setLevel(windowRange / 2);

    // Дожидаемся выполнения потока
    t1.join();

    // Передаем полученную модель в просмотр
    model_viewer->getRenderer()->addActor(model_actor);

    return true;
}

void MriDataProvider::setWindow(int value) {
    for(int i = 0; i != 3; ++i)
        plane_viewer[i]->getRenderer()->setWindow(value);
    model_viewer->getRenderer()->setWindow(value);
}

void MriDataProvider::setLevel(int value) {
    for(int i = 0; i != 3; ++i)
        plane_viewer[i]->getRenderer()->setLevel(value);
    model_viewer->getRenderer()->setLevel(value);
}

void MriDataProvider::pickBasePoint(int point) {
    picking_base_point = point;
    for(int i = 0; i != 3; ++i) {
        plane_viewer[i]->getRenderer()->pickingOn();
    }
}

void MriDataProvider::buildPoints10_20() {
    // Проверка существования базовых точек
    for(int i = 0; i != 4; ++i) {
        if(!pointIsInitialized(base_points[i]))
            return;
    }
    // Инициализация ценра
    LAYOUT_10_20::centerOfMass(base_points[0],
                               base_points[1],
                               base_points[2],
                               base_points[3],
                               base_points[4]);
    // Строим деревья
    if(!obb_tree) {
        obb_tree = vtkSmartPointer<vtkOBBTree>::New();
        obb_tree->SetDataSet(model);
        obb_tree->BuildLocator();
    }
    if(!kd_tree) {
        kd_tree = vtkSmartPointer<vtkKdTreePointLocator>::New();
        kd_tree->SetDataSet(model);
        kd_tree->BuildLocator();
    }
    // Получение точек 10-20
    points10_20 = LAYOUT_10_20::mark(model, kd_tree, obb_tree, base_points[0], base_points[1],
                                     base_points[2], base_points[3], base_points[4]);
    // Отметка их на 3д
    for(int i = 0; i != points10_20->GetNumberOfPoints(); ++i) {
        if(points10_20actors[i])
            model_viewer->getRenderer()->removeActor(points10_20actors[i]);
        double point[3];
        points10_20->GetPoint(i, point);
        points10_20actors[i] = LAYOUT_10_20::pointActor(point, 0, 0, 1);
        model_viewer->getRenderer()->addActor(points10_20actors[i]);
    }

    double test_point_0[3];
    double test_point_1[3];
    double test_point_2[3];

    getPoint10_20("F3", test_point_0);
    getPoint10_20("Pz", test_point_1);
    getPoint10_20("T4", test_point_2);

    std::cout << test_point_0[0] << " " << test_point_0[1] << " " << test_point_0[2] << std::endl;
    std::cout << test_point_1[0] << " " << test_point_1[1] << " " << test_point_1[2] << std::endl;
    std::cout << test_point_2[0] << " " << test_point_2[1] << " " << test_point_2[2] << std::endl;
}

MriDataProvider::MriDataProvider() {
    this->directory = "";
    this->data = vtkSmartPointer<vtkImageData>::New();
    this->initMap10_20();
}

MriDataProvider& MriDataProvider::getInstance() {
    static MriDataProvider provider;
    return provider;
}

void MriDataProvider::addPlaneViewer(QVTKPlaneViewerItem *viewer) {
    plane_viewer[viewer->orientation] = viewer;
}

void MriDataProvider::addModelViewer(QVTKModelViewerItem *item) {
    model_viewer = item;
}

void MriDataProvider::buildModel() {
    model = MODEL_BUILDER::build(directory, model_directory, model_filename);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(model);

    model_actor = vtkSmartPointer<vtkActor>::New();
    model_actor->SetMapper(mapper);
    model_actor->GetProperty()->SetDiffuseColor(0.93, 0.71, 0.63);
}

void MriDataProvider::setBasePoint(double* point) {
    if(picking_base_point == -1)
        return;
    for(int i = 0; i != 3; ++i) {
        // Обновляем значение
        base_points[picking_base_point][i] = point[i];
        // Выключаем пикинг в plane_viewer'ах
        plane_viewer[i]->getRenderer()->pickingOff();
    }
    // Если уже была отмечена точка - удаляем
    if(base_points_actors[picking_base_point])
        model_viewer->getRenderer()->removeActor(base_points_actors[picking_base_point]);
    // Отмечаем новую
    base_points_actors[picking_base_point] = LAYOUT_10_20::pointActor(point);
    model_viewer->getRenderer()->addActor(base_points_actors[picking_base_point]);
    // Сбрасываем режим выбора точки
    picking_base_point = -1;
}

void MriDataProvider::getPoint10_20(int index, double point[3]) {
    if(points10_20)
        points10_20->GetPoint(index, point);
}

void MriDataProvider::getPoint10_20(const std::string& name, double point[3]) {
    if(points10_20)
        points10_20->GetPoint(points10_20map[name], point);
}

bool MriDataProvider::readDirectoryVtk() {
    // Нужно вернуть к стандартному значению, иначе qt ультует vtk
    setlocale(LC_ALL, "C");
    // Читаем директорию
    this->UpdateInformation();
    this->SetDirectoryName(directory.c_str());
    this->Update();
    if(this->GetNumberOfDICOMFileNames() < 20) {
        std::cout << "Недостаточно входных данных в папке: ";
        std::cout << this->GetNumberOfDICOMFileNames()<< std::endl;
        return false;
    }

    // Ищем граничные координаты
    double bounds[6];
    this->GetOutput()->GetBounds(bounds);

    // Поворот набора точек - подготовка к последующей трансформации
    vtkNew<vtkImageReslice> flip;
    flip->SetInputData(this->GetOutput());
    flip->SetResliceAxesOrigin(0, (bounds[3] - bounds[2]), (bounds[5] - bounds[4]));
    flip->SetResliceAxesDirectionCosines(1,0,0, 0,-1,0, 0,0,-1);
    flip->Update();

    vtkNew<vtkMatrix4x4> matrix;
    // Составляем матрицу для перевода точек из системы координат vtk в dicom'овские
    float *position = this->GetImagePositionPatient();
    float *orientation = this->GetImageOrientationPatient();
    float *xdir = &orientation[0];
    float *ydir = &orientation[3];
    float zdir[3];
    vtkMath::Cross(xdir, ydir, zdir);
    for(int i = 0; i != 3; ++i) {
        matrix->Element[i][0] = xdir[i];
        matrix->Element[i][1] = ydir[i];
        matrix->Element[i][2] = zdir[i];
        matrix->Element[i][3] = position[i];
    }
    matrix->Element[3][0] = 0;
    matrix->Element[3][1] = 0;
    matrix->Element[3][2] = 0;
    matrix->Element[3][3] = 1;
    matrix->Modified();
    matrix->Invert();

    // Длеаем трансофрмацию с матрицей
    vtkNew<vtkTransform> tr;
    tr->SetMatrix(matrix);
    tr->Update();

    // monke flip
    vtkNew<vtkImageReslice> monke;
    monke->SetInputData(flip->GetOutput());
    monke->SetResliceTransform(tr);
    monke->SetInterpolationModeToLinear();
    monke->AutoCropOutputOn();
    monke->Update();

    // Сохраняем данные
    this->data->DeepCopy(monke->GetOutput());
    return true;
}

void MriDataProvider::setSlice(int i, int slice) {
    plane_viewer[i]->getRenderer()->setSlice(slice);
    model_viewer->getRenderer()->setSlice(i, slice);
}

bool MriDataProvider::pointIsInitialized(double* point) {
    if(point[0] == 0 && point[1] == 0 && point[2] == 0)
        return false;
    return true;
}

void MriDataProvider::initMap10_20() {
    points10_20map.clear();
    points10_20map["Fpz"] = 0;
    points10_20map["Fz"] = 1;
    points10_20map["Cz"] = 2;
    points10_20map["Pz"] = 3;
    points10_20map["Oz"] = 4;
    points10_20map["T3"] = 5;
    points10_20map["C3"] = 6;
    points10_20map["C4"] = 7;
    points10_20map["T4"] = 8;
    points10_20map["F7"] = 9;
    points10_20map["Fp1"] = 10;
    points10_20map["Fp2"] = 11;
    points10_20map["F8"] = 12;
    points10_20map["F3"] = 13;
    points10_20map["F4"] = 14;
    points10_20map["T5"] = 15;
    points10_20map["O1"] = 16;
    points10_20map["O2"] = 17;
    points10_20map["T6"] = 18;
    points10_20map["P3"] = 19;
    points10_20map["P4"] = 20;
}
