#include "model_builder.hpp"
#include "head_cloud.hpp"
#include "post_processing.hpp"
#include <fstream>
#include <filesystem>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Scale_space_surface_reconstruction_3.h>
#include <CGAL/Scale_space_reconstruction_3/Advancing_front_mesher.h>


typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;
typedef CGAL::Scale_space_surface_reconstruction_3<Kernel>                 Reconstruction;
typedef CGAL::Scale_space_reconstruction_3::Weighted_PCA_smoother<Kernel>  Smoother;
typedef CGAL::Scale_space_reconstruction_3::Advancing_front_mesher<Kernel> Mesher;
typedef Reconstruction::Facet_const_iterator Facet_iterator;
typedef Reconstruction::Point                Point_ss;


namespace {
    /// @brief Выполняет построение полигональной модели по облаку точек
    /// @param cloud Заданное облако точек
    /// @param model_directory Путь, куда сохраняется модель
    /// @param filename Имя сохраняемой модели (без формата)
    /// @param format Формат сохранения модели
    void build_model(std::vector<cv::Point3f> &cloud,
                     const std::string &model_directory,
                     const std::string &model_path);
    
    /// @brief Перевод типа данных облака точек openCV в CGAL
    /// @param cv_cloud Облако точек openCV
    /// @return Облако точек CGAL
    std::vector<Kernel::Point_3> cast_cloud_CV2CGAL(std::vector<cv::Point3f> &cv_cloud);

    /// @brief Построение модели полигональной модели по облаку точек методом space scale
    /// @param cloud - Исходное облако точек
    /// @return Полигональная модель в представлении CGAL
    Reconstruction space_scale(std::vector<Kernel::Point_3> &cloud);

    /// @brief Сохранение полученной полигональной модели в формате PLY
    /// @param reconstruct Полигональная модель в представлении CGAL
    /// @param model_directory Директория, куда сохраняется модель
    /// @param fileName Имя сохраняемой модели
    void save_to_ply(Reconstruction reconstruct,
                     const std::string &model_directory,
                     const std::string &fileName);

    /// @brief Сохранение полученной полигональной модели в формате DAE
    /// @param reconstruct Полигональная модель в представлении CGAL
    /// @param model_directory Директория, куда сохраняется модель
    /// @param fileName Имя сохраняемой модели
    void save_to_dae(Reconstruction reconstruct,
                     const std::string &model_directory,
                     const std::string &fileName);
}

void MODEL_BUILDER::build(const std::string &dcm_path,
                          const std::string &model_directory,
                          const std::string &filename,
                          bool visualise) {
    std::vector<cv::Point3f> cloud = HEAD_POINT_CLOUD::head_cloud(dcm_path);
    build_model(cloud, model_directory, filename);
    VTK_POSTPROCESSING::postprocess(model_directory, filename, visualise);
}

vtkSmartPointer<vtkPolyData> MODEL_BUILDER::build(const std::string &dcm_path,
                                                  const std::string &model_directory,
                                                  const std::string &filename) {
    std::vector<cv::Point3f> cloud = HEAD_POINT_CLOUD::head_cloud(dcm_path);
    build_model(cloud, model_directory, filename);
    return VTK_POSTPROCESSING::postprocess(model_directory, filename, false);
}

namespace {
    void build_model(std::vector<cv::Point3f> &cv_cloud,
                     const std::string &model_directory,
                     const std::string &filename) {
        std::cout << "Calculating Cloud" << std::endl;
        std::vector<Kernel::Point_3> cloud = cast_cloud_CV2CGAL(cv_cloud);
        std::cout << "Building Model" << std::endl;
        Reconstruction reconstruct = space_scale(cloud);
        std::cout << "Saving" << std::endl;
        std::filesystem::create_directories(model_directory);
        save_to_ply(reconstruct, model_directory, filename);
    }

    std::vector<Kernel::Point_3> cast_cloud_CV2CGAL(std::vector<cv::Point3f> &cv_cloud) {
        std::vector<Kernel::Point_3> cloud;
        for(auto &cv_point: cv_cloud)
            cloud.emplace_back(Kernel::Point_3(cv_point.x, cv_point.y, cv_point.z));
        return cloud;
    }

    Reconstruction space_scale(std::vector<Kernel::Point_3> &cloud) {
        Reconstruction reconstruct(cloud.begin(), cloud.end());
        reconstruct.increase_scale<Smoother>(4);
        reconstruct.reconstruct_surface(Mesher(20));
        return reconstruct;
    }

    void save_to_ply(Reconstruction reconstruct,
                     const std::string &model_directory,
                     const std::string &fileName) {
        std::ofstream outputFile(model_directory + "/" + fileName + ".ply");

        outputFile << "ply\n";
        outputFile << "format ascii 1.0\n";
        outputFile << "element vertex " << reconstruct.number_of_points() << "\n";
        outputFile << "property float32 x\n";
        outputFile << "property float32 y\n";
        outputFile << "property float32 z\n";
        outputFile << "element face " << reconstruct.number_of_facets() << "\n";
        outputFile << "property list uint8 int32 vertex_indices\n";
        outputFile << "end_header\n";

        std::copy(reconstruct.points_begin(),
                  reconstruct.points_end(), 
                  std::ostream_iterator<Point_ss>(outputFile, "\n"));

        for(Facet_iterator it  = reconstruct.facets_begin(); it != reconstruct.facets_end(); it++)
            outputFile << "3 " << *it << "\n";
    }

    void save_to_dae(Reconstruction reconstruct,
                     const std::string &model_directory,
                     const std::string &fileName) {
        std::ofstream outputFile(model_directory + "/" + fileName + ".dae");

        outputFile << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                      "<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">\n"
                      "  <asset>\n"
                      "    <contributor>\n"
                      "      <author>MRobot</author>\n"
                      "    </contributor>\n"
                      "    <up_axis>Z_UP</up_axis>\n"
                      "  </asset>\n"
                      "\n"
                      "  <library_geometries>\n"
                      "    <geometry id=\"head_mesh\" name=\"head\">\n"
                      "      <mesh>\n"
                      "        <source id=\"mesh_points\">\n"
                      "          <float_array name=\"values\" count=\"";

        outputFile << reconstruct.number_of_points() * 3 << "\">\n            ";

        std::copy(reconstruct.points_begin(),
                  reconstruct.points_end(), 
                  std::ostream_iterator<Point_ss>(outputFile, "\n            "));

        outputFile << "          </float_array>\n"
                      "          <technique_common>\n"
                      "            <accessor source=\"#values\" count=\"";
        outputFile << reconstruct.number_of_points() << "\" stride=\"3\">\n"
                      "              <param name=\"X\" type=\"float\"/>\n"
                      "              <param name=\"Y\" type=\"float\"/>\n"
                      "              <param name=\"Z\" type=\"float\"/>\n"
                      "            </accessor>\n"
                      "          </technique_common>\n"
                      "        </source>\n"
                      "\n"
                      "        <triangles count=\"";
        outputFile << reconstruct.number_of_facets() << "\">\n";
        outputFile << "          <input semantic=\"POSITION\" source=\"#mesh_points\" offset=\"0\"/>\n"
                      "          <p>\n";

        for(Facet_iterator it  = reconstruct.facets_begin(); it != reconstruct.facets_end(); it++)
            outputFile << "            " << *it << "\n";

        outputFile << "        </p>\n"
                      "        </triangles>\n"
                      "      </mesh>\n"
                      "    </geometry>\n"
                      "  </library_geometries>\n"
                      "\n"
                      "  <library_visual_scenes>\n"
                      "    <visual_scene id=\"Scene\" name=\"Scene\">\n"
                      "      <node id=\"head_model\" name=\"Head_model\" type=\"NODE\">\n"
                      "        <instance_geometry url=\"#head_mesh\" name=\"head_geometry\"/>\n"
                      "      </node>\n"
                      "    </visual_scene>\n"
                      "  </library_visual_scenes>\n"
                      "  \n"
                      "</COLLADA>";
    }
} //namespace
