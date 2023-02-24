#ifndef HEAD_CLOUD_HPP
#define HEAD_CLOUD_HPP

#include <filesystem>
#include "utility_dcm.hpp"

namespace HEAD_POINT_CLOUD {
    void head_cloud_output(const std::string &directory_src,
                       const std::string &directory_dst);
    std::vector<cv::Point3f> head_cloud(const std::string &directory_src);
}

namespace {
    class HeadCloud {
    public:
        HeadCloud(const std::string &directory);
    public:
        void sort();
        void equalizeImages();
        std::vector<cv::Point3f> headSurfaceCloud();
        void saveCloudPLY(std::vector<cv::Point3f> &cloud,
                          const std::string &directory);
    private:
        std::vector<std::string> getPaths(const std::string &directory);
        uint8_t defineThreshold();
        std::vector<int> buildHistogram();
        uint8_t histogramThreshold(std::vector<int> &histogram);
        std::vector<cv::Mat> headSurfaceContours(uint8_t threshold);
        void gaussianKernel(int &g_kernel, int &g_sigma);
        cv::Mat maskCT(cv::Mat &image, uint8_t threshold, int g_kernel, int g_sigma);
        cv::Mat maskMRI(cv::Mat &image, uint8_t threshold, int g_kernel, int g_sigma);
        cv::Mat createLattice(int rows, int cols);
    private:
        std::vector<cv::Mat> images;
        std::vector<cv::Point3f> positions;
        std::pair<float, float> spaces;
        std::array<float, 6> orientation;
        std::string research_type;
};
}


#endif //HEAD_CLOUD_HPP