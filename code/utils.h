#ifndef UTILS_H
#define UTILS_H

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>  // 用於 Subdiv2D
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <sstream>
#include <algorithm>  // 用於 min, max
#include <set>        // 用於 set
#include <cmath>      // 為了 floor, ceil
#include <random>
#include <functional> // 新增：用於 std::function
#include <omp.h>

using namespace cv;
using namespace Eigen;
using namespace std;

// 前向宣告
bool pointInTriangle(const Point2f& p, const Point2f& a, const Point2f& b, const Point2f& c);

// 邊結構體
struct Edge {
    Point2f p1, p2;

    Edge(Point2f _p1, Point2f _p2) {
        // 保證點的順序一致，便於比較
        if (_p1.x < _p2.x || (_p1.x == _p2.x && _p1.y < _p2.y)) {
            p1 = _p1;
            p2 = _p2;
        }
        else {
            p1 = _p2;
            p2 = _p1;
        }
    }

    bool operator==(const Edge& other) const {
        return (abs(p1.x - other.p1.x) < 1e-3 && abs(p1.y - other.p1.y) < 1e-3 &&
            abs(p2.x - other.p2.x) < 1e-3 && abs(p2.y - other.p2.y) < 1e-3);
    }

    bool operator<(const Edge& other) const {
        if (abs(p1.x - other.p1.x) > 1e-3) return p1.x < other.p1.x;
        if (abs(p1.y - other.p1.y) > 1e-3) return p1.y < other.p1.y;
        if (abs(p2.x - other.p2.x) > 1e-3) return p2.x < other.p2.x;
        return p2.y < other.p2.y;
    }
};

// 點類型枚舉
enum PointType {
    CORRESPONDENCE_POINT,
    NON_OVERLAP_POINT,
    MASK_CONTOUR_POINT,    // 邊界法線生成的輪廓點
    MASK_CORNER_POINT      // 遮罩轉折點（角點）
};

// 三角形結構體 - 取代原本的 Grid
struct Triangle {
    int vertex_indices[3];      // 三個頂點的索引
    Point2f vertices[3];        // 三個頂點的座標
    vector<float> coefficients; // 校正係數 (6個係數: RGB各2個)
    vector<int> neighbor_triangles; // 相鄰三角形的索引

    Triangle() {
        coefficients.resize(6, 1.0f); // 初始化係數為1
        for (int i = 0; i < 3; i++) {
            vertex_indices[i] = -1;
        }
    }

    // 檢查點是否在三角形內
    bool contains(const Point2f& point) const {
        return pointInTriangle(point, vertices[0], vertices[1], vertices[2]);
    }

    // 計算三角形面積
    float area() const {
        return abs((vertices[1].x - vertices[0].x) * (vertices[2].y - vertices[0].y) -
            (vertices[2].x - vertices[0].x) * (vertices[1].y - vertices[0].y)) / 2.0f;
    }
};

// Delaunay triangulation 結果結構體
struct DelaunayTriangulation {
    vector<Point2f> points;          // 所有的點
    vector<PointType> pointTypes;    // 新增：每個點的類型
    vector<Triangle> triangles;      // 所有的三角形
    Rect2f bounds;                   // 邊界範圍

    // 根據像素座標找到對應的三角形
    int findTriangleContaining(const Point2f& pixel) const {
        for (int i = 0; i < triangles.size(); i++) {
            if (triangles[i].contains(pixel)) {
                return i;
            }
        }
        return -1; // 沒找到
    }
};

// 對應點結構體
struct Correspondence {
    float x1, y1;  // 第一張圖片的點座標
    float x2, y2;  // 第二張圖片的點座標

    Correspondence(float _x1, float _y1, float _x2, float _y2)
        : x1(_x1), y1(_y1), x2(_x2), y2(_y2) {
    }
};

// 圖片間對應關係結構體
struct ImageCorrespondences {
    int img1_idx, img2_idx;  // 圖片索引
    vector<Correspondence> correspondences;  // 對應點列表

    ImageCorrespondences(int i1, int i2) : img1_idx(i1), img2_idx(i2) {}
};

// 自定義輸出類別，同時寫入檔案和終端
class DualOutput {
private:
    ofstream* fileStream;
    streambuf* coutBuffer;

public:
    DualOutput(ofstream* file);

    template<typename T>
    DualOutput& operator<<(const T& data) {
        // 寫入檔案
        if (fileStream && fileStream->is_open()) {
            *fileStream << data;
            fileStream->flush();
        }
        // 寫入終端
        cout.rdbuf(coutBuffer);
        cout << data;
        cout.flush();
        return *this;
    }

    // 處理 endl 等manipulator
    DualOutput& operator<<(ostream& (*pf)(ostream&));
};

// === 原有網格相關函數宣告 ===
bool fileExists(const string& filename);
Mat smartReadImage(const string& filename, int flags = IMREAD_COLOR);
bool createDirectory(const string& path);

// === Correspondences 相關函數宣告 ===
vector<Correspondence> readCorrespondencesCSV(const string& filename);
vector<ImageCorrespondences> loadAllCorrespondences(const string& globalPath, const vector<int>& validImageIndices);
void printCorrespondencesStatistics(const vector<ImageCorrespondences>& allCorrespondences);

// === 新增：Non-Overlap Features 相關函數宣告 ===
vector<Point2f> readNonOverlapFeaturesCSV(const string& filename);
vector<vector<Point2f>> loadAllNonOverlapFeatures(const string& nonOverlapPath, const vector<int>& validImageIndices);

// === Delaunay Triangle 相關函數宣告 ===
bool pointInTriangle(const Point2f& p, const Point2f& a, const Point2f& b, const Point2f& c);
vector<Point2f> findMaskCornerPoints(const Mat& mask);
vector<Point2f> filterClosePoints(const vector<Point2f>& points, float minDistance);
vector<Point2f> sampleMaskContour(const Mat& mask, int numSamples);

// === 新增：邊界邊相關函數宣告 ===
vector<Edge> findBoundaryEdges(const DelaunayTriangulation& triangulation);
vector<Point2f> orderBoundaryEdges(const vector<Edge>& boundaryEdges);
vector<Edge> extractLargestClosedContour(const vector<Edge>& edges);
vector<Edge> filterTrueBoundaryEdges(const DelaunayTriangulation& triangulation,
    const Size& imageSize);

// === 新增：基於邊界邊法線的輪廓點生成 ===
Mat createCombinedMask(const vector<Mat>& imageMasks);
vector<Point2f> generateBoundaryNormalContourPoints(
    const vector<Edge>& boundaryEdges,
    const Mat& combinedMask,
    const Size& imageSize);

// === 新增：提取遮罩輪廓的轉折點（角點檢測） ===
// 方案1：簡化版（推薦）
vector<Point2f> extractMaskContourCorners(const Mat& mask,
    double qualityLevel = 0.01,
    double minDistance = 10.0,
    int blockSize = 3,
    bool useHarrisDetector = false);

// === 特徵點專用三角化 ===
DelaunayTriangulation createFeatureOnlyTriangulation(
    const vector<ImageCorrespondences>& correspondences,
    const Size& imageSize,
    const vector<vector<Point2f>>& nonOverlapFeatures);

void visualizeFeatureOnlyTriangulation(const DelaunayTriangulation& triangulation,
    const Size& imageSize,
    const string& outputPath,
    const string& datasetName);

// === 完整三角化（包含輪廓點和角點） ===
DelaunayTriangulation createDelaunayFromCorrespondences(
    const vector<ImageCorrespondences>& correspondences,
    const Size& imageSize,
    const vector<Mat>& imageMasks,
    const vector<vector<Point2f>>& nonOverlapFeatures);

void findTriangleNeighbors(DelaunayTriangulation& triangulation);
vector<float> calculateTriangleMean(const Mat& img, const Mat& overlapMask, const Mat& imageMask, const Triangle& triangle);

// === 邊界約束相關函數宣告 ===
Mat findMaskBoundary(const Mat& mask1, const Mat& mask2);
Mat dilatedBoundary(const Mat& boundary, int dilationPixels);
vector<float> calculateTriangleBoundaryMean(const Mat& img, const Mat& boundary, const Mat& imageMask, const Triangle& triangle);
bool triangleIntersectsBoundary(const Mat& boundary, const Triangle& triangle);

// === 求解和應用校正 ===
void solveTriangleCoefficients(vector<Mat>& images, const vector<Mat>& imageMasks,
    const vector<vector<Mat>>& overlaps,
    vector<DelaunayTriangulation>& triangulations);
void applyTriangleCorrections(vector<Mat>& images, const vector<Mat>& imageMasks,
    const vector<DelaunayTriangulation>& triangulations);

void processTriangleColorCorrection(vector<Mat>& images, const vector<Mat>& imageMasks,
    const vector<vector<Mat>>& overlaps,
    const vector<ImageCorrespondences>& correspondences,
    const vector<vector<Point2f>>& nonOverlapFeatures);

// === 驗證和輔助函數 ===
bool isValidPoint(float x, float y, const Size& imageSize);
bool isValidTriangle(const Triangle& triangle, const Size& imageSize);
// 修改函數聲明，添加新參數
void visualizeDelaunayTriangulation(const DelaunayTriangulation& triangulation,
    const Size& imageSize,
    const string& outputPath,
    const string& datasetName,
    const vector<Mat>& imageMasks,
    const vector<ImageCorrespondences>& correspondences,
    const vector<vector<Point2f>>& nonOverlapFeatures);
bool isTriangleInsideImage(const Triangle& triangle, const Size& imageSize);
bool hasValidPixels(const Mat& img, const Mat& imageMask, const Triangle& triangle);
bool hasValidPixelsInOverlap(const Mat& img, const Mat& overlapMask, const Mat& imageMask, const Triangle& triangle);

// === 主要處理函數宣告 ===
bool processSingleDataset(const string& basePath);

#endif // UTILS_H