#include <vtk_custom_utils/layout_10_20.hpp>

// General
#include <vtkNew.h>
#include <vtkNamedColors.h>
#include <vtkPLYReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

// Trees
#include <vtkIdList.h>
#include <vtkDijkstraGraphGeodesicPath.h>

// Cutter
#include <vtkTriangle.h>
#include <vtkSphereSource.h>
#include <vtkPlane.h>
#include <vtkClipPolyData.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkCutter.h>


namespace {
    /// @brief Принимает на вход координаты точек nasion, inion, tragus. 
    //  Совмещает их координаты с ближайшими точками на заданной модели
    void matchPoints(vtkKdTreePointLocator* kd_tree,
                     double* nasion,
                     double* inion,
                     double* tragus_l,
                     double* tragus_r) {
        // Поиск ближайших точек на поверхности к заданным
        vtkIdType iD1 = kd_tree->FindClosestPoint(nasion);
        vtkIdType iD2 = kd_tree->FindClosestPoint(inion);
        vtkIdType iD3 = kd_tree->FindClosestPoint(tragus_l);
        vtkIdType iD4 = kd_tree->FindClosestPoint(tragus_r);

        // Перезапись
        kd_tree->GetDataSet()->GetPoint(iD1, nasion);
        kd_tree->GetDataSet()->GetPoint(iD2, inion);
        kd_tree->GetDataSet()->GetPoint(iD3, tragus_l);
        kd_tree->GetDataSet()->GetPoint(iD4, tragus_r);
    }


    /// @brief Отделяет верхнюю часть модели. Необходимо выполнить, чтобы в дальнейшем
    /// алгоритм поиска кратчайших путей не искал путь через низ модели, тк зачастую 
    /// путь наиболее короткий именно там
    void separateUpperPart(vtkPolyData* model,
                           double* nasion,
                           double* inion,
                           double* tragus_l,
                           double* tragus_r,
                           vtkPolyData* upper_part) {
        // Вычисление нормалей для плоскостей
        // inion-nasion-tragus_l  -  левая плоскоть
        // nasion-inion-tragus_r  -  правая плоскость
        double n_l[3], n_r[3];
        vtkTriangle::ComputeNormal(inion, nasion, tragus_l, n_l);
        vtkTriangle::ComputeNormal(nasion, inion, tragus_r, n_r);

        // Вычисление средней нормали. Нормали секущей плоскости модели
        double normal[3] = {(n_l[0] + n_r[0]) / 2.0,
                            (n_l[1] + n_r[1]) / 2.0,
                            (n_l[2] + n_r[2]) / 2.0};

        // Создание функции секущей плоскости
        vtkNew<vtkPlane> cutting_plane;
        cutting_plane->SetNormal(normal);
        cutting_plane->SetOrigin(tragus_l);

        // Сечение модели плоскостью
        vtkNew<vtkClipPolyData> clipper;
        clipper->SetInputData(model);
        clipper->SetClipFunction(cutting_plane);
        clipper->SetValue(0);
        clipper->Update();

        // Копирование полученных данных
        upper_part->DeepCopy(clipper->GetOutput());
    }


    /// @brief Выполняет аппроксимацию поверхности головы сферой и отсекает ее нижнюю часть
    /// Необходимо выполнить, чтобы в дальнейшем алгоритм поиска кратчайших путей не искал
    /// путь через низ модели, тк зачастую путь наиболее короткий именно там
    /// @param nasion 
    /// @param inion 
    /// @param tragus_l 
    /// @param tragus_r 
    /// @param upper_part - Отсеченная часть сферы
    /// @param sphere_center - Центр аппроксимированной сферы. 
    /// Пригодится при переносе точек обратно на поверхность головы
    /// @param sphere_radius - Радиус аппроксимированной сферы.
    /// Пригодится при переносе точек обратно на поверхность головы
    void approxModelWithSphere(double* nasion,
                               double* inion,
                               double* tragus_l,
                               double* tragus_r,
                               vtkPolyData* upper_part,
                               double* sphere_center,
                               double sphere_radius) {
        // Вычисление нормалей для плоскостей
        // inion-nasion-tragus_l  -  левая плоскоть
        // nasion-inion-tragus_r  -  правая плоскость
        double n_l[3], n_r[3];
        vtkTriangle::ComputeNormal(inion, nasion, tragus_l, n_l);
        vtkTriangle::ComputeNormal(nasion, inion, tragus_r, n_r);

        // Вычисление средней нормали, Нормали секущей плоскости
        double normal[3] = {(n_l[0] + n_r[0]) / 2.0,
                            (n_l[1] + n_r[1]) / 2.0,
                            (n_l[2] + n_r[2]) / 2.0};

        // Создание функции секущей плоскости
        vtkNew<vtkPlane> cutting_plane;
        cutting_plane->SetNormal(normal);
        cutting_plane->SetOrigin(tragus_l);

        // Поиск центра аппроксимирующей сферы
        double center[3];
        center[0] = (nasion[0] + inion[0] + tragus_l[0] + tragus_r[0]) / 4.0;
        center[1] = (nasion[1] + inion[1] + tragus_l[1] + tragus_r[1]) / 4.0;
        center[2] = (nasion[2] + inion[2] + tragus_l[2] + tragus_r[2]) / 4.0;

        // Поиск радиуса аппроксимирующей сферы
        double radius;
        radius = sqrt(vtkMath::Distance2BetweenPoints(nasion, center)) +
                 sqrt(vtkMath::Distance2BetweenPoints(inion, center)) +
                 sqrt(vtkMath::Distance2BetweenPoints(tragus_l, center)) +
                 sqrt(vtkMath::Distance2BetweenPoints(tragus_r, center));
        radius /= 4.0;
        // Подгон центра))
        center[2] += radius * 0.5;

        // Создание сферы
        vtkNew<vtkSphereSource> sphere;
        sphere->SetCenter(center);
        sphere->SetRadius(radius);
        sphere->SetPhiResolution(100);
        sphere->SetThetaResolution(100);
        sphere->Update();

        // Подрезка
        vtkNew<vtkClipPolyData> clipper;
        clipper->SetInputData(sphere->GetOutput());
        clipper->SetClipFunction(cutting_plane);
        clipper->SetValue(0);
        clipper->Update();

        // Копирование полученных данных
        upper_part->DeepCopy(clipper->GetOutput());
        sphere_radius = radius;
        for(int i = 0; i != 3; ++i)
            sphere_center[i] = center[i];
    }


    /// @brief Выполняет аппроксимацию поверхности головы сферой и отсекает ее нижнюю часть
    /// Необходимо выполнить, чтобы в дальнейшем алгоритм поиска кратчайших путей не искал
    /// путь через низ модели, тк зачастую путь наиболее короткий именно там
    /// @param nasion 
    /// @param inion 
    /// @param tragus_l 
    /// @param tragus_r
    /// @param center - центр, рассчитанный ранее на основе базовых точек
    /// @param upper_part - Отсеченная часть сферы
    /// @param sphere_radius - Радиус аппроксимированной сферы.
    /// Пригодится при переносе точек обратно на поверхность головы
    void approxModelWithSphere(double* nasion,
                               double* inion,
                               double* tragus_l,
                               double* tragus_r,
                               double* center,
                               vtkPolyData* upper_part,
                               double sphere_radius) {
        // Вычисление нормалей для плоскостей
        // inion-nasion-tragus_l  -  левая плоскоть
        // nasion-inion-tragus_r  -  правая плоскость
        double n_l[3], n_r[3];
        vtkTriangle::ComputeNormal(inion, nasion, tragus_l, n_l);
        vtkTriangle::ComputeNormal(nasion, inion, tragus_r, n_r);

        // Вычисление средней нормали, Нормали секущей плоскости
        double normal[3] = {(n_l[0] + n_r[0]) / 2.0,
                            (n_l[1] + n_r[1]) / 2.0,
                            (n_l[2] + n_r[2]) / 2.0};

        // Создание функции секущей плоскости
        vtkNew<vtkPlane> cutting_plane;
        cutting_plane->SetNormal(normal);
        cutting_plane->SetOrigin(tragus_l);

        // Поиск радиуса аппроксимирующей сферы
        double radius;
        radius = sqrt(vtkMath::Distance2BetweenPoints(nasion, center)) +
                 sqrt(vtkMath::Distance2BetweenPoints(inion, center)) +
                 sqrt(vtkMath::Distance2BetweenPoints(tragus_l, center)) +
                 sqrt(vtkMath::Distance2BetweenPoints(tragus_r, center));
        radius /= 4.0;
        // Подгон центра))
        center[2] += radius * 0.5;

        // Создание сферы
        vtkNew<vtkSphereSource> sphere;
        sphere->SetCenter(center);
        sphere->SetRadius(radius);
        sphere->SetPhiResolution(100);
        sphere->SetThetaResolution(100);
        sphere->Update();

        // Подрезка
        vtkNew<vtkClipPolyData> clipper;
        clipper->SetInputData(sphere->GetOutput());
        clipper->SetClipFunction(cutting_plane);
        clipper->SetValue(0);
        clipper->Update();

        // Копирование полученных данных
        upper_part->DeepCopy(clipper->GetOutput());
        sphere_radius = radius;
    }


    /// @brief Вычисляет полигональную модель кратчайшего пути между точками по модели
    /// поверхности головы в заданной плоскости
    /// @param model - Модель, по которой ищется путь
    /// @param plane_normal - Нормаль к плоскости, пересечение модели с которой определяет
    /// форму конутра для поиска пути
    /// @param plane_origin - Базовая точка, определяющая положение плоскости
    /// @param start_point - Начальная точка пути
    /// @param end_point - Конечная точка пути
    /// @return модель кратчайшего пути между точками
    vtkSmartPointer<vtkPolyData> pathLine(vtkPolyData* model,
                                          double* plane_normal,
                                          double* plane_origin,
                                          double* start_point,
                                          double* end_point) {
        // Инициализация функции плоскости
        vtkNew<vtkPlane> plane;
        plane->SetOrigin(plane_origin);
        plane->SetNormal(plane_normal);

        // Поиск пересечения плоскости и скальпа
        vtkNew<vtkCutter> cutter;
        cutter->SetCutFunction(plane);
        cutter->SetInputData(model);
        cutter->Update();

        // Тк модель обрезанная, конутр может разрываться в районе ушей
        // Отсекаем эти обрезки
        vtkNew<vtkPolyDataConnectivityFilter> confilter;
        confilter->SetInputData(cutter->GetOutput());
        confilter->SetExtractionModeToLargestRegion();
        confilter->Update();

        // Фильтр соединенности удаляет только грани (а мб просто гасит),
        // нужно дополнительно удалить точки
        vtkNew<vtkCleanPolyData> cleaner;
        cleaner->SetInputData(confilter->GetOutput());
        cleaner->Update();

        // Поиск точек начала и конца пути на конутре
        vtkNew<vtkKdTreePointLocator> kdTree;
        kdTree->SetDataSet(cleaner->GetOutput());
        kdTree->Update();
        vtkIdType iD1 = kdTree->FindClosestPoint(start_point);
        vtkIdType iD2 = kdTree->FindClosestPoint(end_point);

        // Инициализация алгоритма Дейкстры для поиска кратчайшего пути 
        vtkNew<vtkDijkstraGraphGeodesicPath> dijkstra;
        dijkstra->SetInputData(cleaner->GetOutput());
        dijkstra->SetStartVertex(iD1);
        dijkstra->SetEndVertex(iD2);
        dijkstra->Update();

        return dijkstra->GetOutput();
    }


    /// @brief Вычисляет полигональную модель кратчайшего пути между точками сферы
    /// по упрощенному алгоритму в заданной плоскости
    /// @param model - Модель, по которой ищется путь
    /// @param plane_normal - Нормаль к плоскости, пересечение модели с которой определяет
    /// форму конутра для поиска пути
    /// @param plane_origin - Базовая точка, определяющая положение плоскости
    /// @param start_point - Начальная точка пути
    /// @param end_point - Конечная точка пути
    /// @return модель кратчайшего пути между точками
    vtkSmartPointer<vtkPolyData> pathLineSphere(vtkPolyData* model,
                                                double* plane_normal,
                                                double* plane_origin,
                                                double* start_point,
                                                double* end_point) {
        // Инициализация функции плоскости
        vtkNew<vtkPlane> plane;
        plane->SetOrigin(plane_origin);
        plane->SetNormal(plane_normal);

        // Поиск пересечения плоскости и скальпа
        vtkNew<vtkCutter> cutter;
        cutter->SetCutFunction(plane);
        cutter->SetInputData(model);
        cutter->Update();

        // Поиск точек начала и конца пути на конутре
        vtkNew<vtkKdTreePointLocator> kdTree;
        kdTree->SetDataSet(cutter->GetOutput());
        kdTree->Update();
        vtkIdType iD1 = kdTree->FindClosestPoint(start_point);
        vtkIdType iD2 = kdTree->FindClosestPoint(end_point);

        // Инициализация алгоритма Дейкстры для поиска кратчайшего пути 
        vtkNew<vtkDijkstraGraphGeodesicPath> dijkstra;
        dijkstra->SetInputData(cutter->GetOutput());
        dijkstra->SetStartVertex(iD1);
        dijkstra->SetEndVertex(iD2);
        dijkstra->Update();

        return dijkstra->GetOutput();
    }

    /// @brief Длина пути, представленного в виде полигональной модели
    /// @param path 
    /// @return 
    double pathLength(vtkPolyData* path) {
        double distance = 0.0;
        for(int i = 1; i != path->GetNumberOfPoints(); ++i) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i, point1);
            distance += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        return distance;
    }


    /// @brief Секущая плоскость, проходящая через nasion и inion. 
    /// Примерно ортогональна линии tragus_l-tragus_r
    void cuttingPlaneNasionInion(vtkPolyData* upper,
                                 double* nasion, 
                                 double* inion, 
                                 double* tragus_l, 
                                 double* tragus_r,
                                 double* Fpz, 
                                 double* Fz, 
                                 double* Cz, 
                                 double* Pz, 
                                 double* Oz) {
        // Вычисление нормалей для плоскостей
        // nasion-inion-tragus_l  -  левая плоскоть
        // nasion-inion-tragus_r  -  правая плоскость
        double n_l[3], n_r[3];
        vtkTriangle::ComputeNormal(nasion, inion, tragus_l, n_l);
        vtkTriangle::ComputeNormal(nasion, inion, tragus_r, n_r);

        // Вычисление нормали плоскости nasion-Cz-inion
        double normal[3] = {(n_l[0] + n_r[0]) / 2.0,
                            (n_l[1] + n_r[1]) / 2.0,
                            (n_l[2] + n_r[2]) / 2.0};

        // Поиск кратчайшего пути между точками
        vtkSmartPointer<vtkPolyData> path = pathLineSphere(upper, normal, nasion, nasion, inion);

        // Длина кратчайшего пути
        double distance = pathLength(path);

        // Отрезки интереса
        double dist_10 = distance * 0.1;
        double dist_30 = distance * 0.3;
        double dist_50 = distance * 0.5;
        double dist_70 = distance * 0.7;
        double dist_90 = distance * 0.9;

        // Вычисление положения точек на границах отрезков интереса
        double dist = 0;
        int i = 1;

        while(dist < dist_10) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, Oz);

        while(dist < dist_30) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, Pz);

        while(dist < dist_50) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, Cz);

        while(dist < dist_70) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, Fz);

        while(dist < dist_90) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, Fpz);
    }


    /// @brief Секущая плоскость tragus_l - Cz - tragus_r
    void cuttingPlaneTragusLRCz(vtkPolyData* upper,
                                double* tragus_l,
                                double* Cz,
                                double* tragus_r,
                                double* T3,
                                double* C3,
                                double* C4,
                                double* T4) {
        // Вычисление нормали к секущей плоскости
        double normal[3];
        vtkTriangle::ComputeNormal(tragus_l, Cz, tragus_r, normal);

        // Поиск кратчайшего пути между точками tragus
        vtkSmartPointer<vtkPolyData> path = pathLineSphere(upper, normal, tragus_l, tragus_l, tragus_r);

        // Длина кратчайшего пути
        double distance = pathLength(path);
        // Отрезки интереса
        double dist_10 = distance * 0.1;
        double dist_30 = distance * 0.3;
        double dist_70 = distance * 0.7;
        double dist_90 = distance * 0.9;

        // Вычисление положения точек на границах отрезков интереса
        double dist = 0;
        int i = 1;

        while(dist < dist_10) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, T4);

        while(dist < dist_30) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, C4);

        while(dist < dist_70) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, C3);

        while(dist < dist_90) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, T3);
    }


    /// @brief Находит на модели точки, которые находятся в плоскостях T3-Fpz-T4 и T3-Oz-T4
    /// Передаваемые и получаемые данные в случае плоскости T3-Oz-T4 указаны в скобках
    /// @param upper - Модель, на которой ищутся точки
    /// @param T3 (T3)
    /// @param Fpz (Oz)
    /// @param T4 (T4)
    /// @param F7 (T5)
    /// @param Fp1 (O1)
    /// @param Fp2 (O2)
    /// @param F8 (T6)
    void cuttingPlaneT3FpzT4_T3OzT4(vtkPolyData* upper,
                                    double* T3, 
                                    double* Fpz, 
                                    double* T4,
                                    double* F7,
                                    double* Fp1,
                                    double* Fp2,
                                    double* F8) {
        // Вычисление нормали к секущей плоскости
        double normal[3];
        vtkTriangle::ComputeNormal(T4, Fpz, T3, normal);

        // Поиск кратчайшего пути между точками Fpz, T4 и Fpz, T3
        vtkSmartPointer<vtkPolyData> path_r = pathLineSphere(upper, normal, Fpz, T4, Fpz);
        vtkSmartPointer<vtkPolyData> path_l = pathLineSphere(upper, normal, Fpz, T3, Fpz);

        // Длина кратчайшего пути
        double distance_r = pathLength(path_r);
        double distance_l = pathLength(path_l);

        // Отрезки интереса
        double dist_20_r = distance_r * 0.2;
        double dist_60_r = distance_r * 0.6;
        double dist_20_l = distance_l * 0.2;
        double dist_60_l = distance_l * 0.6;

        // Вычисление положения точек на границах отрезков интереса
        double dist = 0;
        int i = 1;

        while(dist < dist_20_r) {
            double point0[3], point1[3];
            path_r->GetPoint(i - 1, point0);
            path_r->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path_r->GetPoint(i - 1, Fp2);

        while(dist < dist_60_r) {
            double point0[3], point1[3];
            path_r->GetPoint(i - 1, point0);
            path_r->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path_r->GetPoint(i - 1, F8);

        // Переход ко второму отрезку
        dist = 0; i = 1;

        while(dist < dist_20_l) {
            double point0[3], point1[3];
            path_l->GetPoint(i - 1, point0);
            path_l->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path_l->GetPoint(i - 1, Fp1);

        while(dist < dist_60_l) {
            double point0[3], point1[3];
            path_l->GetPoint(i - 1, point0);
            path_l->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path_l->GetPoint(i - 1, F7);
    }


    /// @brief Находит на модели точки, которые находятся в плоскостях F7-Fz-F8 и T5-Pz-T6
    /// Передаваемые и получаемые данные в случае плоскости T5-Pz-T6 указаны в скобках
    /// @param upper 
    /// @param F7 (T5)
    /// @param Fz (Pz)
    /// @param F8 (T6)
    /// @param F3 (P3)
    /// @param F4 (P4)
    void cuttingPlaneF7FzF8_T5PzT6(vtkPolyData* upper,
                                   double* F7,
                                   double* Fz,
                                   double* F8,
                                   double* F3,
                                   double* F4) {
        // Вычисление нормали к секущей плоскости
        double normal[3];
        vtkTriangle::ComputeNormal(F7, Fz, F8, normal);

        // Поиск кратчайшего пути между точками F7, F8
        vtkSmartPointer<vtkPolyData> path = pathLineSphere(upper, normal, Fz, F7, F8);

        // Длина кратчайшего пути
        double distance = pathLength(path);

        // Отрезки интереса
        double dist_25 = distance * 0.25;
        double dist_75 = distance * 0.75;

        // Вычисление положения точек на границах отрезков интереса
        double dist = 0;
        int i = 1;

        while(dist < dist_25) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, F4);

        while(dist < dist_75) {
            double point0[3], point1[3];
            path->GetPoint(i - 1, point0);
            path->GetPoint(i++, point1);
            dist += sqrt(vtkMath::Distance2BetweenPoints(point0, point1));
        }
        path->GetPoint(i - 1, F3);
    }


    vtkSmartPointer<vtkPoints> packPoints(double* Fpz, 
                                          double* Fz, 
                                          double* Cz, 
                                          double* Pz, 
                                          double* Oz, 
                                          double* T3, 
                                          double* C3, 
                                          double* C4, 
                                          double* T4, 
                                          double* F7, 
                                          double* Fp1, 
                                          double* Fp2, 
                                          double* F8, 
                                          double* F3, 
                                          double* F4, 
                                          double* T5, 
                                          double* O1, 
                                          double* O2, 
                                          double* T6, 
                                          double* P3, 
                                          double* P4) {
        vtkNew<vtkPoints> points;
        points->SetNumberOfPoints(21);
        points->SetPoint(0, Fpz);
        points->SetPoint(1, Fz);
        points->SetPoint(2, Cz);
        points->SetPoint(3, Pz);
        points->SetPoint(4, Oz);
        points->SetPoint(5, T3);
        points->SetPoint(6, C3);
        points->SetPoint(7, C4);
        points->SetPoint(8, T4);
        points->SetPoint(9, F7);
        points->SetPoint(10, Fp1);
        points->SetPoint(11, Fp2);
        points->SetPoint(12, F8);
        points->SetPoint(13, F3);
        points->SetPoint(14, F4);
        points->SetPoint(15, T5);
        points->SetPoint(16, O1);
        points->SetPoint(17, O2);
        points->SetPoint(18, T6);
        points->SetPoint(19, P3);
        points->SetPoint(20, P4);
        return points;
    }


    /// @brief Переносит точки, найденные на сфере, аппроксимирующей модель головы, на модель головы
    /// @param model - Модель головы
    /// @param sphere_center - Центр сферы
    /// @param sphere_radius - Рaдиус сферы
    /// @param points - Точки, которые переносим
    void transferPointsFromSphereToModel(vtkOBBTree* obb_tree,
                                         vtkKdTreePointLocator* kd_tree, 
                                         double* sphere_center,
                                         double sphere_radius,
                                         vtkPoints* points) {
        // Проход по каждой точке
        for(int i = 0; i != points->GetNumberOfPoints(); ++i) {
            // Вытаскиваем положение на сфере
            double point[3];
            points->GetPoint(i, point);

            // Вытаскиваем радиус вектор для этой точки и сразу удлиняем его
            for(int i = 0; i != 3; ++i) {
                point[i] -= sphere_center[i];
                point[i] *= 2.0;
                point[i] += sphere_center[i];
            }

            // Поиск пересечения
            vtkNew<vtkPoints> intersection;
            if(obb_tree->IntersectWithLine(sphere_center, point, intersection, NULL)) {
                // Вытаскиваем точку из пересечения
                intersection->GetPoint(0, point);
            } else {
                // По kd дереву ищем ближайшую точку, если не найдено пересечений
                std::cout << "Hole in model, using nearest point" << std::endl;
                vtkIdType iD1 = kd_tree->FindClosestPoint(points->GetPoint(i));
                kd_tree->GetDataSet()->GetPoint(iD1, point);
            }

            // Обновляем точку
            points->SetPoint(i, point);
        }
    }
} //namespace


vtkSmartPointer<vtkPoints> LAYOUT_10_20::mark(vtkPolyData* model,
                                              vtkKdTreePointLocator* kd_tree,
                                              vtkOBBTree* obb_tree,
                                              double* nasion,
                                              double* inion,
                                              double* tragus_l,
                                              double* tragus_r,
                                              double* center) {
    // Связываем заданные точки и точки на поверхности модели
    matchPoints(kd_tree, nasion, inion, tragus_l, tragus_r);

    // Аппроксимируем верхнюю часть головы сферой (обрезанной)
    vtkNew<vtkPolyData> upper;
    double sphere_radius;
    approxModelWithSphere(nasion, inion, tragus_l, tragus_r, center, upper, sphere_radius);

    // Точки 10-20
    double         Fp1[3], Fpz[3], Fp2[3],
           F7[3],  F3[3],  Fz[3],  F4[3],  F8[3],
           T3[3],  C3[3],  Cz[3],  C4[3],  T4[3],
           T5[3],  P3[3],  Pz[3],  P4[3],  T6[3],
                   O1[3],  Oz[3],  O2[3];
    
    // Просчитываем точки на сфере
    cuttingPlaneNasionInion(upper, nasion, inion, tragus_l, tragus_r,
                            Fpz, Fz, Cz, Pz, Oz);
    cuttingPlaneTragusLRCz(upper, tragus_l, Cz, tragus_r,
                         T3, C3, C4, T4);
    cuttingPlaneT3FpzT4_T3OzT4(upper, T3, Fpz, T4,
                               F7, Fp1, Fp2, F8);
    cuttingPlaneF7FzF8_T5PzT6(upper, F7, Fz, F8, 
                              F3, F4);
    cuttingPlaneT3FpzT4_T3OzT4(upper, T3, Oz, T4,
                               T5, O1, O2, T6);
    cuttingPlaneF7FzF8_T5PzT6(upper, T5, Pz, T6,
                              P3, P4);

    // Запаковываем все точки в один объект
    vtkSmartPointer<vtkPoints> pts = packPoints(Fpz, Fz, Cz, Pz, Oz, T3, C3,
                                                C4, T4, F7, Fp1, Fp2, F8, F3,
                                                F4, T5, O1, O2, T6, P3, P4);

    // Переносим точки на соответствующие места на модели
    transferPointsFromSphereToModel(obb_tree, kd_tree, center, sphere_radius, pts);
    return pts;
}

/// @brief Поиск центра масс, основываясь на заданных точках
/// @param nasion 
/// @param inion 
/// @param tragus_l 
/// @param tragus_r 
/// @param center 
void LAYOUT_10_20::centerOfMass(double* nasion,
                                double* inion,
                                double* tragus_l,
                                double* tragus_r,
                                double* center) {
    center[0] = (nasion[0] + inion[0] + tragus_l[0] + tragus_r[0]) / 4.0;
    center[1] = (nasion[1] + inion[1] + tragus_l[1] + tragus_r[1]) / 4.0;
    center[2] = (nasion[2] + inion[2] + tragus_l[2] + tragus_r[2]) / 4.0;
}

/// @brief Создает актера точки
/// @param coordinates 
/// @return 
vtkSmartPointer<vtkActor> LAYOUT_10_20::pointActor(double* coordinates) {
    vtkNew<vtkSphereSource> sphere_source;
    sphere_source->SetCenter(coordinates);
    sphere_source->SetRadius(4);
    sphere_source->Update();
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(sphere_source->GetOutput());
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0,1,0);
    return actor;
}

/// @brief Создает актера точки
/// @param coordinates
/// @return 
vtkSmartPointer<vtkActor> LAYOUT_10_20::pointActor(double* coordinates, 
                                                   double r,
                                                   double g,
                                                   double b) {
    vtkNew<vtkSphereSource> sphere_source;
    sphere_source->SetCenter(coordinates);
    sphere_source->SetRadius(4);
    sphere_source->Update();
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(sphere_source->GetOutput());
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(r,g,b);
    return actor;
}
