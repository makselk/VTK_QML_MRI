#ifndef LAYOUT_10_20_HPP
#define LAYOUT_10_20_HPP

#include <vtkActor.h>
#include <vtkPoints.h>
#include <vtkOBBTree.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkKdTreePointLocator.h>


namespace LAYOUT_10_20 {
    /// @brief Размечает модель головы по системе 10-20
    /// @param model Модель головы
    /// @param nasion
    /// @param inion 
    /// @param tragus_l 
    /// @param tragus_r
    /// @param center
    /// @return Найденные точки, записанные в последовательности:
    /// Fpz, Fz, Cz, Pz, Oz, T3, C3, C4, T4, F7, Fp1, Fp2, F8, F3, F4, T5, O1, O2, T6, P3, P4
    vtkSmartPointer<vtkPoints> mark(vtkPolyData* model,
                                    vtkKdTreePointLocator* kd_tree,
                                    vtkOBBTree* obb_tree,
                                    double* nasion,
                                    double* inion,
                                    double* tragus_l,
                                    double* tragus_r,
                                    double* center);

    
    /// @brief Поиск центра масс, основываясь на заданных точках
    /// @param nasion 
    /// @param inion 
    /// @param tragus_l 
    /// @param tragus_r 
    /// @param center 
    void centerOfMass(double* nasion,
                      double* inion,
                      double* tragus_l,
                      double* tragus_r,
                      double* center);
    

    /// @brief Создает актера точки
    /// @param coordinates 
    /// @return 
    vtkSmartPointer<vtkActor> pointActor(double* coordinates);


    /// @brief Создает актера точки
    vtkSmartPointer<vtkActor> pointActor(double* coordinates, 
                                         double r,
                                         double g,
                                         double b);
}


#endif //LAYOUT_10_20_HPP