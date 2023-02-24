#ifndef VTK_POST_PROCESSING
#define VTK_POST_PROCESSING

#include <string>
#include <vtkPolyData.h>


namespace VTK_POSTPROCESSING {
    vtkSmartPointer<vtkPolyData> postprocess(const std::string &model_directory,
                                             const std::string &filename,
                                             bool visualise);
}


#endif //VTK_POST_PROCESSING
