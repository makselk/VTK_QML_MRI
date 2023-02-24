#include "head_cloud.hpp"


void HEAD_POINT_CLOUD::head_cloud_output(const std::string &directory_src,
                       const std::string &directory_dst) {
    HeadCloud data(directory_src);
    data.sort();
    data.equalizeImages();
    std::vector<cv::Point3f> cloud = data.headSurfaceCloud();
    data.saveCloudPLY(cloud, directory_dst);
}

std::vector<cv::Point3f> HEAD_POINT_CLOUD::head_cloud(const std::string &directory_src) {
    HeadCloud data(directory_src);
    data.sort();
    data.equalizeImages();
    std::vector<cv::Point3f> cloud = data.headSurfaceCloud();
    return cloud;
}

HeadCloud::HeadCloud(const std::string &directory) {
    std::vector<std::string> paths = getPaths(directory);
    std::vector<DICOM> dcm_files;
    for(auto & dcm_path: paths)
        dcm_files.emplace_back(DICOM(dcm_path));
    std::cout << dcm_files.size() << " files was found"  << std::endl;

    spaces = dcm_files[0].extractSpaces();
    orientation = dcm_files[0].extractOrientation();
    research_type = dcm_files[0].extractResearchType();

    for(auto & dcm: dcm_files) {
        images.emplace_back(dcm.extractImage());
        positions.emplace_back(dcm.extractPosition());
    }
}

std::vector<std::string> HeadCloud::getPaths(const std::string &directory) {
    std::vector<std::string> paths;
    for(const auto& entry: std::filesystem::recursive_directory_iterator(directory)) {
        if(!entry.is_directory())
            paths.emplace_back(entry.path().string());
    }
    return std::move(paths);
}

void HeadCloud::sort() {
    /// Поиск минимального и максимального значения положения изображений исследования
    float xmin = positions[0].x;
    float ymin = positions[0].y;
    float zmin = positions[0].z;
    float xmax = xmin;
    float ymax = ymin;
    float zmax = zmin;

    for(auto & position: positions) {
        if(position.x < xmin)
            xmin = position.x;
        else if(position.x > xmax)
            xmax = position.x;
            
        if(position.y < ymin)
            ymin = position.y;
        else if(position.y > ymax)
            ymax = position.y;

        if(position.z < zmin)
            zmin = position.z;
        else if(position.z > zmax)
            zmax = position.z;
    }

    /// Поиск наибольшего смещения за исследование
    float dx = xmax - xmin;
    float dy = ymax - ymin;
    float dz = zmax - zmin;

    /// На основе наибольшего смещения определяется тип среза, 
    /// а также создается мапа для сортировки
    std::vector<float> order;
    std::map<float, int> orderMap;
    //Sagittal
    if(dx > dy && dx > dz) {
        int i = 0;
        for(auto & position : positions) {
            order.emplace_back(position.x);
            orderMap[position.x] = i++;
        }
        std::sort(order.begin(), order.end(), std::greater<float>());
    }
    //Axial
    else if(dy > dx && dy > dz) {
        int i = 0;
        for(auto & position: positions) {
            order.emplace_back(position.y);
            orderMap[position.y] = i++;
        }
        std::sort(order.begin(), order.end(), std::less<float>());
    }
    //Coronal
    else if(dz > dy && dz > dx) {
        int i = 0;
        for(auto & position: positions) {
            order.emplace_back(position.z);
            orderMap[position.z] = i++;
        }
        std::sort(order.begin(), order.end(), std::less<float>());
    }

    /// Сортировка
    std::vector<cv::Mat> new_images;
    std::vector<cv::Point3f> new_positions;
    for(auto &i: order) {
        new_images.emplace_back(images[orderMap[i]]);
        new_positions.emplace_back(positions[orderMap[i]]);
    }
    this->images = new_images;
    this->positions = new_positions;
}

void HeadCloud::equalizeImages() {
    int16_t maxVal = 0;
    // находим максимальную интенсивность среди всех изображений
    for(auto &image: images)
        for(int i = 0; i != image.rows; i++)
            for(int j = 0; j != image.cols; j++)
                if(image.at<int16_t>(i, j) > maxVal)
                    maxVal = image.at<int16_t>(i, j);

    /// Находим делитель для перевода в 8 бит
    auto divider = (int16_t)(maxVal / 255);

    /// Применяем его
    for(auto &image: images) {
        cv::Mat newMat(image.rows, image.cols, CV_8UC1);
        for(int i = 0; i != image.rows; i++)
            for(int j = 0; j != image.cols; j++)
                newMat.at<uint8_t>(i, j) = static_cast<uint8_t>(image.at<uint16_t>(i, j) / divider);
        image = newMat;
    }
}

std::vector<cv::Point3f> HeadCloud::headSurfaceCloud() {
    uint8_t threshold = defineThreshold();
    std::vector<cv::Mat> contours = headSurfaceContours(threshold);

    // визуализация контуров
    /*for(int i = 0; i != contours.size(); ++i) {
        cv::Mat vis = images[i].clone();
        cv::bitwise_or(vis, contours[i], vis);
        cv::imshow("Contours", vis);
        cv::waitKey(0);
    }*/

    // инициализация матриц для вычисления координат точек
    cv::Mat A(4, 4, CV_32FC1);
    cv::Mat B(4, 1, CV_32FC1);
    A.at<float>(0, 2) = 0;
    A.at<float>(1, 2) = 0;
    A.at<float>(2, 2) = 0;
    A.at<float>(3, 2) = 0;
    A.at<float>(3, 0) = 0;
    A.at<float>(3, 1) = 0;
    A.at<float>(3, 3) = 1;
    B.at<float>(0, 2) = 0;
    B.at<float>(0, 3) = 1;
    A.at<float>(0, 0) = orientation[0] * spaces.first;
    A.at<float>(1, 0) = orientation[1] * spaces.first;
    A.at<float>(2, 0) = orientation[2] * spaces.first;
    A.at<float>(0, 1) = orientation[3] * spaces.second;
    A.at<float>(1, 1) = orientation[4] * spaces.second;
    A.at<float>(2, 1) = orientation[5] * spaces.second;

    std::vector<cv::Point3f> surface_cloud;
    for(size_t i = 0; i != contours.size(); ++i) {
        A.at<float>(0, 3) = positions[i].x; 
        A.at<float>(1, 3) = positions[i].y;
        A.at<float>(2, 3) = positions[i].z;
        for(int row = 0; row != contours[i].rows; ++row) {
            for(int col = 0; col != contours[i].cols; ++col) {
                // Если в данном пикселе контура нет - заносить в облако точек не надо
                if(!contours[i].at<uint8_t>(row, col))
                    continue;
                
                /// Конечная инициализация матрицы
                B.at<float>(0, 0) = (float)col;
                B.at<float>(0, 1) = (float)row;
                cv::Mat res = A * B;

                // Создание новой точки и запись ее в облако
                cv::Point3f tmp(res.at<float>(0, 0), res.at<float>(0, 1), res.at<float>(0, 2));
                surface_cloud.emplace_back(tmp);
            }
        }
    }
    return surface_cloud;
}

uint8_t HeadCloud::defineThreshold() {
    if(research_type == "CT") {
        return 40;
    } else {
        std::vector<int> histogram = buildHistogram();
        return histogramThreshold(histogram);
    }
}

std::vector<int> HeadCloud::buildHistogram() {
    // Гистограмма для определения пороговых значений
    std::vector<int> histogram(256, 0);

    // Подсчет количества пикселей каждой интенсивности
    for(auto &image: images) {
        for(auto pixel = image.datastart ; pixel != image.dataend; pixel++)
            histogram[*pixel]++;
    }
    return histogram;
}

uint8_t HeadCloud::histogramThreshold(std::vector<int> &histogram) {
    /// Общее количество пикселей на всех изображениях
    int numberOfPixels = 0;
    for(auto &pixels : histogram)
        numberOfPixels += pixels;

    /// Игнорируемый хвост - количество пикселей с каждой стороны диаграммы,
    /// которое не учитывается при расчете (2%)
    int ignoreTail = (int)(0.02 * (double)numberOfPixels);

    /// Нижняя граница интенсивности
    int tmpSum = 0;
    uint8_t t2 = 0;
    while(tmpSum < ignoreTail)
        tmpSum += histogram[t2++];

    /// Верхняя граница интенсивности
    uint8_t t98 = 255;
    tmpSum = 0;
    while(tmpSum < ignoreTail)
        tmpSum += histogram[t98--];

    /// Граница фона изображения
    uint8_t t = (t98 - t2) * (10.0 / 100.0);
    return t;
}

std::vector<cv::Mat> HeadCloud::headSurfaceContours(uint8_t threshold) {
    int g_kernel, g_sigma;
    gaussianKernel(g_kernel, g_sigma);

    std::vector<cv::Mat> masks;
    if(research_type == "CT") {
        for(auto &image: images)
            masks.emplace_back(maskCT(image, threshold, g_kernel, g_sigma));
    } else {
        for(auto &image: images)
            masks.emplace_back(maskMRI(image, threshold, g_kernel, g_sigma));
    }

    std::vector<cv::Mat> head_surface_contours;
    for(auto &mask: masks) {
        // поиск списка контуров на маске
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(mask, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);
        
        // прорисовка всех контуров
        cv::Mat head_contour = cv::Mat::zeros(mask.rows, mask.cols, CV_8UC1);
        for(int i = 0; i != contours.size(); i++)
            cv::drawContours(head_contour, contours, i, 255, 1, cv::LINE_8, hierarchy);
        head_surface_contours.emplace_back(head_contour);
    }

    // Небольшая децимация точек в контуре
    cv::Mat lattice = createLattice(masks[0].rows, masks[0].cols);
    for(auto &contour: head_surface_contours)
        cv::bitwise_and(contour, lattice, contour);
    
    return head_surface_contours;
}

void HeadCloud::gaussianKernel(int &g_kernel, int &g_sigma) {
    float scale_X = images[0].cols / 256.0f;
    float scale_Y = images[0].rows / 256.0f;
    float scale_f = (scale_X + scale_Y) / 2.0f;
    float kernel = 7.0f * scale_f;
    float sigma = 5.0f * scale_f;
    g_kernel = std::lround(kernel);
    g_sigma = std::lround(sigma);
    if(g_kernel % 2 == 0)
        g_kernel -= 1;
    if(g_sigma % 2 == 0)
        g_sigma -= 1;
}

cv::Mat HeadCloud::maskCT(cv::Mat &image, uint8_t threshold, int g_kernel, int g_sigma) {
    // Делаем копию, чтоб не повредить исходник
    cv::Mat tmp = image.clone();
    // Убираем шумы и дефекты изображения
    cv::GaussianBlur(tmp, tmp, cv::Size(7, 7), 5);
    // Убираем все, что ниже проницаемости скальпа (кожи головы)
    cv::threshold(tmp, tmp, threshold, 255, cv::THRESH_BINARY);
    // Заливка всего, что вокруг головы
    cv::Mat floodFill = tmp.clone();
    cv::floodFill(floodFill, cv::Point(0, 0), cv::Scalar(255));
    // Инвариация. Выделение незаполненых полостей внутри головы
    cv::Mat tmp_inv;
    bitwise_not(floodFill, tmp_inv);
    // Заливка незаполненных полостей
    cv::Mat out = (tmp | tmp_inv);
    return out;
}

cv::Mat HeadCloud::maskMRI(cv::Mat &image, uint8_t threshold, int g_kernel, int g_sigma) {
    // Делаем копию, чтоб не повредить исходник
    cv::Mat tmp = image.clone();
    // Предварительная чистка шумов
    cv::threshold(tmp, tmp, threshold, 255, cv::THRESH_TOZERO);
    // Убираем оставшиеся шумы и дефекты изображения
    cv::GaussianBlur(tmp, tmp, cv::Size(g_kernel, g_kernel), g_sigma);
    // Убираем все, что ниже магнитной проницаемости скальпа (кожи головы)
    cv::threshold(tmp, tmp, threshold, 255, cv::THRESH_BINARY);
    // Морфологическая операция для замыкания маски для правильной заливки
    int kernel_size = image.cols / 40;
    cv::dilate(tmp, tmp, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kernel_size, kernel_size)));
    cv::erode(tmp, tmp, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kernel_size, kernel_size)));
    // Заливка всего, что вокруг головы
    cv::Mat floodFill = tmp.clone();
    cv::floodFill(floodFill, cv::Point(0, 0), cv::Scalar(255));
    // Инвариация. Выделение незаполненых полостей внутри головы
    cv::Mat tmp_inv;
    bitwise_not(floodFill, tmp_inv);
    // Заливка незаполненных полостей
    cv::Mat out = (tmp | tmp_inv);
    // Уменьшение границ, размазанных гауссовым фильтром
    cv::erode(out, out, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(g_sigma, g_sigma)));
    return out;
}

cv::Mat HeadCloud::createLattice(int rows, int cols) {
    cv::Mat lattice = cv::Mat::zeros(rows, cols, CV_8UC1);

    for(int x = 0; x != lattice.cols; ++x)
        for(int y = 1; y < lattice.rows; y += 2) 
            lattice.at<uint8_t>(y,x) = 255;

    for(int x = 1; x < lattice.cols; x += 2)
        for(int y = 0; y != lattice.rows; ++y)
            lattice.at<uint8_t>(y,x) = 255;
        
    return lattice;
}

void HeadCloud::saveCloudPLY(std::vector<cv::Point3f> &cloud,
                             const std::string &directory) {
    std::cout << "Saving to PLY" << std::endl;
    std::string filename = "cloud";
    std::ofstream output_file(directory + "/" + filename + ".ply");

    output_file << "ply\n";
    output_file << "format ascii 1.0\n";
    output_file << "element vertex " << cloud.size() << "\n";
    output_file << "property float32 x\n";
    output_file << "property float32 y\n";
    output_file << "property float32 z\n";
    output_file << "element face " << 0 << "\n";
    output_file << "property list uint8 int32 vertex_indices\n";
    output_file << "end_header\n";

    for(auto &point: cloud)
        output_file << point.x << " " << point.y << " " << point.z << "\n";
}
