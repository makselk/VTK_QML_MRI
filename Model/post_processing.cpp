#include "post_processing.hpp"

// Post-processing
#include <vtkNew.h>
#include <vtkNamedColors.h>
#include <vtkPLYReader.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkFillHolesFilter.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkPLYWriter.h>
#include <vtkSTLWriter.h>

// Visualise
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>


namespace {
    void visualise_model(vtkPolyData *data);
    void save_to_dae(vtkPolyData *data,
                     const std::string &model_directory,
                     const std::string &fileName);
    void save_to_ply(vtkPolyData* data,
                     const std::string &model_directory,
                     const std::string &filename);
    void save_to_stl(vtkPolyData* data,
                     const std::string &model_directory,
                     const std::string &filename);
}

vtkSmartPointer<vtkPolyData> VTK_POSTPROCESSING::postprocess(const std::string &model_directory,
                                                             const std::string &filename,
                                                             bool visualise) {
    vtkNew<vtkPLYReader> reader;
    reader->SetFileName((model_directory + "/" + filename + ".ply").c_str());
    reader->Update();

    vtkNew<vtkSmoothPolyDataFilter> smooth;
    smooth->SetInputData(reader->GetOutput());
    smooth->SetNumberOfIterations(25);
    smooth->SetRelaxationFactor(0.1);
    smooth->FeatureEdgeSmoothingOff();
    smooth->BoundarySmoothingOn();
    smooth->Update();

    vtkNew<vtkFillHolesFilter> fill_holes;
    fill_holes->SetInputData(smooth->GetOutput());
    fill_holes->SetHoleSize(100000.0);
    fill_holes->Update();

    vtkNew<vtkPolyDataConnectivityFilter> confilter;
    confilter->SetInputData(fill_holes->GetOutput());
    confilter->SetExtractionModeToLargestRegion();
    confilter->Update();

    vtkNew<vtkPolyData> poly_data;
    poly_data->DeepCopy(confilter->GetOutput());

    save_to_dae(poly_data, model_directory, filename);
    save_to_ply(poly_data, model_directory, filename);
    
    if(visualise) {
        vtkNew<vtkPolyData> polyDataTest;
        polyDataTest->ShallowCopy(poly_data);
        visualise_model(poly_data);
    }

    return poly_data;
}

namespace {
    void save_to_dae(vtkPolyData *data,
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

        outputFile << data->GetNumberOfPoints() * 3 << "\">\n            ";

        for(auto i = 0; i != data->GetNumberOfPoints(); ++i) {
            double *point;
            point = data->GetPoint(i);
            outputFile << point[0] << " " << point[1] << " " << point[2] << "\n            ";
        }

        outputFile << "          </float_array>\n"
                      "          <technique_common>\n"
                      "            <accessor source=\"#values\" count=\"";
        outputFile << data->GetNumberOfPoints() << "\" stride=\"3\">\n"
                      "              <param name=\"X\" type=\"float\"/>\n"
                      "              <param name=\"Y\" type=\"float\"/>\n"
                      "              <param name=\"Z\" type=\"float\"/>\n"
                      "            </accessor>\n"
                      "          </technique_common>\n"
                      "        </source>\n"
                      "\n"
                      "        <triangles count=\"";
        outputFile << data->GetNumberOfCells() << "\">\n";
        outputFile << "          <input semantic=\"POSITION\" source=\"#mesh_points\" offset=\"0\"/>\n"
                      "          <p>\n";

        vtkNew<vtkIdList> idL;
        data->GetPolys()->InitTraversal();
        while(data->GetPolys()->GetNextCell(idL))
            outputFile << "            " << idL->GetId(0) << " " << idL->GetId(1) << " " << idL->GetId(2) << "\n";

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

    void save_to_ply(vtkPolyData* data,
                     const std::string &model_directory,
                     const std::string &filename) {
        vtkNew<vtkPLYWriter> writer;
        writer->SetInputData(data);
        writer->SetFileName((model_directory + "/" + filename + ".ply").c_str());
        writer->Update();
    }

    // Эта падла занимает более чем в 15 раз больше места
    void save_to_stl(vtkPolyData* data,
                     const std::string &model_directory,
                     const std::string &filename) {
        vtkNew<vtkSTLWriter> writer;
        writer->SetInputData(data);
        writer->SetFileTypeToASCII();
        writer->SetFileName((model_directory + "/" + filename + ".stl").c_str());
        writer->Update();
    }

    void visualise_model(vtkPolyData *data) {
        vtkNew<vtkNamedColors> colors;
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(data);

        vtkNew<vtkActor> actor; 
        actor->SetMapper(mapper);
        actor->GetProperty()->SetDiffuseColor(colors->GetColor3d("NavajoWhite").GetData());
        
        vtkNew<vtkRenderer> renderer;
        renderer->AddActor(actor);
        renderer->SetBackground(colors->GetColor3d("LightSlateGray").GetData());

        vtkNew<vtkRenderWindow> render_window;
        render_window->SetSize(1000, 1000);
        render_window->SetWindowName("Model");
        render_window->AddRenderer(renderer);

        vtkNew<vtkRenderWindowInteractor> iren;
        render_window->SetInteractor(iren);

        vtkNew<vtkInteractorStyleTrackballCamera> style;
        iren->SetInteractorStyle(style);

        render_window->Render();
        iren->Start();
    }
} //namespace
