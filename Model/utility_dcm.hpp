#ifndef UTILITY_DCM_HPP
#define UTILITY_DCM_HPP

#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmimgle/dcmimage.h"
#include "dcmtk/dcmjpeg/djdecode.h"
#include <opencv2/opencv.hpp>


class DICOM {
public:
    explicit DICOM(const std::string& path);
    virtual ~DICOM()=default;
private:
    DcmDataset                  dataset;       /// Данные из файла
public:
    /// @brief Переводит матрицу изображения из 16-битной в 8-битную
    /// @param divider - Делитель исходного 16-битного числа,
    ///                  для перевода в соответствующее 8-битное
    /// @return 8-битная матрица
    cv::Mat                     equalize(int16_t divider);

    /// @brief Находит контур головы
    /// @param threshold - Граничное значение интенсивности цвета скальпа
    /// @return Матрица размера исходного изображения с контуром головы
    cv::Mat                     findHeadSurface(uint8_t threshold);
public:
    /// Возвращает матрицу изображения, пригодную для работы с openCV
    cv::Mat                     extractImage();
    cv::Mat                     extractImageCLAHE();
    /// Возвращает матрицу изображения, пригодную для работы с openCV,
    /// сжатая в соответствии со значениями compressScale
    cv::Mat                     extractImage(std::pair<float, float> compressScale);
    /// Возвращает наибольшее значение яркости пикселя в данном срезе
    int16_t                     extractLargestValue();
    /// Возвращает расстояния между пикселями
    std::pair<float, float>     extractSpaces();
    std::pair<float, float>     extractSpaces(std::pair<float, float> compressScale);
    /// Возвращает положение изображения в пространстве
    cv::Point3f                 extractPosition();
    /// Возвращает ориентацию изображения cos's
    std::array<float, 6>        extractOrientation();
    /// Возвращает тип исследования
    std::string                 extractResearchType();
    /// Возвращает импульсную последовательность проведенного исследования
    std::string                 extractScanningSequence();
    /// Возвращает дату проведенного исследования
    std::string                 extractDate();
    /// Возвращает время проведенного иссследования
    std::string                 extractTime();

private:
    /// Извлечение датасета из зашифрованного (сжатого) DICOM файла
    DcmDataset                  extractDataset(const std::string& path);
    /// Разделяет данные, полученные от OFStringArray
    std::pair<OFString, OFString> separateData(const OFString& src);

///Методы, которые не пригодились, но могут быть полезны когда-нибудь
private:
    int getIntData(DcmDataset& dataset, const DcmTagKey& key);
    float getFloatData(DcmDataset& dataset, const DcmTagKey& key);
};


#endif //UTILITY_DCM_HPP
