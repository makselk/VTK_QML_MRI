#include "utility_dcm.hpp"


DICOM::DICOM(const std::string& path) {
    dataset = extractDataset(path);
}

DcmDataset DICOM::extractDataset(const std::string& path) {
    DcmFileFormat file;

    ///Подключение кодеков для декодирования
    DJDecoderRegistration::registerCodecs();

    ///Проверка корректности пути к файлу
    OFCondition status = file.loadFile(path.c_str());
    if (status.bad()) {
        std::cerr << "Error: cannot read DICOM file (" << status.text() << ")" << std::endl;
        return dataset;
    }

    ///Извлечение датасета и приведение его к необходимому типу
    dataset = *file.getDataset();
    dataset.chooseRepresentation(EXS_LittleEndianExplicit, nullptr);

    ///Кодеки больше не нужны (вроде)
    DJDecoderRegistration::cleanup();
    return dataset;
}

cv::Mat DICOM::extractImage() {
    ///Инициализация пустой матрицы
    cv::Mat OpenCVMatrix(0,0, CV_8UC1);

    ///Поиск размеров матрицы
    uint16_t rows, cols;
    dataset.findAndGetUint16(DCM_Rows, rows);
    dataset.findAndGetUint16(DCM_Columns, cols);

    ///Матрица значений пикселей из датасета
    const uint16_t *pixelDataset = new uint16_t[rows * cols];
    if(dataset.findAndGetUint16Array(DCM_PixelData, pixelDataset).bad()) {
        std::cerr << "Error: cannot access Patient's Info!" << std::endl;
        return OpenCVMatrix;
    }

    if(pixelDataset == nullptr) {
        std::cerr << "Error: empty pixelData" << std::endl;
        return OpenCVMatrix;
    }

    ///Добавление данных изображения в начальную матрицу
    OpenCVMatrix = cv::Mat(rows, cols, CV_16UC1);
    for(int i = 0; i != rows; i++)
        for(int j = 0; j != cols; j++)
            OpenCVMatrix.at<uint16_t>(i, j) = pixelDataset[i * cols + j];

    //delete[] pixelDataset;
    return OpenCVMatrix;
}

cv::Mat DICOM::extractImage(std::pair<float, float> compressScale) {
    ///Инициализация пустой матрицы
    cv::Mat OpenCVMatrix(0,0, CV_8UC1);

    ///Поиск размеров матрицы
    uint16_t rows, cols;
    dataset.findAndGetUint16(DCM_Rows, rows);
    dataset.findAndGetUint16(DCM_Columns, cols);

    ///Матрица значений пикселей из датасета
    const uint16_t *pixelDataset = new uint16_t[rows * cols];
    if(dataset.findAndGetUint16Array(DCM_PixelData, pixelDataset).bad()) {
        std::cerr << "Error: cannot access Patient's Info!" << std::endl;
        return OpenCVMatrix;
    }

    if(pixelDataset == nullptr) {
        std::cerr << "Error: empty pixelData" << std::endl;
        return OpenCVMatrix;
    }

    ///Добавление данных изображения в начальную матрицу
    cv::Mat tmpMat = cv::Mat(rows, cols, CV_16UC1);
    for(int i = 0; i != rows; i++)
        for(int j = 0; j != cols; j++)
            tmpMat.at<uint16_t>(i, j) = pixelDataset[i * cols + j];

    /// Если сжатие не требуется
    if(compressScale.first == 0.0f && compressScale.second == 0.0f)
        return tmpMat;
    
    /// Иначе сжимаем
    cv::resize(tmpMat, OpenCVMatrix, cv::Size((uint16_t)((float)rows / compressScale.first),
                                              (uint16_t)((float)cols / compressScale.second)));

    //delete[] pixelDataset;
    return OpenCVMatrix;
}

cv::Mat DICOM::extractImageCLAHE(){
    ///Инициализация пустой матрицы
    cv::Mat openCVMatrix(0, 0, CV_8UC1);

    ///Поиск размеров матрицы
    uint16_t rows, cols;
    dataset.findAndGetUint16(DCM_Rows, rows);
    dataset.findAndGetUint16(DCM_Columns, cols);

    ///Создание матрицы значений пикселей из датасета
    const uint16_t *pixelDataset = new uint16_t[rows * cols];
    if(dataset.findAndGetUint16Array(DCM_PixelData, pixelDataset).bad()) {
        std::cerr << "Error: cannot access Patient's Info!" << std::endl;
        return openCVMatrix;
    }

    if (pixelDataset == nullptr) {
        std::cerr << "Error: empty pixelData" << std::endl;
        return openCVMatrix;
    }

    ///Добавление данных из датасета в "сырую" матрицу
    cv::Mat raw(rows, cols, CV_16UC1);
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++)
            raw.at<uint16_t>(i, j) = pixelDataset[i * rows + j];
    }

    ///Адаптивная коррекия гистограммы с ограниченной контрастностью
    ///Contrast limited adaptive histogram equalization (CLAHE)
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(4.0, cv::Size(8,8));
    clahe->apply(raw, raw);

    ///Перенос данных из сырого 16-битного изображения в исходное 8-битное
    openCVMatrix = cv::Mat(rows, cols, CV_8UC1);
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++)
            openCVMatrix.at<uint8_t>(i, j) = static_cast<uint8_t> (raw.at<uint16_t>(i, j) / 256);
    }

    return openCVMatrix;
}

int16_t DICOM::extractLargestValue() {
    int16_t largestValue = 0;
    dataset.findAndGetSint16(DCM_LargestImagePixelValue, largestValue);
    return largestValue;
}

std::pair<float, float> DICOM::extractSpaces() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFStringArray(DCM_PixelSpacing, data);
    if(condition.bad())
        throw std::runtime_error(condition.text());
    auto parts = separateData(data);
    return std::make_pair(atof(parts.first.c_str()), atof(parts.second.c_str()));
}

std::pair<float, float> DICOM::extractSpaces(std::pair<float, float> compressScale) {
    std::pair spaces = this->extractSpaces();
    spaces.first *= compressScale.first;
    spaces.second *= compressScale.second;
    return spaces;
}

cv::Point3f DICOM::extractPosition() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFStringArray(DCM_ImagePositionPatient, data);
    cv::Point3f result;
    if(condition.bad())
        throw std::runtime_error(condition.text());
    auto firstParts = separateData(data);
    auto secondParts = separateData(firstParts.second);
    result.x = (float)atof(firstParts.first.c_str());
    result.y = (float)atof(secondParts.first.c_str());
    result.z = (float)atof(secondParts.second.c_str());
    return result;
}

std::array<float, 6> DICOM::extractOrientation() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFStringArray(DCM_ImageOrientationPatient, data);
    std::array<float, 6> result({0});
    if(condition.bad())
        throw std::runtime_error(condition.text());
    for(int i = 0 ; i != 5; ++i){
        auto parts = separateData(data);
        data = parts.second;
        result[i] = (float)atof(parts.first.c_str());
    }
    result[5] = (float)atof(data.c_str());
    return result;
}

std::string DICOM::extractResearchType() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFString(DCM_Modality, data);
    if(condition.bad())
        std::cout << "cannot define the type of research" << std::endl;
    return data.c_str();
}

std::string DICOM::extractScanningSequence() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFStringArray(DCM_ScanningSequence, data);
    if(condition.bad())
        std::cout << "cannot define method of research" << std::endl;
    return data.c_str();
}

std::string DICOM::extractDate() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFString(DCM_StudyDate, data);
    if(condition.bad())
        return std::string();
    return data.c_str();
}

std::string DICOM::extractTime() {
    OFCondition condition;
    OFString data;
    condition = dataset.findAndGetOFString(DCM_StudyTime, data);
    if(condition.bad())
        return std::string();
    return data.c_str();
}

std::pair<OFString, OFString> DICOM::separateData(const OFString& src) {
    size_t sepN = src.find('\\');
    return std::make_pair(src.substr(0, sepN), src.substr(sepN+1));
}


/////////////////////////////////////////////////////////////////////////////////////
///Методы, которые не пригодились, но могут быть полезны когда-нибудь
int DICOM::getIntData(DcmDataset& ds, const DcmTagKey& key){
    OFCondition condition;
    OFString data;
    condition = ds.findAndGetOFStringArray(key, data);
    if(condition.bad())
        throw std::runtime_error(condition.text());
    return atoi(data.c_str());
}

float DICOM::getFloatData(DcmDataset& ds, const DcmTagKey& key){
    OFCondition condition;
    OFString data;
    condition = ds.findAndGetOFStringArray(key, data);
    if(condition.bad())
        throw std::runtime_error(condition.text());
    return (float)atof(data.c_str());
}
