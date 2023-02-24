#ifndef MODEL_BUILDER_HPP
#define MODEL_BUILDER_HPP

#include <string>
#include <vtkPolyData.h>


namespace MODEL_BUILDER {
    /// @brief Построение полигональной модели по данным исследований в формате DICOM
    /// @param dcm_path Путь к репозиторию с исследованием
    /// @param model_directory Путь к репозиторию, в который будет сохранена модель
    /// @param filename Имя сохраняемой модели (без указания формата)
    /// @param visualise Визуализация построенной модели
    void build(const std::string &dcm_path,
               const std::string &model_directory,
               const std::string &filename,
               bool visualise);
    
    /// @brief Построение полигональной модели по данным исследований в формате DICOM
    /// @param dcm_path Путь к репозиторию с исследованием
    /// @param model_directory Путь к репозиторию, в который будет сохранена модель
    /// @param filename Имя сохраняемой модели (без указания формата)
    vtkSmartPointer<vtkPolyData> build(const std::string &dcm_path,
                                       const std::string &model_directory,
                                       const std::string &filename);
}


#endif //MODEL_BUILDER_HPP
