#include "strech_grid.hpp"
#include <vtkTriangle.h>
#include <vtkTransform.h>
#include <vtkAppendPolyData.h>
#include <vtkSphereSource.h>


namespace {
    vtkSmartPointer<vtkPoints> baseGrid(int num_of_points, double spacing) {
        vtkNew<vtkPoints> points;
        double x = 0;
        double y = 0;
        double z = 0;
        // Подсовываем нулевую точку (не вписывается в алгоритм)
        points->InsertPoint(0, x , y , z);

        // Длина текущей стороны
        double length = 1.0;
        // Чекпоинт поворота без смены длины
        double turn_checkpoint = length;
        // Чекпоинт поворота со сменой длины
        double length_checkpoint = 2.0 * length;
        // Мультипликаторы, определяющие
        // по какой стороне и в каком направлении пойдем
        double x_mult = 0.0;
        double y_mult = 1.0;

        // Цикл заполнения сетки
        for(int i = 1; i != num_of_points; ++i) {
            x += spacing * x_mult;
            y += spacing * y_mult;
            points->InsertPoint(i, x, y, z);

            // Поворот без смены длины
            if(i == turn_checkpoint) {
                if(x_mult == 0.0) {
                    x_mult = y_mult;
                    y_mult = 0.0;
                } else {
                    y_mult = -x_mult;
                    x_mult = 0.0;
                }
                continue;
            }

            // Поворот со сменой длины
            if(i == length_checkpoint) {
                ++length;
                turn_checkpoint = length_checkpoint + length;
                length_checkpoint += 2.0 * length;
                if(x_mult == 0.0) {
                    x_mult = y_mult;
                    y_mult = 0.0;
                } else {
                    y_mult = -x_mult;
                    x_mult = 0.0;
                }
            }
        }
        return points;
    }


    void normalAtPointOnModel(vtkPolyData* model, vtkIdType point_id, double* normal) {
        // Ищем, каким полигонам принадлежит эта точка
        std::vector<int> polys;
        for(int i = 0; i != model->GetNumberOfCells(); ++i) {
            vtkNew<vtkIdList> id_list;
            model->GetCellPoints(i, id_list);
            for(int j = 0; j != id_list->GetNumberOfIds(); ++j) {
                if(id_list->GetId(j) == point_id) polys.emplace_back(i);
            }
        }

        // Ищем среднюю нормаль для этих полигонов
        double norm[3] = {0};
        for(int i = 0; i != polys.size(); ++i) {
            // Поиск точек внутри полигона
            vtkNew<vtkIdList> id_list;
            model->GetCellPoints(polys[i], id_list);

            double p1[3];
            double p2[3];
            double p3[3];
            model->GetPoint(id_list->GetId(0), p1);
            model->GetPoint(id_list->GetId(1), p2);
            model->GetPoint(id_list->GetId(2), p3);

            // Нормаль для полигона
            double n[3];
            vtkTriangle::ComputeNormal(p1, p2, p3, n);

            for(int i = 0; i != 3; ++i)
                norm[i] += n[i];
        }

        // Проверка на противонаправленные нормали
        double norm_length = vtkMath::Norm(norm);
        if(polys.size() - norm_length > 0.5)
            std::cout << "Pizda normalyam setki" << std::endl;

        // Делаем нормаль снова равной единице
        for(int i = 0 ; i != 3; ++i)
            normal[i] = norm[i] / norm_length;
    }


    void parametersFromModel(vtkKdTreePointLocator* kdTree_locator,
                             vtkPolyData* model,
                             double* given_center, 
                             double* real_center, 
                             double* rotate_axis, 
                             double& angle) {
        // Находим соответствующую точку на модели
        vtkIdType id = kdTree_locator->FindClosestPoint(given_center);
        kdTree_locator->GetDataSet()->GetPoint(id, real_center);
        //std::cout << "Real point: " << real_center[0] << " " << real_center[1] << " " << real_center[2] << std::endl;

        // Поиск нормали к плоскости модели в данной точке
        double model_normal[3] = {0};
        normalAtPointOnModel(model, id, model_normal);

        // Поиск угла между нормалью на поверхности модели и нормали сетки
        double grid_normal[3] = {0,0,1};
        angle = vtkMath::AngleBetweenVectors(grid_normal, model_normal);
        //std::cout << angle * 57.3 << std::endl;

        // Поиск оси, вокург которой нужно повернуть сетку на найденный угол
        vtkMath::Cross(grid_normal, model_normal, rotate_axis);
    }


    vtkSmartPointer<vtkPoints> transformGrid(vtkPoints* points, double* rotate_axis, double angle, double* translate) {
        // Поворот
        vtkNew<vtkTransform> rotation;
        rotation->RotateWXYZ(angle * 57.296, rotate_axis);
        rotation->Update();

        vtkNew<vtkPoints> rotated_points;
        rotation->TransformPoints(points, rotated_points);

        // Перенос
        vtkNew<vtkTransform> translation;
        translation->Translate(translate);
        translation->Update();

        vtkNew<vtkPoints> translated_points;
        translation->TransformPoints(rotated_points, translated_points);

        return translated_points;
    }


    vtkSmartPointer<vtkPoints> projectGridOnModel(vtkPoints* src_points, vtkOBBTree* obb_tree, double* center) {
        vtkNew<vtkPoints> dst_points;
        for(int i = 0; i != src_points->GetNumberOfPoints(); ++i) {
            double point[3];
            src_points->GetPoint(i, point);
            // Вытаскиваем радиус вектор для этой точки и сразу удлиняем его
            for(int j = 0; j != 3; ++j) {
                point[j] -= center[j];
                point[j] *= 2.0;
                point[j] += center[j];
            }
            vtkNew<vtkPoints> intersection;
            if(obb_tree->IntersectWithLine(point, center, intersection, NULL))
                dst_points->InsertPoint(i, intersection->GetPoint(0));
            else std::cout << "No intersection, gg" << std::endl;
        }
        return dst_points;
    }


    vtkSmartPointer<vtkPolyData> generateGridPolyData(vtkPoints* grid_points, double spacing) {
        vtkNew<vtkAppendPolyData> append;

        // Создаем полидату точек
        for(int i = 0; i != grid_points->GetNumberOfPoints(); ++i) {
            // Достаем координаты точки
            double point[3];
            grid_points->GetPoint(i, point);

            // Создаем ее модель
            vtkNew<vtkSphereSource> sphere;
            sphere->SetCenter(point);
            sphere->SetRadius(spacing / 3);
            sphere->Update();

            // Засовываем в общую кучу
            append->AddInputData(sphere->GetOutput());
            append->Update();
        }

        // Соединяем линиями
        vtkNew<vtkCellArray> lines;
        lines->InsertNextCell(grid_points->GetNumberOfPoints());
        for(int i = 0; i != grid_points->GetNumberOfPoints(); ++i)
            lines->InsertCellPoint(i);

        vtkNew<vtkPolyData> poly_data;
        poly_data->SetPoints(grid_points);
        poly_data->SetLines(lines);

        // Также закидываем в общую кучу
        append->AddInputData(poly_data);
        append->Update();

        return append->GetOutput();
    }
}


vtkSmartPointer<vtkPoints> STRECH_GRID::stretchGridOnModel(int num_of_points,
                                                           double spacing,
                                                           double* grid_position,
                                                           vtkPolyData* model,
                                                           double* model_center,
                                                           vtkKdTreePointLocator* kd_tree,
                                                           vtkOBBTree* obb_tree,
                                                           vtkPolyData* grid_poly_data) {
    // Базовая сетка с заданными параметрами, расположенная в начале координат в плоскости XY
    vtkSmartPointer<vtkPoints> points = baseGrid(num_of_points, spacing);

    // Извлечение необходимых параметров из модели
    double rotate_axis[3] = {0.0};
    double angle = 0.0;
    parametersFromModel(kd_tree, model, grid_position, grid_position, rotate_axis, angle);

    // Крутим и пермещаем сетку в выбранную точку сетку
    points = transformGrid(points, rotate_axis, angle, grid_position);

    // Проецируем сетку на ближайшие 
    points = projectGridOnModel(points, obb_tree, model_center);

    // Создаем исходник для отображения сетки
    vtkSmartPointer<vtkPolyData> poly_data = generateGridPolyData(points, spacing);
    grid_poly_data->DeepCopy(poly_data);

    return points;
}
