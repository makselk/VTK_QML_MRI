#ifndef STRECH_GRID_HPP
#define STRECH_GRID_HPP

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkKdTreePointLocator.h>
#include <vtkOBBTree.h>


namespace STRECH_GRID {
    vtkSmartPointer<vtkPoints> stretchGridOnModel(int num_of_points,
                                                  double spacing,
                                                  double* grid_position,
                                                  vtkPolyData* model,
                                                  double* model_center,
                                                  vtkKdTreePointLocator* kd_tree,
                                                  vtkOBBTree* obb_tree,
                                                  vtkPolyData* grid_poly_data);
}

#endif //STRETCH_GRID_HPP