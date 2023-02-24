#include "MriDataProvider.h"
#include "Model/model_builder.hpp"

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

MriDataProvider::MriDataProvider() {
    this->directory = "";
    this->data = vtkSmartPointer<vtkImageData>::New();
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
    picking_base_point = -1;
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
