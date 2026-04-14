#include "utils.h"

// DualOutput 類別實作
DualOutput::DualOutput(ofstream* file) : fileStream(file), coutBuffer(cout.rdbuf()) {}

DualOutput& DualOutput::operator<<(ostream& (*pf)(ostream&)) {
    if (fileStream && fileStream->is_open()) {
        *fileStream << pf;
        fileStream->flush();
    }
    cout.rdbuf(coutBuffer);
    cout << pf;
    cout.flush();
    return *this;
}

// 檢查檔案是否存在的函數
bool fileExists(const string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

// 智慧讀取圖像檔案的函數
Mat smartReadImage(const string& filename, int flags) {
    if (!fileExists(filename)) {
        // 檔案不存在，直接返回空的Mat，不會產生OpenCV警告
        return Mat();
    }
    return imread(filename, flags);
}

// Windows 下創建目錄的函數
bool createDirectory(const string& path) {
    if (_mkdir(path.c_str()) == 0) {
        cout << "Created directory: " << path << endl;
        return true;
    }
    else {
        // 檢查是否因為目錄已存在而失敗
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            cout << "Directory already exists: " << path << endl;
            return true;
        }
        else {
            cout << "Failed to create directory: " << path << " (Error: " << error << ")" << endl;
            return false;
        }
    }
}

// 新增：讀取單個 correspondences CSV 檔案
vector<Correspondence> readCorrespondencesCSV(const string& filename) {
    vector<Correspondence> correspondences;

    if (!fileExists(filename)) {
        cout << "Correspondences file does not exist: " << filename << endl;
        return correspondences;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Failed to open correspondences file: " << filename << endl;
        return correspondences;
    }

    string line;
    int lineCount = 0;

    while (getline(file, line)) {
        lineCount++;

        // 跳過空行
        if (line.empty()) {
            cout << "Skipping empty line " << lineCount << " in " << filename << endl;
            continue;
        }

        // 使用 stringstream 來解析 CSV (逗號分隔)
        istringstream ss(line);
        string token;
        vector<float> values;

        // 讀取四個值：src_x, src_y, tgt_x, tgt_y
        try {
            // 讀取第一個值 (src_x)
            if (getline(ss, token, ',')) {
                values.push_back(stof(token));
            }
            // 讀取第二個值 (src_y)
            if (getline(ss, token, ',')) {
                values.push_back(stof(token));
            }
            // 讀取第三個值 (tgt_x)
            if (getline(ss, token, ',')) {
                values.push_back(stof(token));
            }
            // 讀取第四個值 (tgt_y) - 最後一個值可能沒有逗號
            if (getline(ss, token)) {  // 不指定分隔符，讀取到行尾
                values.push_back(stof(token));
            }
        }
        catch (const exception& e) {
            cout << "Error parsing line " << lineCount << " in " << filename
                << ": " << e.what() << " (line content: '" << line << "')" << endl;
            continue;
        }

        // 檢查是否有 4 個值
        if (values.size() == 4) {
            correspondences.emplace_back(values[0], values[1], values[2], values[3]);
        }
        else {
            cout << "Warning: Invalid line format in " << filename
                << " at line " << lineCount << " (expected 4 values, got "
                << values.size() << ") - line content: '" << line << "'" << endl;
        }
    }

    file.close();
    //cout << "Successfully loaded " << correspondences.size() << " correspondences from " << filename << endl;

    return correspondences;
}

// 新增：載入所有的 correspondences 檔案
vector<ImageCorrespondences> loadAllCorrespondences(const string& globalPath, const vector<int>& validImageIndices) {
    vector<ImageCorrespondences> allCorrespondences;

    cout << "\n--- Loading Correspondences ---" << endl;
    cout << "Global path: " << globalPath << endl;
    cout << "Valid image indices: ";
    for (int idx : validImageIndices) {
        cout << idx << " ";
    }
    cout << endl;

    int totalFiles = 0;
    int loadedFiles = 0;

    // 遍歷所有有效圖片的組合
    for (int i = 0; i < validImageIndices.size(); i++) {
        for (int j = i + 1; j < validImageIndices.size(); j++) {
            int img1_idx = validImageIndices[i];
            int img2_idx = validImageIndices[j];

            // 構建檔案名稱 (兩個方向都嘗試)
            string filename1 = globalPath + to_string(img1_idx / 10) + to_string(img1_idx % 10) + "__" +
                to_string(img2_idx / 10) + to_string(img2_idx % 10) + "_inliers.csv";

            string filename2 = globalPath + to_string(img2_idx / 10) + to_string(img2_idx % 10) + "__" +
                to_string(img1_idx / 10) + to_string(img1_idx % 10) + "_inliers.csv";

            totalFiles += 2;  // 兩個可能的檔案

            // 嘗試讀取第一個方向的檔案
            if (fileExists(filename1)) {
                vector<Correspondence> corr = readCorrespondencesCSV(filename1);
                if (!corr.empty()) {
                    ImageCorrespondences imgCorr(img1_idx, img2_idx);
                    imgCorr.correspondences = corr;
                    allCorrespondences.push_back(imgCorr);
                    loadedFiles++;
                }
            }

            // 嘗試讀取第二個方向的檔案
            if (fileExists(filename2)) {
                vector<Correspondence> corr = readCorrespondencesCSV(filename2);
                if (!corr.empty()) {
                    // 注意：如果兩個方向的檔案都存在，第二個會覆蓋第一個
                    // 或者你可以選擇合併或跳過
                    bool alreadyExists = false;
                    for (auto& existing : allCorrespondences) {
                        if ((existing.img1_idx == img1_idx && existing.img2_idx == img2_idx) ||
                            (existing.img1_idx == img2_idx && existing.img2_idx == img1_idx)) {
                            alreadyExists = true;
                            break;
                        }
                    }

                    if (!alreadyExists) {
                        ImageCorrespondences imgCorr(img2_idx, img1_idx);
                        imgCorr.correspondences = corr;
                        allCorrespondences.push_back(imgCorr);
                        loadedFiles++;
                    }
                }
            }
        }
    }

    cout << "Correspondences loading summary:" << endl;
    cout << "  Total possible files checked: " << totalFiles << endl;
    cout << "  Successfully loaded: " << loadedFiles << " files" << endl;
    cout << "  Image pairs with correspondences: " << allCorrespondences.size() << endl;

    return allCorrespondences;
}

// 新增：讀取單個非重疊特徵點 CSV 檔案
vector<Point2f> readNonOverlapFeaturesCSV(const string& filename) {
    vector<Point2f> features;

    if (!fileExists(filename)) {
        cout << "Non-overlap features file does not exist: " << filename << endl;
        return features;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Failed to open non-overlap features file: " << filename << endl;
        return features;
    }

    string line;
    int lineCount = 0;

    while (getline(file, line)) {
        lineCount++;

        // 跳過空行
        if (line.empty()) {
            continue;
        }

        // 使用 stringstream 來解析 CSV (逗號分隔)
        istringstream ss(line);
        string token;
        vector<float> values;

        // 讀取兩個值：x, y
        try {
            // 讀取 x 座標
            if (getline(ss, token, ',')) {
                values.push_back(stof(token));
            }
            // 讀取 y 座標
            if (getline(ss, token)) {
                values.push_back(stof(token));
            }
        }
        catch (const exception& e) {
            cout << "Error parsing line " << lineCount << " in " << filename
                << ": " << e.what() << " (line content: '" << line << "')" << endl;
            continue;
        }

        // 檢查是否有 2 個值
        if (values.size() == 2) {
            features.emplace_back(values[0], values[1]);
        }
        else {
            cout << "Warning: Invalid line format in " << filename
                << " at line " << lineCount << " (expected 2 values, got "
                << values.size() << ") - line content: '" << line << "'" << endl;
        }
    }

    file.close();
    //cout << "Successfully loaded " << features.size() << " non-overlap features from " << filename << endl;

    return features;
}

// 新增：載入所有圖片的非重疊特徵點
vector<vector<Point2f>> loadAllNonOverlapFeatures(const string& nonOverlapPath,
    const vector<int>& validImageIndices) {
    vector<vector<Point2f>> allNonOverlapFeatures;

    cout << "\n--- Loading Non-Overlap Features ---" << endl;
    cout << "Non-overlap path: " << nonOverlapPath << endl;

    int totalFeatures = 0;

    for (int idx : validImageIndices) {
        // 構建檔案名稱：00_non_overlap_features.csv, 01_non_overlap_features.csv, ...
        string filename = nonOverlapPath + to_string(idx / 10) + to_string(idx % 10) +
            "_non_overlap_features.csv";

        vector<Point2f> features = readNonOverlapFeaturesCSV(filename);
        allNonOverlapFeatures.push_back(features);
        totalFeatures += features.size();

        cout << "Image " << idx << ": " << features.size() << " non-overlap features" << endl;
    }

    cout << "Total non-overlap features loaded: " << totalFeatures << endl;
    cout << "-----------------------------------" << endl;

    return allNonOverlapFeatures;
}

// 新增：列印 correspondences 統計資訊
void printCorrespondencesStatistics(const vector<ImageCorrespondences>& allCorrespondences) {
    cout << "\n--- Correspondences Statistics ---" << endl;

    if (allCorrespondences.empty()) {
        cout << "No correspondences loaded." << endl;
        return;
    }

    int totalCorrespondences = 0;
    int minCorrespondences = INT_MAX;
    int maxCorrespondences = 0;

    for (const auto& imgCorr : allCorrespondences) {
        int count = imgCorr.correspondences.size();
        totalCorrespondences += count;
        minCorrespondences = min(minCorrespondences, count);
        maxCorrespondences = max(maxCorrespondences, count);

        cout << "Images " << imgCorr.img1_idx << "-" << imgCorr.img2_idx
            << ": " << count << " correspondences" << endl;
    }

    double avgCorrespondences = static_cast<double>(totalCorrespondences) / allCorrespondences.size();

    cout << "\nSummary:" << endl;
    cout << "  Total image pairs: " << allCorrespondences.size() << endl;
    cout << "  Total correspondences: " << totalCorrespondences << endl;
    cout << "  Average per pair: " << avgCorrespondences << endl;
    cout << "  Min per pair: " << minCorrespondences << endl;
    cout << "  Max per pair: " << maxCorrespondences << endl;
    cout << "-------------------------------" << endl;
}

// 檢查點是否在三角形內 (重心座標法)
bool pointInTriangle(const Point2f& p, const Point2f& a, const Point2f& b, const Point2f& c) {
    float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (abs(denom) < 1e-7) return false; // 退化三角形

    float alpha = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
    float beta = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
    float gamma = 1.0f - alpha - beta;

    return (alpha >= 0) && (beta >= 0) && (gamma >= 0);
}

// 重新設計的函數，找到遮罩的真實角點
vector<Point2f> findMaskCornerPoints(const Mat& mask) {
    vector<Point2f> cornerPoints;

    if (mask.empty()) {
        return cornerPoints;
    }

    // 找到所有非零點
    vector<Point> nonZeroPoints;
    findNonZero(mask, nonZeroPoints);

    if (nonZeroPoints.empty()) {
        return cornerPoints;
    }

    // 初始化極值
    Point topLeft = nonZeroPoints[0];      // 左上：x+y最小
    Point topRight = nonZeroPoints[0];     // 右上：x-y最大  
    Point bottomRight = nonZeroPoints[0];  // 右下：x+y最大
    Point bottomLeft = nonZeroPoints[0];   // 左下：y-x最大

    // 遍歷所有非零點找到真正的角點
    for (const Point& pt : nonZeroPoints) {
        // 左上角：x+y最小的點
        if (pt.x + pt.y < topLeft.x + topLeft.y) {
            topLeft = pt;
        }

        // 右上角：x-y最大的點 
        if (pt.x - pt.y > topRight.x - topRight.y) {
            topRight = pt;
        }

        // 右下角：x+y最大的點
        if (pt.x + pt.y > bottomRight.x + bottomRight.y) {
            bottomRight = pt;
        }

        // 左下角：y-x最大的點
        if (pt.y - pt.x > bottomLeft.y - bottomLeft.x) {
            bottomLeft = pt;
        }
    }

    // 按順序添加四個角點
    cornerPoints.push_back(Point2f(topLeft.x, topLeft.y));
    cornerPoints.push_back(Point2f(topRight.x, topRight.y));
    cornerPoints.push_back(Point2f(bottomRight.x, bottomRight.y));
    cornerPoints.push_back(Point2f(bottomLeft.x, bottomLeft.y));

    cout << "    真實遮罩角點:" << endl;
    cout << "      左上: (" << topLeft.x << ", " << topLeft.y << ")" << endl;
    cout << "      右上: (" << topRight.x << ", " << topRight.y << ")" << endl;
    cout << "      右下: (" << bottomRight.x << ", " << bottomRight.y << ")" << endl;
    cout << "      左下: (" << bottomLeft.x << ", " << bottomLeft.y << ")" << endl;

    return cornerPoints;
}

// 新增：找到兩個遮罩的邊界線
Mat findMaskBoundary(const Mat& mask1, const Mat& mask2) {
    // 邊界 = mask1 與 mask2 的交集的邊緣
    Mat intersection;
    bitwise_and(mask1, mask2, intersection);

    // 使用形態學操作找到邊界
    Mat boundary;
    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    Mat eroded;
    erode(intersection, eroded, kernel);
    subtract(intersection, eroded, boundary);

    return boundary;
}

// 新增：擴張邊界線 n 個像素
Mat dilatedBoundary(const Mat& boundary, int dilationPixels) {
    if (dilationPixels == 0) {
        return boundary.clone();
    }

    Mat dilated;
    int kernelSize = 2 * dilationPixels + 1;
    Mat kernel = getStructuringElement(MORPH_RECT, Size(kernelSize, kernelSize));
    dilate(boundary, dilated, kernel);

    return dilated;
}

// 新增：計算三角形在邊界線上的像素平均值
vector<float> calculateTriangleBoundaryMean(const Mat& img, const Mat& boundary,
    const Mat& imageMask, const Triangle& triangle) {
    vector<float> mean(3, 0.0f);
    int count = 0;

    float minX = min(min(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float maxX = max(max(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float minY = min(min(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);
    float maxY = max(max(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);

    int startX = max(0, (int)floor(minX));
    int endX = min(img.cols - 1, (int)ceil(maxX));
    int startY = max(0, (int)floor(minY));
    int endY = min(img.rows - 1, (int)ceil(maxY));

    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            // 必須在三角形內、在邊界上、在圖像遮罩內
            if (triangle.contains(Point2f(j, i))) {
                if (!imageMask.empty() && imageMask.at<uchar>(i, j) == 0) {
                    continue;
                }
                if (!boundary.empty() && boundary.at<uchar>(i, j) != 0) {
                    Vec3b pixel = img.at<Vec3b>(i, j);
                    for (int c = 0; c < 3; c++) {
                        mean[c] += pixel[c];
                    }
                    count++;
                }
            }
        }
    }

    if (count > 0) {
        for (int c = 0; c < 3; c++) {
            mean[c] /= count;
        }
    }

    return mean;
}

// 新增：檢查三角形是否與邊界相交
bool triangleIntersectsBoundary(const Mat& boundary, const Triangle& triangle) {
    float minX = min(min(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float maxX = max(max(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float minY = min(min(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);
    float maxY = max(max(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);

    int startX = max(0, (int)floor(minX));
    int endX = min(boundary.cols - 1, (int)ceil(maxX));
    int startY = max(0, (int)floor(minY));
    int endY = min(boundary.rows - 1, (int)ceil(maxY));

    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            if (triangle.contains(Point2f(j, i)) && boundary.at<uchar>(i, j) != 0) {
                return true;
            }
        }
    }
    return false;
}

// 新增：過濾距離過近的點
vector<Point2f> filterClosePoints(const vector<Point2f>& points, float minDistance) {
    if (points.empty()) {
        return points;
    }

    vector<Point2f> filtered;
    filtered.reserve(points.size());

    // 使用貪心算法：保留第一個點，然後只保留與已保留點距離足夠遠的點
    for (const Point2f& point : points) {
        bool tooClose = false;

        // 檢查與所有已保留點的距離
        for (const Point2f& kept : filtered) {
            float dx = point.x - kept.x;
            float dy = point.y - kept.y;
            float distance = sqrt(dx * dx + dy * dy);

            if (distance < minDistance) {
                tooClose = true;
                break;
            }
        }

        // 如果與所有已保留點的距離都足夠遠，則保留此點
        if (!tooClose) {
            filtered.push_back(point);
        }
    }

    return filtered;
}

// 新增：提取遮罩輪廓上的採樣點
vector<Point2f> sampleMaskContour(const Mat& mask, int numSamples) {
    vector<Point2f> contourPoints;

    if (mask.empty()) {
        return contourPoints;
    }

    // 找到遮罩的輪廓
    vector<vector<Point>> contours;
    findContours(mask.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        return contourPoints;
    }

    // 使用最大的輪廓（應該是遮罩的外邊界）
    int maxContourIdx = 0;
    double maxArea = 0;
    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxContourIdx = i;
        }
    }

    const vector<Point>& contour = contours[maxContourIdx];

    if (contour.empty()) {
        return contourPoints;
    }

    // 計算輪廓總長度
    double totalLength = arcLength(contour, true);

    // 根據輪廓長度決定採樣點數量
    if (numSamples <= 0) {
        numSamples = max(20, (int)(totalLength / 60.0));  // 每60像素一個點
    }

    // 均勻採樣輪廓點
    double samplingInterval = totalLength / numSamples;
    double accumulatedLength = 0;
    double nextSampleAt = 0;

    for (int i = 0; i < contour.size(); i++) {
        Point p1 = contour[i];
        Point p2 = contour[(i + 1) % contour.size()];

        double segmentLength = norm(p1 - p2);

        while (accumulatedLength + segmentLength >= nextSampleAt && nextSampleAt <= totalLength) {
            // 在這個線段上插值採樣點
            double t = (nextSampleAt - accumulatedLength) / segmentLength;
            Point2f samplePoint(
                p1.x + t * (p2.x - p1.x),
                p1.y + t * (p2.y - p1.y)
            );
            contourPoints.push_back(samplePoint);
            nextSampleAt += samplingInterval;
        }

        accumulatedLength += segmentLength;
    }

    return contourPoints;
}








// 新增：創建只包含 correspondence 和 non-overlap 特徵點的 Delaunay 三角網格
DelaunayTriangulation createFeatureOnlyTriangulation(
    const vector<ImageCorrespondences>& correspondences,
    const Size& imageSize,
    const vector<vector<Point2f>>& nonOverlapFeatures) {

    DelaunayTriangulation triangulation;

    cout << "\n=== Creating Feature-Only Delaunay Triangulation ===" << endl;

    map<pair<float, float>, PointType> pointTypeMap;

    // 1. 收集 correspondence 點
    vector<Point2f> allPoints;
    int corrCount = 0;

    for (const auto& imgCorr : correspondences) {
        for (const auto& corr : imgCorr.correspondences) {
            if (isValidPoint(corr.x1, corr.y1, imageSize)) {
                allPoints.emplace_back(corr.x1, corr.y1);
                float rx = round(corr.x1 * 10.0f) / 10.0f;
                float ry = round(corr.y1 * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = CORRESPONDENCE_POINT;
                corrCount++;
            }
            if (isValidPoint(corr.x2, corr.y2, imageSize)) {
                allPoints.emplace_back(corr.x2, corr.y2);
                float rx = round(corr.x2 * 10.0f) / 10.0f;
                float ry = round(corr.y2 * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = CORRESPONDENCE_POINT;
                corrCount++;
            }
        }
    }

    // 2. 收集非重疊特徵點
    int nonOverlapCount = 0;
    for (const auto& features : nonOverlapFeatures) {
        for (const auto& pt : features) {
            if (isValidPoint(pt.x, pt.y, imageSize)) {
                allPoints.push_back(pt);
                float rx = round(pt.x * 10.0f) / 10.0f;
                float ry = round(pt.y * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = NON_OVERLAP_POINT;
                nonOverlapCount++;
            }
        }
    }

    cout << "Correspondence points: " << corrCount << endl;
    cout << "Non-overlap feature points: " << nonOverlapCount << endl;
    cout << "Total feature points: " << allPoints.size() << endl;

    // 3. 隨機採樣
    vector<Point2f> sampledPoints;
    if (!allPoints.empty()) {
        random_device rd;
        mt19937 g(rd());
        shuffle(allPoints.begin(), allPoints.end(), g);

        float sampleRatio = 1.0f;
        int sampleSize = max(1, (int)(allPoints.size() * sampleRatio));
        sampledPoints.assign(allPoints.begin(), allPoints.begin() + sampleSize);
    }

    cout << "Points after sampling: " << sampledPoints.size() << endl;

    // 4. 距離過濾
    float minPointDistance = 13.0f;
    vector<Point2f> filteredPoints = filterClosePoints(sampledPoints, minPointDistance);

    cout << "Points after distance filtering: " << filteredPoints.size() << endl;

    // 5. 去重並保存點類型
    set<pair<float, float>> uniquePointSet;
    for (const Point2f& point : filteredPoints) {
        float roundedX = round(point.x * 10.0f) / 10.0f;
        float roundedY = round(point.y * 10.0f) / 10.0f;
        uniquePointSet.insert({ roundedX, roundedY });
    }

    triangulation.points.clear();
    triangulation.pointTypes.clear();

    for (const auto& point : uniquePointSet) {
        triangulation.points.emplace_back(point.first, point.second);
        auto it = pointTypeMap.find(point);
        if (it != pointTypeMap.end()) {
            triangulation.pointTypes.push_back(it->second);
        }
        else {
            triangulation.pointTypes.push_back(CORRESPONDENCE_POINT);
        }
    }

    // 統計
    int corrFinal = 0, nonOverlapFinal = 0;
    for (PointType type : triangulation.pointTypes) {
        if (type == CORRESPONDENCE_POINT) corrFinal++;
        else if (type == NON_OVERLAP_POINT) nonOverlapFinal++;
    }

    cout << "\nFinal distribution:" << endl;
    cout << "  Correspondence points: " << corrFinal << endl;
    cout << "  Non-overlap points: " << nonOverlapFinal << endl;
    cout << "  Total points: " << triangulation.points.size() << endl;

    if (triangulation.points.size() < 3) {
        cout << "Error: Not enough points for triangulation" << endl;
        return triangulation;
    }

    // 執行三角化
    Rect2f rect(-10, -10, imageSize.width + 20, imageSize.height + 20);
    triangulation.bounds = Rect2f(0, 0, imageSize.width, imageSize.height);

    try {
        Subdiv2D subdiv(rect);
        for (const Point2f& point : triangulation.points) {
            subdiv.insert(point);
        }

        vector<Vec6f> triangleList;
        subdiv.getTriangleList(triangleList);

        for (const Vec6f& t : triangleList) {
            Triangle triangle;
            triangle.vertices[0] = Point2f(t[0], t[1]);
            triangle.vertices[1] = Point2f(t[2], t[3]);
            triangle.vertices[2] = Point2f(t[4], t[5]);

            if (isTriangleInsideImage(triangle, imageSize) && triangle.area() > 10.0f) {
                triangulation.triangles.push_back(triangle);
            }
        }

        cout << "Valid triangles: " << triangulation.triangles.size() << endl;

        if (!triangulation.triangles.empty()) {
            float minArea = FLT_MAX, maxArea = 0.0f, totalArea = 0.0f;
            for (const Triangle& tri : triangulation.triangles) {
                float area = tri.area();
                minArea = min(minArea, area);
                maxArea = max(maxArea, area);
                totalArea += area;
            }
            float avgArea = totalArea / triangulation.triangles.size();

            cout << "Triangle statistics:" << endl;
            cout << "  Min area: " << minArea << " px^2" << endl;
            cout << "  Max area: " << maxArea << " px^2" << endl;
            cout << "  Avg area: " << avgArea << " px^2" << endl;
        }

        if (!triangulation.triangles.empty()) {
            findTriangleNeighbors(triangulation);
        }

    }
    catch (const cv::Exception& e) {
        cout << "OpenCV error during triangulation: " << e.what() << endl;
    }

    cout << "=======================================" << endl;

    return triangulation;
}

// 新增：找出三角網格的邊界邊（外圍邊）
vector<Edge> findBoundaryEdges(const DelaunayTriangulation& triangulation) {
    map<Edge, int> edgeCount;

    // 計算每條邊被多少個三角形共享
    for (const Triangle& tri : triangulation.triangles) {
        Edge e1(tri.vertices[0], tri.vertices[1]);
        Edge e2(tri.vertices[1], tri.vertices[2]);
        Edge e3(tri.vertices[2], tri.vertices[0]);

        edgeCount[e1]++;
        edgeCount[e2]++;
        edgeCount[e3]++;
    }

    // 只被一個三角形使用的邊就是邊界邊
    vector<Edge> boundaryEdges;
    for (const auto& pair : edgeCount) {
        if (pair.second == 1) {
            boundaryEdges.push_back(pair.first);
        }
    }

    return boundaryEdges;
}

// 新增：按順序排列邊界邊（形成連續的輪廓）
vector<Point2f> orderBoundaryEdges(const vector<Edge>& boundaryEdges) {
    if (boundaryEdges.empty()) {
        return vector<Point2f>();
    }

    vector<Point2f> orderedPoints;
    vector<bool> used(boundaryEdges.size(), false);

    // 從第一條邊開始
    orderedPoints.push_back(boundaryEdges[0].p1);
    orderedPoints.push_back(boundaryEdges[0].p2);
    used[0] = true;

    Point2f currentPoint = boundaryEdges[0].p2;

    // 查找連接的下一條邊
    while (orderedPoints.size() < boundaryEdges.size() + 1) {
        bool foundNext = false;

        for (int i = 0; i < boundaryEdges.size(); i++) {
            if (used[i]) continue;

            const Edge& edge = boundaryEdges[i];
            float dist1 = norm(currentPoint - edge.p1);
            float dist2 = norm(currentPoint - edge.p2);

            if (dist1 < 1e-3) {
                orderedPoints.push_back(edge.p2);
                currentPoint = edge.p2;
                used[i] = true;
                foundNext = true;
                break;
            }
            else if (dist2 < 1e-3) {
                orderedPoints.push_back(edge.p1);
                currentPoint = edge.p1;
                used[i] = true;
                foundNext = true;
                break;
            }
        }

        if (!foundNext) break;
    }

    return orderedPoints;
}

// 新增：提取最大的封閉輪廓
vector<Edge> extractLargestClosedContour(const vector<Edge>& edges) {
    if (edges.empty()) return vector<Edge>();

    cout << "Extracting largest closed contour from " << edges.size() << " edges..." << endl;

    // 建立鄰接表
    map<pair<float, float>, vector<pair<int, pair<float, float>>>> adjacency;

    for (int i = 0; i < edges.size(); i++) {
        const Edge& edge = edges[i];
        pair<float, float> p1(round(edge.p1.x * 10) / 10, round(edge.p1.y * 10) / 10);
        pair<float, float> p2(round(edge.p2.x * 10) / 10, round(edge.p2.y * 10) / 10);

        adjacency[p1].push_back({ i, p2 });
        adjacency[p2].push_back({ i, p1 });
    }

    // 尋找所有的環（使用 DFS）
    vector<vector<int>> cycles;
    set<int> visitedEdges;

    for (int startEdgeIdx = 0; startEdgeIdx < edges.size(); startEdgeIdx++) {
        if (visitedEdges.find(startEdgeIdx) != visitedEdges.end()) continue;

        // 嘗試從這條邊開始找環
        vector<int> currentCycle;
        set<int> currentVisited;

        pair<float, float> start(round(edges[startEdgeIdx].p1.x * 10) / 10,
            round(edges[startEdgeIdx].p1.y * 10) / 10);
        pair<float, float> current = start;
        pair<float, float> next(round(edges[startEdgeIdx].p2.x * 10) / 10,
            round(edges[startEdgeIdx].p2.y * 10) / 10);

        currentCycle.push_back(startEdgeIdx);
        currentVisited.insert(startEdgeIdx);
        current = next;

        // 沿著鏈走，直到回到起點或無路可走
        bool foundCycle = false;
        int maxSteps = edges.size() + 10;
        int steps = 0;

        while (steps < maxSteps) {
            steps++;

            // 檢查是否回到起點
            if (abs(current.first - start.first) < 1e-3 &&
                abs(current.second - start.second) < 1e-3) {
                foundCycle = true;
                break;
            }

            // 找下一條邊
            bool foundNext = false;
            for (const auto& neighbor : adjacency[current]) {
                int edgeIdx = neighbor.first;
                pair<float, float> nextPoint = neighbor.second;

                if (currentVisited.find(edgeIdx) == currentVisited.end()) {
                    currentCycle.push_back(edgeIdx);
                    currentVisited.insert(edgeIdx);
                    current = nextPoint;
                    foundNext = true;
                    break;
                }
            }

            if (!foundNext) break;
        }

        if (foundCycle && currentCycle.size() >= 3) {
            cycles.push_back(currentCycle);
            for (int idx : currentCycle) {
                visitedEdges.insert(idx);
            }
        }
    }

    cout << "Found " << cycles.size() << " closed contours" << endl;

    if (cycles.empty()) {
        cout << "No closed contour found, returning all edges" << endl;
        return edges;
    }

    // 找最大的環
    int maxCycleIdx = 0;
    int maxCycleSize = 0;
    for (int i = 0; i < cycles.size(); i++) {
        cout << "  Contour " << i << ": " << cycles[i].size() << " edges" << endl;
        if (cycles[i].size() > maxCycleSize) {
            maxCycleSize = cycles[i].size();
            maxCycleIdx = i;
        }
    }

    cout << "Selected largest contour: " << maxCycleIdx << " with " << maxCycleSize << " edges" << endl;

    // 返回最大環的邊
    vector<Edge> result;
    for (int idx : cycles[maxCycleIdx]) {
        result.push_back(edges[idx]);
    }

    return result;
}

// 改進版：使用輪廓面積來選擇最外層邊界
vector<Edge> filterTrueBoundaryEdges(const DelaunayTriangulation& triangulation,
    const Size& imageSize) {
    cout << "\n=== Filtering True Boundary Edges (Using Contour Area) ===" << endl;

    // 步驟 1: 找出所有候選邊界邊
    map<Edge, int> edgeCount;
    for (const Triangle& tri : triangulation.triangles) {
        Edge e1(tri.vertices[0], tri.vertices[1]);
        Edge e2(tri.vertices[1], tri.vertices[2]);
        Edge e3(tri.vertices[2], tri.vertices[0]);
        edgeCount[e1]++;
        edgeCount[e2]++;
        edgeCount[e3]++;
    }

    vector<Edge> candidateBoundaryEdges;
    for (const auto& pair : edgeCount) {
        if (pair.second == 1) {
            candidateBoundaryEdges.push_back(pair.first);
        }
    }

    cout << "Step 1 - Candidate boundary edges: " << candidateBoundaryEdges.size() << endl;

    if (candidateBoundaryEdges.empty()) {
        return vector<Edge>();
    }

    // 步驟 2: 建立邊的鄰接關係
    map<pair<float, float>, vector<pair<int, pair<float, float>>>> adjacency;

    for (int i = 0; i < candidateBoundaryEdges.size(); i++) {
        const Edge& edge = candidateBoundaryEdges[i];
        pair<float, float> p1(round(edge.p1.x * 10) / 10, round(edge.p1.y * 10) / 10);
        pair<float, float> p2(round(edge.p2.x * 10) / 10, round(edge.p2.y * 10) / 10);

        adjacency[p1].push_back({ i, p2 });
        adjacency[p2].push_back({ i, p1 });
    }

    // 步驟 3: 找出所有封閉輪廓
    set<int> visitedEdges;
    vector<vector<Point2f>> allContours;
    vector<vector<int>> allContourEdgeIndices;

    cout << "Step 2 - Finding all closed contours..." << endl;

    for (int startEdgeIdx = 0; startEdgeIdx < candidateBoundaryEdges.size(); startEdgeIdx++) {
        if (visitedEdges.find(startEdgeIdx) != visitedEdges.end()) continue;

        // 從這條邊開始追蹤輪廓
        vector<Point2f> contourPoints;
        vector<int> contourEdgeIndices;
        set<int> currentVisited;

        pair<float, float> start(round(candidateBoundaryEdges[startEdgeIdx].p1.x * 10) / 10,
            round(candidateBoundaryEdges[startEdgeIdx].p1.y * 10) / 10);
        pair<float, float> current(round(candidateBoundaryEdges[startEdgeIdx].p2.x * 10) / 10,
            round(candidateBoundaryEdges[startEdgeIdx].p2.y * 10) / 10);

        contourPoints.push_back(Point2f(start.first, start.second));
        contourPoints.push_back(Point2f(current.first, current.second));
        contourEdgeIndices.push_back(startEdgeIdx);
        currentVisited.insert(startEdgeIdx);

        // 追蹤輪廓直到回到起點
        bool foundCycle = false;
        int maxSteps = candidateBoundaryEdges.size() + 10;
        int steps = 0;

        while (steps < maxSteps) {
            steps++;

            // 檢查是否回到起點
            if (abs(current.first - start.first) < 1e-3 &&
                abs(current.second - start.second) < 1e-3 &&
                contourPoints.size() > 2) {
                foundCycle = true;
                break;
            }

            // 找下一條邊
            bool foundNext = false;
            for (const auto& neighbor : adjacency[current]) {
                int edgeIdx = neighbor.first;
                pair<float, float> nextPoint = neighbor.second;

                if (currentVisited.find(edgeIdx) == currentVisited.end()) {
                    contourPoints.push_back(Point2f(nextPoint.first, nextPoint.second));
                    contourEdgeIndices.push_back(edgeIdx);
                    currentVisited.insert(edgeIdx);
                    current = nextPoint;
                    foundNext = true;
                    break;
                }
            }

            if (!foundNext) break;
        }

        // 如果找到封閉輪廓
        if (foundCycle && contourPoints.size() >= 3) {
            allContours.push_back(contourPoints);
            allContourEdgeIndices.push_back(contourEdgeIndices);

            // 標記這些邊為已訪問
            for (int idx : contourEdgeIndices) {
                visitedEdges.insert(idx);
            }
        }
    }

    cout << "Found " << allContours.size() << " closed contours" << endl;

    if (allContours.empty()) {
        cout << "No closed contour found!" << endl;
        cout << "Falling back to convex hull method..." << endl;

        // 備用方案：使用凸包
        vector<Point2f> allPoints = triangulation.points;
        vector<Point2f> hull;
        convexHull(allPoints, hull);

        set<pair<float, float>> hullVertexSet;
        for (const Point2f& p : hull) {
            pair<float, float> rounded(round(p.x * 10) / 10, round(p.y * 10) / 10);
            hullVertexSet.insert(rounded);
        }

        vector<Edge> hullEdges;
        for (const Edge& edge : candidateBoundaryEdges) {
            pair<float, float> p1(round(edge.p1.x * 10) / 10, round(edge.p1.y * 10) / 10);
            pair<float, float> p2(round(edge.p2.x * 10) / 10, round(edge.p2.y * 10) / 10);

            if (hullVertexSet.find(p1) != hullVertexSet.end() &&
                hullVertexSet.find(p2) != hullVertexSet.end()) {
                hullEdges.push_back(edge);
            }
        }

        return hullEdges;
    }

    // 步驟 4: 計算每個輪廓的面積
    cout << "Step 3 - Computing contour areas..." << endl;

    vector<double> contourAreas;
    int maxAreaIdx = -1;
    double maxArea = 0.0;

    for (int i = 0; i < allContours.size(); i++) {
        double area = contourArea(allContours[i]);
        contourAreas.push_back(area);

        /*cout << "  Contour " << i << ": "
            << allContours[i].size() << " points, "
            << allContourEdgeIndices[i].size() << " edges, "
            << "area = " << (int)area << " px^2" << endl;*/

        if (area > maxArea) {
            maxArea = area;
            maxAreaIdx = i;
        }
    }

    // 步驟 5: 選擇面積最大的輪廓
    if (maxAreaIdx >= 0) {
        cout << "Step 4 - Selected contour " << maxAreaIdx
            << " with largest area: " << (int)maxArea << " px^2" << endl;

        vector<Edge> selectedEdges;
        for (int idx : allContourEdgeIndices[maxAreaIdx]) {
            selectedEdges.push_back(candidateBoundaryEdges[idx]);
        }

        cout << "Final boundary edges: " << selectedEdges.size() << endl;
        cout << "=====================================" << endl;

        return selectedEdges;
    }

    cout << "Error: Could not select valid contour" << endl;
    return candidateBoundaryEdges;
}

void visualizeFeatureOnlyTriangulation(const DelaunayTriangulation& triangulation,
    const Size& imageSize,
    const string& outputPath,
    const string& datasetName) {

    // 定義顏色
    Scalar triangleColor(0, 255, 0);           // 綠色：三角形邊
    Scalar boundaryColor(0, 255, 255);         // 黃色：邊界邊（突出顯示）
    Scalar corrPointColor(0, 0, 255);          // 紅色：correspondence points
    Scalar nonOverlapPointColor(255, 165, 0);  // 橙色：non-overlap feature points
    Scalar minTriangleColor(0, 255, 255);      // 黃色：最小三角形
    Scalar maxTriangleColor(255, 0, 255);      // 紫色：最大三角形

    cout << "\n=== Creating Feature-Only Triangulation Visualization ===" << endl;

    // 使用輪廓面積過濾後的邊界邊
    vector<Edge> boundaryEdges = filterTrueBoundaryEdges(triangulation, imageSize);

    // 排序邊界邊形成輪廓
    vector<Point2f> boundaryContour = orderBoundaryEdges(boundaryEdges);
    cout << "Ordered boundary contour with " << boundaryContour.size() << " points" << endl;

    // 計算並顯示最終輪廓的面積
    if (!boundaryContour.empty()) {
        double finalArea = contourArea(boundaryContour);
        cout << "Final boundary contour area: " << (int)finalArea << " px^2" << endl;
    }

    // 計算三角形面積統計
    float minArea = FLT_MAX;
    float maxArea = 0.0f;
    float totalArea = 0.0f;
    int minTriangleIdx = -1;
    int maxTriangleIdx = -1;

    for (int i = 0; i < triangulation.triangles.size(); i++) {
        float area = triangulation.triangles[i].area();
        totalArea += area;
        if (area < minArea) {
            minArea = area;
            minTriangleIdx = i;
        }
        if (area > maxArea) {
            maxArea = area;
            maxTriangleIdx = i;
        }
    }

    float avgArea = triangulation.triangles.empty() ? 0.0f :
        totalArea / triangulation.triangles.size();

    // 統計點的類型
    int corrCount = 0, nonOverlapCount = 0;
    for (int i = 0; i < triangulation.points.size(); i++) {
        PointType type = triangulation.pointTypes[i];
        if (type == CORRESPONDENCE_POINT) {
            corrCount++;
        }
        else {
            nonOverlapCount++;
        }
    }

    // ========== 第一張圖：不含 boundary edges 的純 Delaunay 三角網格 ==========
    {
        Mat visualization = Mat::zeros(imageSize, CV_8UC3);

        cout << "\nGenerating visualization WITHOUT boundary edges..." << endl;

        // 繪製所有三角形邊
        for (int i = 0; i < triangulation.triangles.size(); i++) {
            const Triangle& triangle = triangulation.triangles[i];
            Scalar edgeColor = triangleColor;
            int thickness = 1;

            if (i == minTriangleIdx) {
                edgeColor = minTriangleColor;
                thickness = 2;
            }
            else if (i == maxTriangleIdx) {
                edgeColor = maxTriangleColor;
                thickness = 2;
            }

            line(visualization, triangle.vertices[0], triangle.vertices[1], edgeColor, thickness);
            line(visualization, triangle.vertices[1], triangle.vertices[2], edgeColor, thickness);
            line(visualization, triangle.vertices[2], triangle.vertices[0], edgeColor, thickness);
        }

        // 繪製點
        for (int i = 0; i < triangulation.points.size(); i++) {
            const Point2f& point = triangulation.points[i];
            PointType type = triangulation.pointTypes[i];

            Scalar color;
            int radius = 2;

            if (type == CORRESPONDENCE_POINT) {
                color = corrPointColor;
            }
            else {
                color = nonOverlapPointColor;
            }

            circle(visualization, point, radius, color, -1);
        }

        // 添加文字信息
        int yPos = 30;
        int lineHeight = 25;

        putText(visualization, "Dataset: " + datasetName + " (Features Only - No Boundary)",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
        yPos += lineHeight;

        putText(visualization, "Triangles: " + to_string(triangulation.triangles.size()),
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
        yPos += lineHeight;

        putText(visualization, "Total Points: " + to_string(triangulation.points.size()),
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
        yPos += lineHeight;

        putText(visualization, "Min Area: " + to_string((int)minArea) + " px^2",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, minTriangleColor, 2);
        yPos += lineHeight;

        putText(visualization, "Max Area: " + to_string((int)maxArea) + " px^2",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, maxTriangleColor, 2);
        yPos += lineHeight;

        putText(visualization, "Avg Area: " + to_string((int)avgArea) + " px^2",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);

        // 添加圖例
        int legendY = imageSize.height - 20;
        int legendSpacing = 25;

        circle(visualization, Point(20, legendY), 3, corrPointColor, -1);
        putText(visualization, "Correspondence Points (" + to_string(corrCount) + ")",
            Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, corrPointColor, 1);
        legendY -= legendSpacing;

        circle(visualization, Point(20, legendY), 3, nonOverlapPointColor, -1);
        putText(visualization, "Non-Overlap Features (" + to_string(nonOverlapCount) + ")",
            Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, nonOverlapPointColor, 1);
        legendY -= legendSpacing;

        putText(visualization, "Green: All Triangle Edges",
            Point(10, legendY), FONT_HERSHEY_SIMPLEX, 0.5, triangleColor, 1);

        // 保存第一張圖（不含邊界）
        string filename1 = outputPath + datasetName + "_delaunay_features_only_no_boundary.png";
        bool success1 = imwrite(filename1, visualization);

        if (success1) {
            cout << "✓ Visualization WITHOUT boundary saved: " << filename1 << endl;
        }
        else {
            cout << "✗ Failed to save: " << filename1 << endl;
        }
    }

    // ========== 第二張圖：含 boundary edges 的可視化 ==========
    {
        Mat visualization = Mat::zeros(imageSize, CV_8UC3);

        cout << "\nGenerating visualization WITH boundary edges..." << endl;

        // 繪製所有三角形邊（先畫，作為背景）
        for (int i = 0; i < triangulation.triangles.size(); i++) {
            const Triangle& triangle = triangulation.triangles[i];
            Scalar edgeColor = triangleColor;
            int thickness = 1;

            if (i == minTriangleIdx) {
                edgeColor = minTriangleColor;
                thickness = 2;
            }
            else if (i == maxTriangleIdx) {
                edgeColor = maxTriangleColor;
                thickness = 2;
            }

            line(visualization, triangle.vertices[0], triangle.vertices[1], edgeColor, thickness);
            line(visualization, triangle.vertices[1], triangle.vertices[2], edgeColor, thickness);
            line(visualization, triangle.vertices[2], triangle.vertices[0], edgeColor, thickness);
        }

        // 繪製過濾後的邊界邊（後畫，覆蓋在上面，使用更粗的線）
        for (const Edge& edge : boundaryEdges) {
            line(visualization, edge.p1, edge.p2, boundaryColor, 3);
        }

        // 繪製點
        for (int i = 0; i < triangulation.points.size(); i++) {
            const Point2f& point = triangulation.points[i];
            PointType type = triangulation.pointTypes[i];

            Scalar color;
            int radius = 2;

            if (type == CORRESPONDENCE_POINT) {
                color = corrPointColor;
            }
            else {
                color = nonOverlapPointColor;
            }

            circle(visualization, point, radius, color, -1);
        }

        // 添加文字信息
        int yPos = 30;
        int lineHeight = 25;

        putText(visualization, "Dataset: " + datasetName + " (Features Only)",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
        yPos += lineHeight;

        putText(visualization, "Triangles: " + to_string(triangulation.triangles.size()),
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
        yPos += lineHeight;

        putText(visualization, "Boundary Edges: " + to_string(boundaryEdges.size()),
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, boundaryColor, 2);
        yPos += lineHeight;

        putText(visualization, "Total Points: " + to_string(triangulation.points.size()),
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
        yPos += lineHeight;

        putText(visualization, "Min Area: " + to_string((int)minArea) + " px^2",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, minTriangleColor, 2);
        yPos += lineHeight;

        putText(visualization, "Max Area: " + to_string((int)maxArea) + " px^2",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, maxTriangleColor, 2);
        yPos += lineHeight;

        putText(visualization, "Avg Area: " + to_string((int)avgArea) + " px^2",
            Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);

        // 添加圖例
        int legendY = imageSize.height - 20;
        int legendSpacing = 25;

        // 邊界邊圖例
        line(visualization, Point(10, legendY), Point(25, legendY), boundaryColor, 3);
        putText(visualization, "Filtered Boundary Edges (" + to_string(boundaryEdges.size()) + ")",
            Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, boundaryColor, 1);
        legendY -= legendSpacing;

        circle(visualization, Point(20, legendY), 3, corrPointColor, -1);
        putText(visualization, "Correspondence Points (" + to_string(corrCount) + ")",
            Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, corrPointColor, 1);
        legendY -= legendSpacing;

        circle(visualization, Point(20, legendY), 3, nonOverlapPointColor, -1);
        putText(visualization, "Non-Overlap Features (" + to_string(nonOverlapCount) + ")",
            Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, nonOverlapPointColor, 1);
        legendY -= legendSpacing;

        putText(visualization, "Green: Interior Edges",
            Point(10, legendY), FONT_HERSHEY_SIMPLEX, 0.5, triangleColor, 1);

        // 保存第二張圖（含邊界）
        string filename2 = outputPath + datasetName + "_delaunay_features_only.png";
        bool success2 = imwrite(filename2, visualization);

        if (success2) {
            cout << "✓ Visualization WITH boundary saved: " << filename2 << endl;
        }
        else {
            cout << "✗ Failed to save: " << filename2 << endl;
        }
    }

    // 總結
    cout << "\n=== Visualization Summary ===" << endl;
    cout << "  Point distribution: Corr=" << corrCount
        << ", NonOverlap=" << nonOverlapCount << endl;
    cout << "  Triangle statistics: Min=" << (int)minArea
        << ", Max=" << (int)maxArea
        << ", Avg=" << (int)avgArea << " px^2" << endl;
    cout << "  Boundary edges: " << boundaryEdges.size() << endl;
    cout << "  Generated 2 visualizations:" << endl;
    cout << "    1. " << datasetName << "_delaunay_features_only_no_boundary.png" << endl;
    cout << "    2. " << datasetName << "_delaunay_features_only.png" << endl;
    cout << "=========================================================" << endl;
}

// 新增：創建合併的大遮罩
Mat createCombinedMask(const vector<Mat>& imageMasks) {
    if (imageMasks.empty()) {
        cout << "Error: No masks to combine" << endl;
        return Mat();
    }

    // 初始化為全黑遮罩
    Mat combinedMask = Mat::zeros(imageMasks[0].size(), CV_8UC1);

    cout << "\n=== Creating Combined Mask ===" << endl;
    cout << "Combining " << imageMasks.size() << " masks..." << endl;

    // 將所有遮罩進行 OR 操作
    for (int i = 0; i < imageMasks.size(); i++) {
        if (!imageMasks[i].empty()) {
            bitwise_or(combinedMask, imageMasks[i], combinedMask);
            int nonZeroPixels = countNonZero(imageMasks[i]);
            cout << "  Mask " << i << ": " << nonZeroPixels << " non-zero pixels" << endl;
        }
    }

    int totalNonZero = countNonZero(combinedMask);
    cout << "Combined mask total non-zero pixels: " << totalNonZero << endl;
    cout << "===============================" << endl;

    return combinedMask;
}

// 改進版：基於邊界邊法線生成輪廓點（法線垂直於邊界邊）
vector<Point2f> generateBoundaryNormalContourPoints(
    const vector<Edge>& boundaryEdges,
    const Mat& combinedMask,
    const Size& imageSize) {

    cout << "\n=== Generating Boundary Normal Contour Points (Corrected) ===" << endl;
    cout << "Processing " << boundaryEdges.size() << " boundary edges..." << endl;

    if (boundaryEdges.empty() || combinedMask.empty()) {
        cout << "Error: Empty boundary edges or combined mask" << endl;
        return vector<Point2f>();
    }

    vector<Point2f> contourPoints;
    int validPoints = 0;
    int invalidPoints = 0;

    // 先找到遮罩的所有邊界點
    vector<Point2f> maskBoundaryPoints;
    for (int y = 0; y < combinedMask.rows; y++) {
        for (int x = 0; x < combinedMask.cols; x++) {
            if (combinedMask.at<uchar>(y, x) != 0) {
                // 檢查是否為邊界點
                bool isBoundary = false;
                for (int dy = -1; dy <= 1 && !isBoundary; dy++) {
                    for (int dx = -1; dx <= 1 && !isBoundary; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < combinedMask.cols &&
                            ny >= 0 && ny < combinedMask.rows) {
                            if (combinedMask.at<uchar>(ny, nx) == 0) {
                                isBoundary = true;
                            }
                        }
                        else {
                            isBoundary = true; // 圖像邊緣也算邊界
                        }
                    }
                }
                if (isBoundary) {
                    maskBoundaryPoints.emplace_back(x, y);
                }
            }
        }
    }

    cout << "Found " << maskBoundaryPoints.size() << " mask boundary pixels" << endl;

    for (int i = 0; i < boundaryEdges.size(); i++) {
        const Edge& edge = boundaryEdges[i];

        // 計算邊的中點
        Point2f midpoint(
            (edge.p1.x + edge.p2.x) / 2.0f,
            (edge.p1.y + edge.p2.y) / 2.0f
        );

        // 計算邊的方向向量（從p1到p2）
        Point2f edgeVector(edge.p2.x - edge.p1.x, edge.p2.y - edge.p1.y);
        float edgeLength = sqrt(edgeVector.x * edgeVector.x + edgeVector.y * edgeVector.y);

        if (edgeLength < 1e-6) {
            cout << "Warning: Edge " << i << " has zero length, skipping" << endl;
            invalidPoints++;
            contourPoints.push_back(midpoint);
            continue;
        }

        // 正規化邊向量
        edgeVector.x /= edgeLength;
        edgeVector.y /= edgeLength;

        // 在遮罩邊界點中尋找滿足以下條件的點：
        // 1. 從中點到該點的向量與邊界邊垂直（點積接近0）
        // 2. 距離最近

        vector<pair<Point2f, float>> candidatePoints; // (點, 距離)

        for (const Point2f& boundaryPt : maskBoundaryPoints) {
            // 計算從中點到邊界點的向量
            Point2f toMaskBoundary(boundaryPt.x - midpoint.x, boundaryPt.y - midpoint.y);
            float distance = sqrt(toMaskBoundary.x * toMaskBoundary.x +
                toMaskBoundary.y * toMaskBoundary.y);

            if (distance < 1e-3) continue; // 跳過太近的點（可能就是中點本身）

            // 正規化向量
            Point2f toMaskBoundaryNorm(toMaskBoundary.x / distance,
                toMaskBoundary.y / distance);

            // 計算與邊向量的點積（檢查是否垂直）
            float dotProduct = toMaskBoundaryNorm.x * edgeVector.x +
                toMaskBoundaryNorm.y * edgeVector.y;

            // 垂直條件：點積的絕對值應該接近0
            // 使用較寬鬆的閾值允許一定的角度偏差（例如 ±15度）
            float perpendicularThreshold = 0.15f; // sin(15度) ≈ 0.26

            if (abs(dotProduct) < perpendicularThreshold) {
                candidatePoints.push_back(make_pair(boundaryPt, distance));
            }
        }

        // 如果沒找到符合垂直條件的候選點，使用光線投射作為備用
        if (candidatePoints.empty()) {
            cout << "Warning: Edge " << i << " - no perpendicular candidate found, using ray casting..." << endl;

            // 計算兩個垂直方向（法線方向）
            Point2f normal1(-edgeVector.y, edgeVector.x);   // 垂直方向1
            Point2f normal2(edgeVector.y, -edgeVector.x);   // 垂直方向2

            float maxDistance = max(imageSize.width, imageSize.height) * 1.5f;
            float step = 0.5f;

            for (int dir = 0; dir < 2; dir++) {
                Point2f direction = (dir == 0) ? normal1 : normal2;

                bool insideMaskLast = false;
                int mx = (int)round(midpoint.x);
                int my = (int)round(midpoint.y);
                if (mx >= 0 && mx < combinedMask.cols && my >= 0 && my < combinedMask.rows) {
                    insideMaskLast = (combinedMask.at<uchar>(my, mx) != 0);
                }

                for (float t = step; t < maxDistance; t += step) {
                    Point2f currentPoint(
                        midpoint.x + direction.x * t,
                        midpoint.y + direction.y * t
                    );

                    if (currentPoint.x < 0 || currentPoint.x >= imageSize.width ||
                        currentPoint.y < 0 || currentPoint.y >= imageSize.height) {
                        break;
                    }

                    int cx = (int)round(currentPoint.x);
                    int cy = (int)round(currentPoint.y);
                    bool insideMask = (combinedMask.at<uchar>(cy, cx) != 0);

                    if (insideMaskLast != insideMask) {
                        float dist = sqrt((currentPoint.x - midpoint.x) * (currentPoint.x - midpoint.x) +
                            (currentPoint.y - midpoint.y) * (currentPoint.y - midpoint.y));
                        candidatePoints.push_back(make_pair(currentPoint, dist));
                        break;
                    }

                    insideMaskLast = insideMask;
                }
            }
        }

        // 從所有候選點中選擇距離最近的
        if (!candidatePoints.empty()) {
            // 按距離排序
            sort(candidatePoints.begin(), candidatePoints.end(),
                [](const pair<Point2f, float>& a, const pair<Point2f, float>& b) {
                    return a.second < b.second;
                });

            Point2f closestPoint = candidatePoints[0].first;
            float minDistance = candidatePoints[0].second;

            contourPoints.push_back(closestPoint);
            validPoints++;

            // 驗證垂直性（用於調試）
            Point2f toClosest(closestPoint.x - midpoint.x, closestPoint.y - midpoint.y);
            float distCheck = sqrt(toClosest.x * toClosest.x + toClosest.y * toClosest.y);
            if (distCheck > 1e-3) {
                toClosest.x /= distCheck;
                toClosest.y /= distCheck;
                float dotCheck = abs(toClosest.x * edgeVector.x + toClosest.y * edgeVector.y);

                if (i < 5 || i % 100 == 0) {
                    cout << "  Edge " << i << ": midpoint(" << (int)midpoint.x << "," << (int)midpoint.y
                        << ") -> contour(" << (int)closestPoint.x << "," << (int)closestPoint.y
                        << "), dist=" << (int)minDistance
                        << ", perpendicularity=" << dotCheck << " (should be ~0)"
                        << ", candidates=" << candidatePoints.size() << endl;
                }
            }
        }
        else {
            cout << "Warning: Edge " << i << " - no valid intersection found, using midpoint" << endl;
            invalidPoints++;
            contourPoints.push_back(midpoint);
        }
    }

    cout << "\n=== Contour Points Generation Summary ===" << endl;
    cout << "  Valid contour points: " << validPoints << endl;
    cout << "  Invalid/fallback points: " << invalidPoints << endl;
    cout << "  Total contour points: " << contourPoints.size() << endl;
    cout << "=========================================" << endl;

    return contourPoints;
}

// 改進版：更精確地提取遮罩輪廓的轉折點
vector<Point2f> extractMaskContourCorners(const Mat& mask,
    double qualityLevel,
    double minDistance,
    int blockSize,
    bool useHarrisDetector) {

    cout << "\n=== Extracting Mask Contour Corners (Precision Mode) ===" << endl;

    if (mask.empty()) {
        cout << "Error: Empty mask" << endl;
        return vector<Point2f>();
    }

    // ===== 只使用 approxPolyDP 方法（最可靠） =====
    vector<vector<Point>> contours;
    findContours(mask.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);

    if (contours.empty()) {
        cout << "No contours found in mask" << endl;
        return vector<Point2f>();
    }

    // 使用最大的輪廓
    int maxContourIdx = 0;
    double maxArea = 0;
    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxContourIdx = i;
        }
    }

    const vector<Point>& contour = contours[maxContourIdx];
    double perimeter = arcLength(contour, true);
    cout << "Largest contour: " << contour.size() << " points, perimeter = " << perimeter << endl;

    // 使用單一的適中 epsilon 值
    double epsilon = 0.0005 * perimeter;  // 可調整此值：越大越少角點，越小越多角點

    vector<Point> approxContour;
    approxPolyDP(contour, approxContour, epsilon, true);

    cout << "approxPolyDP with epsilon=" << epsilon << " -> "
        << approxContour.size() << " vertices" << endl;

    // ===== 基於角度過濾：只保留真正的轉折點 =====
    vector<Point2f> filteredCorners;
    float minAngle = 20.0f;   // 最小轉角（度）- 小於此值的不算轉折點
    float maxAngle = 160.0f;  // 最大轉角（度）- 大於此值的太平緩，不算轉折點

    cout << "\nAnalyzing corner angles (keeping " << minAngle << "° to " << maxAngle << "°):" << endl;

    for (int i = 0; i < approxContour.size(); i++) {
        Point p1 = approxContour[(i - 1 + approxContour.size()) % approxContour.size()];
        Point p2 = approxContour[i];
        Point p3 = approxContour[(i + 1) % approxContour.size()];

        // 計算兩個向量
        Point2f v1(p1.x - p2.x, p1.y - p2.y);
        Point2f v2(p3.x - p2.x, p3.y - p2.y);

        // 正規化
        float len1 = sqrt(v1.x * v1.x + v1.y * v1.y);
        float len2 = sqrt(v2.x * v2.x + v2.y * v2.y);

        if (len1 > 1e-6 && len2 > 1e-6) {
            v1.x /= len1; v1.y /= len1;
            v2.x /= len2; v2.y /= len2;

            // 計算角度（使用點積）
            float dotProduct = v1.x * v2.x + v1.y * v2.y;
            dotProduct = max(-1.0f, min(1.0f, dotProduct)); // 限制範圍避免數值誤差
            float angle = acos(dotProduct) * 180.0f / CV_PI;

            // 只保留在指定角度範圍內的點
            if (angle >= minAngle && angle <= maxAngle) {
                filteredCorners.emplace_back(p2.x, p2.y);

                cout << "  Corner " << filteredCorners.size()
                    << " at (" << p2.x << ", " << p2.y
                    << "), angle = " << (int)angle << "°" << endl;
            }
            else {
                if (i < 5) { // 顯示前幾個被過濾掉的點
                    cout << "  [Filtered] Point at (" << p2.x << ", " << p2.y
                        << "), angle = " << (int)angle << "° (out of range)" << endl;
                }
            }
        }
    }

    cout << "\n=== Corner Detection Summary ===" << endl;
    cout << "  Initial vertices (approxPolyDP): " << approxContour.size() << endl;
    cout << "  Filtered corners (angle-based): " << filteredCorners.size() << endl;
    cout << "  Angle range: [" << minAngle << "°, " << maxAngle << "°]" << endl;
    cout << "=====================================" << endl;

    return filteredCorners;
}





// 修改後的 createDelaunayFromCorrespondences 函數 - 所有點都參與距離過濾
DelaunayTriangulation createDelaunayFromCorrespondences(
    const vector<ImageCorrespondences>& correspondences,
    const Size& imageSize,
    const vector<Mat>& imageMasks,
    const vector<vector<Point2f>>& nonOverlapFeatures) {

    DelaunayTriangulation triangulation;

    cout << "Creating Delaunay triangulation with unified distance filtering..." << endl;

    map<pair<float, float>, PointType> pointTypeMap;

    // ========== 階段1：收集所有點 ==========
    vector<Point2f> allPoints;  // 改為統一收集所有點

    // 1. 收集 correspondence 點
    int corrCount = 0;
    for (const auto& imgCorr : correspondences) {
        for (const auto& corr : imgCorr.correspondences) {
            if (isValidPoint(corr.x1, corr.y1, imageSize)) {
                allPoints.emplace_back(corr.x1, corr.y1);
                float rx = round(corr.x1 * 10.0f) / 10.0f;
                float ry = round(corr.y1 * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = CORRESPONDENCE_POINT;
                corrCount++;
            }
            if (isValidPoint(corr.x2, corr.y2, imageSize)) {
                allPoints.emplace_back(corr.x2, corr.y2);
                float rx = round(corr.x2 * 10.0f) / 10.0f;
                float ry = round(corr.y2 * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = CORRESPONDENCE_POINT;
                corrCount++;
            }
        }
    }

    // 2. 收集非重疊特徵點
    int nonOverlapCount = 0;
    for (const auto& features : nonOverlapFeatures) {
        for (const auto& pt : features) {
            if (isValidPoint(pt.x, pt.y, imageSize)) {
                allPoints.push_back(pt);
                float rx = round(pt.x * 10.0f) / 10.0f;
                float ry = round(pt.y * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = NON_OVERLAP_POINT;
                nonOverlapCount++;
            }
        }
    }

    // 3. 收集遮罩輪廓點 - 使用新方法：基於邊界邊法線
    int contourCount = 0;

    // 先創建臨時的 feature-only 三角化來獲取邊界邊
    cout << "\nCreating temporary triangulation to extract boundary edges..." << endl;
    DelaunayTriangulation tempTriangulation = createFeatureOnlyTriangulation(
        correspondences, imageSize, nonOverlapFeatures);

    // 提取並過濾邊界邊
    vector<Edge> boundaryEdges = filterTrueBoundaryEdges(tempTriangulation, imageSize);
    cout << "Extracted " << boundaryEdges.size() << " boundary edges" << endl;

    // 創建合併的大遮罩
    Mat combinedMask = createCombinedMask(imageMasks);

    // 基於邊界邊法線生成新的輪廓點
    if (!boundaryEdges.empty() && !combinedMask.empty()) {
        vector<Point2f> boundaryNormalPoints = generateBoundaryNormalContourPoints(
            boundaryEdges, combinedMask, imageSize);

        cout << "Generated " << boundaryNormalPoints.size()
            << " boundary normal contour points" << endl;

        for (const Point2f& pt : boundaryNormalPoints) {
            if (isValidPoint(pt.x, pt.y, imageSize)) {
                allPoints.push_back(pt);
                float rx = round(pt.x * 10.0f) / 10.0f;
                float ry = round(pt.y * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = MASK_CONTOUR_POINT;  // 改為 MASK_CONTOUR_POINT
                contourCount++;
            }
        }
    }
    else {
        cout << "Warning: Cannot generate boundary normal points, using fallback method" << endl;

        // 備用方案：使用原始的 sampleMaskContour
        for (int i = 0; i < imageMasks.size(); i++) {
            if (!imageMasks[i].empty()) {
                vector<Point2f> contourPoints = sampleMaskContour(imageMasks[i], 0);
                for (const Point2f& pt : contourPoints) {
                    allPoints.push_back(pt);
                    float rx = round(pt.x * 10.0f) / 10.0f;
                    float ry = round(pt.y * 10.0f) / 10.0f;
                    pointTypeMap[{rx, ry}] = MASK_CONTOUR_POINT;  // 改為 MASK_CONTOUR_POINT
                    contourCount++;
                }
            }
        }
    }

    // ===== 4. 收集大遮罩的輪廓轉折點（角點） =====
    int cornerCount = 0;
    if (!combinedMask.empty()) {
        cout << "\nExtracting corner points from combined mask..." << endl;

        vector<Point2f> maskCorners = extractMaskContourCorners(
            combinedMask, 0.01, 15.0, 3, false);

        cout << "Extracted " << maskCorners.size() << " corner points from combined mask" << endl;

        for (const Point2f& corner : maskCorners) {
            if (isValidPoint(corner.x, corner.y, imageSize)) {
                allPoints.push_back(corner);
                float rx = round(corner.x * 10.0f) / 10.0f;
                float ry = round(corner.y * 10.0f) / 10.0f;
                pointTypeMap[{rx, ry}] = MASK_CORNER_POINT;  // 保持為 MASK_CORNER_POINT
                cornerCount++;
            }
        }
    }

    cout << "\n=== Point Collection Summary ===" << endl;
    cout << "  Correspondence points: " << corrCount << endl;
    cout << "  Non-overlap feature points: " << nonOverlapCount << endl;
    cout << "  Mask contour points (boundary normal): " << contourCount << endl;
    cout << "  Mask corner points (turning points): " << cornerCount << endl;
    cout << "  Total points before processing: " << allPoints.size() << endl;

    // ========== 階段2：隨機採樣 ==========
    vector<Point2f> sampledPoints;
    if (!allPoints.empty()) {
        random_device rd;
        mt19937 g(rd());
        shuffle(allPoints.begin(), allPoints.end(), g);

        float sampleRatio = 1.0f;
        int sampleSize = max(1, (int)(allPoints.size() * sampleRatio));
        sampledPoints.assign(allPoints.begin(), allPoints.begin() + sampleSize);
    }

    cout << "\nPoints after sampling (10%): " << sampledPoints.size() << endl;

    // ========== 階段3：統一距離過濾（所有點類型） ==========
    float minPointDistance = 13.0f; //13
    vector<Point2f> filteredPoints = filterClosePoints(sampledPoints, minPointDistance);

    cout << "Points after distance filtering (>= " << minPointDistance << " pixels): "
        << filteredPoints.size() << endl;
    cout << "Removed " << (sampledPoints.size() - filteredPoints.size())
        << " points due to distance constraint" << endl;

    // ========== 階段4：去重並保存點類型 ==========
    set<pair<float, float>> uniquePointSet;
    for (const Point2f& point : filteredPoints) {
        float roundedX = round(point.x * 10.0f) / 10.0f;
        float roundedY = round(point.y * 10.0f) / 10.0f;
        uniquePointSet.insert({ roundedX, roundedY });
    }

    triangulation.points.clear();
    triangulation.pointTypes.clear();

    for (const auto& point : uniquePointSet) {
        triangulation.points.emplace_back(point.first, point.second);
        auto it = pointTypeMap.find(point);
        if (it != pointTypeMap.end()) {
            triangulation.pointTypes.push_back(it->second);
        }
        else {
            triangulation.pointTypes.push_back(CORRESPONDENCE_POINT);
        }
    }

    cout << "Unique points after deduplication: " << triangulation.points.size() << endl;

    // ========== 階段5：統計最終點類型分布 ==========
    int corrFinal = 0, nonOverlapFinal = 0, contourFinal = 0, cornerFinal = 0;
    for (PointType type : triangulation.pointTypes) {
        if (type == CORRESPONDENCE_POINT) corrFinal++;
        else if (type == NON_OVERLAP_POINT) nonOverlapFinal++;
        else if (type == MASK_CONTOUR_POINT) contourFinal++;
        else if (type == MASK_CORNER_POINT) cornerFinal++;
    }

    cout << "\n=== Final Point Type Distribution ===" << endl;
    cout << "  Correspondence points: " << corrFinal << endl;
    cout << "  Non-overlap points: " << nonOverlapFinal << endl;
    cout << "  Mask contour points: " << contourFinal << endl;
    cout << "  Mask corner points: " << cornerFinal << endl;
    cout << "  Total points for triangulation: " << triangulation.points.size() << endl;
    cout << "=====================================" << endl;

    if (triangulation.points.size() < 3) {
        cout << "Error: Not enough valid points for triangulation" << endl;
        return triangulation;
    }

    // ========== 階段6：執行三角化 ==========
    Rect2f rect(-10, -10, imageSize.width + 20, imageSize.height + 20);
    triangulation.bounds = Rect2f(0, 0, imageSize.width, imageSize.height);

    try {
        Subdiv2D subdiv(rect);
        for (const Point2f& point : triangulation.points) {
            subdiv.insert(point);
        }

        vector<Vec6f> triangleList;
        subdiv.getTriangleList(triangleList);

        cout << "\nGenerated " << triangleList.size() << " raw triangles" << endl;

        for (const Vec6f& t : triangleList) {
            Triangle triangle;
            triangle.vertices[0] = Point2f(t[0], t[1]);
            triangle.vertices[1] = Point2f(t[2], t[3]);
            triangle.vertices[2] = Point2f(t[4], t[5]);

            if (isTriangleInsideImage(triangle, imageSize) && triangle.area() > 10.0f) {
                triangulation.triangles.push_back(triangle);
            }
        }

        cout << "Valid triangles inside image: " << triangulation.triangles.size() << endl;

        if (!triangulation.triangles.empty()) {
            float minArea = FLT_MAX, maxArea = 0.0f, totalArea = 0.0f;
            for (const Triangle& tri : triangulation.triangles) {
                float area = tri.area();
                minArea = min(minArea, area);
                maxArea = max(maxArea, area);
                totalArea += area;
            }
            float avgArea = totalArea / triangulation.triangles.size();

            cout << "\nTriangle size statistics:" << endl;
            cout << "  Min area: " << minArea << " px^2" << endl;
            cout << "  Max area: " << maxArea << " px^2" << endl;
            cout << "  Avg area: " << avgArea << " px^2" << endl;
        }

        if (!triangulation.triangles.empty()) {
            findTriangleNeighbors(triangulation);
        }

    }
    catch (const cv::Exception& e) {
        cout << "OpenCV error during triangulation: " << e.what() << endl;
    }

    return triangulation;
}

// 添加新的檢查函數
bool isTriangleInsideImage(const Triangle& triangle, const Size& imageSize) {
    for (int i = 0; i < 3; i++) {
        if (triangle.vertices[i].x < 0 || triangle.vertices[i].x >= imageSize.width ||
            triangle.vertices[i].y < 0 || triangle.vertices[i].y >= imageSize.height) {
            return false;
        }
    }
    return true;
}

// 完整的 visualizeDelaunayTriangulation 函數 - 使用橘色實心點標記角點
void visualizeDelaunayTriangulation(const DelaunayTriangulation& triangulation,
    const Size& imageSize,
    const string& outputPath,
    const string& datasetName,
    const vector<Mat>& imageMasks,
    const vector<ImageCorrespondences>& correspondences,
    const vector<vector<Point2f>>& nonOverlapFeatures) {

    Mat visualization = Mat::zeros(imageSize, CV_8UC3);

    // 定義顏色
    Scalar triangleColor(0, 255, 0);           // 綠色：三角形邊
    Scalar corrPointColor(0, 0, 255);          // 紅色：correspondence points
    Scalar nonOverlapPointColor(255, 165, 0);  // 橙色：non-overlap feature points
    Scalar contourPointColor(255, 0, 255);     // 紫紅色：輪廓點（法線生成）
    Scalar cornerPointColor(0, 255, 255);      // 青色：角點（轉折點）
    Scalar maskBoundaryColor(255, 255, 255);   // 白色：合併遮罩邊界
    Scalar maskCornerHighlight(255, 0, 255);   // 紫紅：遮罩角點高亮標記

    cout << "\n=== Creating Triangulation Visualization ===" << endl;

    // 計算三角形面積統計
    float totalArea = 0.0f;
    for (int i = 0; i < triangulation.triangles.size(); i++) {
        float area = triangulation.triangles[i].area();
        totalArea += area;
    }
    float avgArea = triangulation.triangles.empty() ? 0.0f :
        totalArea / triangulation.triangles.size();

    // ========== 生成遮罩角點數據（用於高亮顯示） ==========
    Mat combinedMask = createCombinedMask(imageMasks);
    vector<Point2f> maskCorners = extractMaskContourCorners(combinedMask, 0.01, 15.0, 3, false);
    cout << "Mask corner points for visualization: " << maskCorners.size() << endl;

    // ========== 階段1：繪製合併遮罩邊界 ==========
    if (!combinedMask.empty()) {
        vector<vector<Point>> contours;
        findContours(combinedMask.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        for (const auto& contour : contours) {
            for (int i = 0; i < contour.size(); i++) {
                Point p1 = contour[i];
                Point p2 = contour[(i + 1) % contour.size()];
                //line(visualization, p1, p2, maskBoundaryColor, 1);
            }
        }
    }

    // ========== 階段2：繪製所有三角形邊 ==========
    cout << "Drawing triangle edges..." << endl;
    for (int i = 0; i < triangulation.triangles.size(); i++) {
        const Triangle& triangle = triangulation.triangles[i];

        line(visualization, triangle.vertices[0], triangle.vertices[1], triangleColor, 1);
        line(visualization, triangle.vertices[1], triangle.vertices[2], triangleColor, 1);
        line(visualization, triangle.vertices[2], triangle.vertices[0], triangleColor, 1);
    }

    // ========== 階段3：繪製三角化的頂點（根據類型） ==========
    cout << "Drawing triangulation vertices..." << endl;
    int corrCount = 0, nonOverlapCount = 0, contourCount = 0, cornerCount = 0;

    for (int i = 0; i < triangulation.points.size(); i++) {
        const Point2f& point = triangulation.points[i];
        PointType type = triangulation.pointTypes[i];

        Scalar color;
        int radius;

        switch (type) {
        case CORRESPONDENCE_POINT:
            color = corrPointColor;
            radius = 2;
            corrCount++;
            break;
        case NON_OVERLAP_POINT:
            color = nonOverlapPointColor;
            radius = 2;
            nonOverlapCount++;
            break;
        case MASK_CONTOUR_POINT:
            color = contourPointColor;
            radius = 3;
            contourCount++;
            break;
        case MASK_CORNER_POINT:
            color = cornerPointColor;
            radius = 3;
            cornerCount++;
            break;
        }

        //circle(visualization, point, radius, color, -1);
    }

    // ========== 階段4：繪製遮罩轉折點的高亮標記（橘色實心點） ==========
    for (const Point2f& corner : maskCorners) {
        // 繪製橘色實心圓
        //circle(visualization, corner, 6, maskCornerHighlight, -1);
    }

    // ========== 階段5：添加文字信息 ==========
    /*int yPos = 30;
    int lineHeight = 25;

    putText(visualization, "Dataset: " + datasetName,
        Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
    yPos += lineHeight;

    putText(visualization, "Triangles: " + to_string(triangulation.triangles.size()),
        Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
    yPos += lineHeight;

    putText(visualization, "Total Points: " + to_string(triangulation.points.size()),
        Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
    yPos += lineHeight;

    putText(visualization, "Corner Points: " + to_string(maskCorners.size()),
        Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, maskCornerHighlight, 2);
    yPos += lineHeight;

    putText(visualization, "Avg Area: " + to_string((int)avgArea) + " px^2",
        Point(10, yPos), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);*/

        // ========== 階段6：添加圖例 ==========
        //int legendY = imageSize.height - 20;
        //int legendSpacing = 25;

        //// 圖例：遮罩角點高亮標記
        //circle(visualization, Point(20, legendY), 6, maskCornerHighlight, -1);
        //putText(visualization, "Corner Highlights (" + to_string(maskCorners.size()) + ")",
        //    Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, maskCornerHighlight, 1);
        //legendY -= legendSpacing;

        //// 圖例：角點（在三角化中）
        //circle(visualization, Point(20, legendY), 4, cornerPointColor, -1);
        //putText(visualization, "Corner Points in Triangulation (" + to_string(cornerCount) + ")",
        //    Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, cornerPointColor, 1);
        //legendY -= legendSpacing;

        //// 圖例：輪廓點（在三角化中）
        //circle(visualization, Point(20, legendY), 4, contourPointColor, -1);
        //putText(visualization, "Contour Points in Triangulation (" + to_string(contourCount) + ")",
        //    Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, contourPointColor, 1);
        //legendY -= legendSpacing;

        //// 圖例：correspondence points
        //circle(visualization, Point(20, legendY), 3, corrPointColor, -1);
        //putText(visualization, "Correspondence Points (" + to_string(corrCount) + ")",
        //    Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, corrPointColor, 1);
        //legendY -= legendSpacing;

        //// 圖例：non-overlap points
        //circle(visualization, Point(20, legendY), 3, nonOverlapPointColor, -1);
        //putText(visualization, "Non-Overlap Features (" + to_string(nonOverlapCount) + ")",
        //    Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, nonOverlapPointColor, 1);
        //legendY -= legendSpacing;

        //// 圖例：遮罩邊界
        //line(visualization, Point(15, legendY), Point(25, legendY), maskBoundaryColor, 1);
        //putText(visualization, "Mask Boundary",
        //    Point(30, legendY + 5), FONT_HERSHEY_SIMPLEX, 0.5, maskBoundaryColor, 1);
        //legendY -= legendSpacing;

        //// 圖例：三角形邊
        //putText(visualization, "Green: Triangle Edges",
        //    Point(10, legendY), FONT_HERSHEY_SIMPLEX, 0.5, triangleColor, 1);

        // ========== 階段7：保存圖像 ==========
    string filename = outputPath + datasetName + "_delaunay_triangulation_with_normals.png";
    bool success = imwrite(filename, visualization);

    if (success) {
        cout << "\n✓ Triangulation visualization saved: " << filename << endl;
        cout << "  Point distribution in triangulation:" << endl;
        cout << "    - Correspondence: " << corrCount << endl;
        cout << "    - Non-Overlap: " << nonOverlapCount << endl;
        cout << "    - Contour Points: " << contourCount << endl;
        cout << "    - Corner Points: " << cornerCount << endl;
        cout << "  Corner highlights: " << maskCorners.size() << endl;
    }
    else {
        cout << "✗ Failed to save visualization: " << filename << endl;
    }

    cout << "================================================" << endl;
}

bool isValidPoint(float x, float y, const Size& imageSize) {
    return !isnan(x) && !isnan(y) &&
        !isinf(x) && !isinf(y) &&
        x >= 0 && x < imageSize.width &&
        y >= 0 && y < imageSize.height;
}

bool isValidTriangle(const Triangle& triangle, const Size& imageSize) {
    // 檢查所有頂點是否有效
    for (int i = 0; i < 3; i++) {
        if (!isValidPoint(triangle.vertices[i].x, triangle.vertices[i].y, imageSize)) {
            return false;
        }
    }

    // 檢查面積是否太小
    if (triangle.area() < 10.0f) {
        return false;
    }

    return true;
}

// 修改後的找到三角形鄰居關係 - 共享至少一個頂點即為鄰居
void findTriangleNeighbors(DelaunayTriangulation& triangulation) {
    cout << "Finding triangle neighbors (共點即為鄰居)..." << endl;

    for (int i = 0; i < triangulation.triangles.size(); i++) {
        triangulation.triangles[i].neighbor_triangles.clear();

        for (int j = 0; j < triangulation.triangles.size(); j++) {
            if (i == j) continue;

            // 檢查兩個三角形是否共享至少一個頂點
            int sharedVertices = 0;
            for (int vi = 0; vi < 3; vi++) {
                for (int vj = 0; vj < 3; vj++) {
                    Point2f p1 = triangulation.triangles[i].vertices[vi];
                    Point2f p2 = triangulation.triangles[j].vertices[vj];

                    // 使用較小的容差來判斷是否為同一點
                    if (abs(p1.x - p2.x) < 1e-3 && abs(p1.y - p2.y) < 1e-3) {
                        sharedVertices++;
                        break; // 找到共享點就跳出內層循環
                    }
                }
            }

            // 修改判斷條件：共享至少一個頂點即為鄰居
            if (sharedVertices >= 1) {
                triangulation.triangles[i].neighbor_triangles.push_back(j);
            }
        }
    }

    // 統計鄰居信息
    int totalNeighbors = 0;
    int minNeighbors = INT_MAX;
    int maxNeighbors = 0;

    for (const auto& triangle : triangulation.triangles) {
        int neighborCount = triangle.neighbor_triangles.size();
        totalNeighbors += neighborCount;
        minNeighbors = min(minNeighbors, neighborCount);
        maxNeighbors = max(maxNeighbors, neighborCount);
    }

    float avgNeighbors = triangulation.triangles.empty() ? 0.0f :
        (float)totalNeighbors / triangulation.triangles.size();

    cout << "Triangle neighbor statistics:" << endl;
    cout << "  Average neighbors per triangle: " << avgNeighbors << endl;
    cout << "  Min neighbors: " << minNeighbors << endl;
    cout << "  Max neighbors: " << maxNeighbors << endl;
    cout << "  Total neighbor relationships: " << totalNeighbors << endl;
}

// 修正後的計算三角形內平均值函數 - 只考慮圖像遮罩內的有效像素
vector<float> calculateTriangleMean(const Mat& img, const Mat& overlapMask, const Mat& imageMask, const Triangle& triangle) {
    vector<float> mean(3, 0.0f);
    int count = 0;

    // 找到三角形的邊界矩形
    float minX = min(min(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float maxX = max(max(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float minY = min(min(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);
    float maxY = max(max(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);

    int startX = max(0, (int)floor(minX));
    int endX = min(img.cols - 1, (int)ceil(maxX));
    int startY = max(0, (int)floor(minY));
    int endY = min(img.rows - 1, (int)ceil(maxY));

    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            // 檢查像素是否在三角形內
            if (triangle.contains(Point2f(j, i))) {
                // 優先檢查圖像遮罩，確保像素在有效區域內
                if (!imageMask.empty() && imageMask.at<uchar>(i, j) == 0) {
                    continue; // 跳過遮罩外的像素（黑色背景）
                }

                // 如果有重疊遮罩，也要檢查是否在重疊區域內
                if (!overlapMask.empty() && overlapMask.at<uchar>(i, j) == 0) {
                    continue; // 跳過非重疊區域的像素
                }

                // 像素通過所有遮罩檢查，納入計算
                Vec3b pixel = img.at<Vec3b>(i, j);
                for (int c = 0; c < 3; c++) {
                    mean[c] += pixel[c];
                }
                count++;
            }
        }
    }

    if (count > 0) {
        for (int c = 0; c < 3; c++) {
            mean[c] /= count;
        }
    }
    // 注意：如果 count == 0，mean 保持為 {0, 0, 0}
    // 調用方應該先用 hasValidPixels() 檢查有效性再調用此函數

    return mean;
}

// 檢查三角形是否有有效像素的輔助函數
bool hasValidPixels(const Mat& img, const Mat& imageMask, const Triangle& triangle) {
    float minX = min(min(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float maxX = max(max(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float minY = min(min(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);
    float maxY = max(max(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);

    int startX = max(0, (int)floor(minX));
    int endX = min(img.cols - 1, (int)ceil(maxX));
    int startY = max(0, (int)floor(minY));
    int endY = min(img.rows - 1, (int)ceil(maxY));

    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            if (triangle.contains(Point2f(j, i))) {
                if (!imageMask.empty() && imageMask.at<uchar>(i, j) != 0) {
                    return true; // 找到至少一個有效像素
                }
            }
        }
    }
    return false; // 沒有找到有效像素
}

// 檢查三角形在重疊區域是否有有效像素
bool hasValidPixelsInOverlap(const Mat& img, const Mat& overlapMask, const Mat& imageMask, const Triangle& triangle) {
    if (overlapMask.empty()) return false;

    float minX = min(min(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float maxX = max(max(triangle.vertices[0].x, triangle.vertices[1].x), triangle.vertices[2].x);
    float minY = min(min(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);
    float maxY = max(max(triangle.vertices[0].y, triangle.vertices[1].y), triangle.vertices[2].y);

    int startX = max(0, (int)floor(minX));
    int endX = min(img.cols - 1, (int)ceil(maxX));
    int startY = max(0, (int)floor(minY));
    int endY = min(img.rows - 1, (int)ceil(maxY));

    for (int i = startY; i <= endY; i++) {
        for (int j = startX; j <= endX; j++) {
            if (triangle.contains(Point2f(j, i))) {
                if ((!imageMask.empty() && imageMask.at<uchar>(i, j) != 0) &&
                    overlapMask.at<uchar>(i, j) != 0) {
                    return true; // 找到至少一個在重疊區域內的有效像素
                }
            }
        }
    }
    return false;
}

// 修改後的求解函數 - 排除沒有有效像素的三角形
void solveTriangleCoefficients(vector<Mat>& images, const vector<Mat>& imageMasks,
    const vector<vector<Mat>>& overlaps,
    vector<DelaunayTriangulation>& triangulations) {

    int n = images.size();
    if (triangulations.empty()) {
        cout << "Error: No triangulations available" << endl;
        return;
    }

    int trianglesPerImage = triangulations[0].triangles.size();
    int paramsPerTriangle = 6;
    int totalParams = n * trianglesPerImage * paramsPerTriangle;

    cout << "\n=== Computing Area-Based Weights (Fixed) ===" << endl;

    // ========== 面積影響控制參數 ==========
    // alpha = 0.0: 完全忽略面積，使用固定權重（與原本相同）
    // alpha = 0.5: 使用平方根縮放（推薦）
    // alpha = 1.0: 線性面積縮放
    float area_influence_alpha = 1.0f;  // 建議從 0.0-0.5 之間調整

    cout << "Area influence parameter (alpha): " << area_influence_alpha << endl;
    if (area_influence_alpha < 1e-6) {
        cout << "  Using FIXED weights (no area influence, same as original)" << endl;
    }
    else {
        cout << "  Using AREA-WEIGHTED approach with alpha = " << area_influence_alpha << endl;
    }

    // ========== 預先計算所有三角形的面積和有效性 ==========
    vector<vector<bool>> triangleValidity(n, vector<bool>(trianglesPerImage, false));
    vector<vector<float>> triangleAreas(n, vector<float>(trianglesPerImage, 0.0f));

    int totalValidTriangles = 0;
    int totalInvalidTriangles = 0;
    float minArea = FLT_MAX;
    float maxArea = 0.0f;
    float totalAreaSum = 0.0f;

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            bool isValid = hasValidPixels(images[i], imageMasks[i],
                triangulations[i].triangles[tri_idx]);
            triangleValidity[i][tri_idx] = isValid;

            if (isValid) {
                float area = triangulations[i].triangles[tri_idx].area();
                triangleAreas[i][tri_idx] = area;
                totalAreaSum += area;
                minArea = min(minArea, area);
                maxArea = max(maxArea, area);
                totalValidTriangles++;
            }
            else {
                totalInvalidTriangles++;
            }
        }
    }

    cout << "Triangle validity and area analysis:" << endl;
    cout << "  Total valid triangles: " << totalValidTriangles << endl;
    cout << "  Total invalid triangles: " << totalInvalidTriangles << endl;
    cout << "  Area range: [" << minArea << ", " << maxArea << "] px^2" << endl;
    cout << "  Area ratio (max/min): " << (maxArea / minArea) << "x" << endl;
    cout << "  Average area: " << (totalAreaSum / totalValidTriangles) << " px^2" << endl;

    // ========== 設置基礎權重 ==========
    float overlap_base_weight = 1.0f;
    float original_base_weight = 0.3f;
    float smooth_base_weight = 0.81f; //0.6
    float boundary_base_weight = 0.1f;

    cout << "\n=== Base Weight Settings ===" << endl;
    cout << "  Overlap base weight: " << overlap_base_weight << endl;
    cout << "  Original base weight: " << original_base_weight << endl;
    cout << "  Smooth base weight: " << smooth_base_weight << endl;
    cout << "  Boundary base weight: " << boundary_base_weight << endl;

    // ========== 權重計算函數 ==========
    auto computeTriangleWeight = [area_influence_alpha](
        float area,
        float base_weight,
        float weighted_area_sum,
        int triangle_count) -> float {

            // 如果 alpha 接近 0，使用固定權重
            if (area_influence_alpha < 1e-6) {
                return base_weight;  // 每個三角形使用相同的基礎權重
            }

            // 否則使用面積加權
            if (weighted_area_sum < 1e-6 || triangle_count == 0) {
                return base_weight;
            }

            float weighted_area = pow(area, area_influence_alpha);
            // 注意：這裡不除以 triangle_count，而是除以總加權面積
            // 然後乘以 base_weight × triangle_count，使得總權重和 = base_weight × triangle_count
            return (weighted_area / weighted_area_sum) * base_weight * triangle_count;
        };

    // ========== 計算每種約束類型的統計信息 ==========

    // 1. 重疊約束
    float overlapWeightedAreaSum = 0.0f;
    int overlapTriangleCount = 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!overlaps[i][j].empty()) {
                for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
                    bool validInI = hasValidPixelsInOverlap(images[i], overlaps[i][j],
                        imageMasks[i], triangulations[i].triangles[tri_idx]);
                    bool validInJ = hasValidPixelsInOverlap(images[j], overlaps[i][j],
                        imageMasks[j], triangulations[j].triangles[tri_idx]);
                    if (validInI && validInJ) {
                        float area = triangleAreas[i][tri_idx];
                        if (area_influence_alpha >= 1e-6) {
                            overlapWeightedAreaSum += pow(area, area_influence_alpha);
                        }
                        overlapTriangleCount++;
                    }
                }
            }
        }
    }

    // 2. 原始約束
    float originalWeightedAreaSum = 0.0f;
    int originalTriangleCount = 0;

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            if (triangleValidity[i][tri_idx]) {
                float area = triangleAreas[i][tri_idx];
                if (area_influence_alpha >= 1e-6) {
                    originalWeightedAreaSum += pow(area, area_influence_alpha);
                }
                originalTriangleCount++;
            }
        }
    }

    // 3. 平滑約束
    float smoothWeightedAreaSum = 0.0f;
    int smoothPairCount = 0;

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            if (triangleValidity[i][tri_idx]) {
                const Triangle& currentTriangle = triangulations[i].triangles[tri_idx];
                for (int neighbor_idx : currentTriangle.neighbor_triangles) {
                    if (neighbor_idx < trianglesPerImage && triangleValidity[i][neighbor_idx]) {
                        float avg_area = (triangleAreas[i][tri_idx] +
                            triangleAreas[i][neighbor_idx]) / 2.0f;
                        if (area_influence_alpha >= 1e-6) {
                            smoothWeightedAreaSum += pow(avg_area, area_influence_alpha);
                        }
                        smoothPairCount++;
                    }
                }
            }
        }
    }

    // ========== 計算邊界 ==========
    vector<vector<Mat>> boundaries(n, vector<Mat>(n));
    cout << "\n--- Computing Mask Boundaries ---" << endl;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!overlaps[i][j].empty()) {
                boundaries[i][j] = findMaskBoundary(imageMasks[i], imageMasks[j]);
                boundaries[j][i] = boundaries[i][j];
            }
        }
    }

    // 4. 邊界約束
    vector<int> dilationLevels = { 0, 1, 2, 3, 4, 5 };
    float boundaryWeightedAreaSum = 0.0f;
    int boundaryTriangleCount = 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!boundaries[i][j].empty()) {
                for (int dilationLevel : dilationLevels) {
                    Mat dilatedBound = dilatedBoundary(boundaries[i][j], dilationLevel);

                    for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
                        if (triangleValidity[i][tri_idx] && triangleValidity[j][tri_idx]) {
                            bool intersectsI = triangleIntersectsBoundary(dilatedBound,
                                triangulations[i].triangles[tri_idx]);
                            bool intersectsJ = triangleIntersectsBoundary(dilatedBound,
                                triangulations[j].triangles[tri_idx]);
                            if (intersectsI && intersectsJ) {
                                float area = triangleAreas[i][tri_idx];
                                if (area_influence_alpha >= 1e-6) {
                                    boundaryWeightedAreaSum += pow(area, area_influence_alpha);
                                }
                                if (dilationLevel == 0) boundaryTriangleCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    if (area_influence_alpha >= 1e-6) {
        cout << "\nWeighted area sums (area^" << area_influence_alpha << "):" << endl;
        cout << "  Overlap: " << overlapWeightedAreaSum
            << " (" << overlapTriangleCount << " constraints)" << endl;
        cout << "  Original: " << originalWeightedAreaSum
            << " (" << originalTriangleCount << " constraints)" << endl;
        cout << "  Smooth: " << smoothWeightedAreaSum
            << " (" << smoothPairCount << " pairs)" << endl;
        cout << "  Boundary: " << boundaryWeightedAreaSum
            << " (" << boundaryTriangleCount << " constraints)" << endl;
    }

    // ========== 計算方程總數 ==========
    vector<uchar> testValues = { 0, 64, 128, 192, 255 };

    int validOverlapConstraints = 0;
    int validOriginalConstraints = 0;
    int validSmoothConstraints = 0;
    int validBoundaryConstraints = 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!overlaps[i][j].empty()) {
                for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
                    bool validInI = hasValidPixelsInOverlap(images[i], overlaps[i][j],
                        imageMasks[i], triangulations[i].triangles[tri_idx]);
                    bool validInJ = hasValidPixelsInOverlap(images[j], overlaps[i][j],
                        imageMasks[j], triangulations[j].triangles[tri_idx]);
                    if (validInI && validInJ) {
                        validOverlapConstraints += 3;
                    }
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            if (triangleValidity[i][tri_idx]) {
                validOriginalConstraints += 3;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            if (triangleValidity[i][tri_idx]) {
                const Triangle& currentTriangle = triangulations[i].triangles[tri_idx];
                for (int neighbor_idx : currentTriangle.neighbor_triangles) {
                    if (neighbor_idx < trianglesPerImage && triangleValidity[i][neighbor_idx]) {
                        validSmoothConstraints += testValues.size() * 3;
                    }
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!boundaries[i][j].empty()) {
                for (int dilationLevel : dilationLevels) {
                    Mat dilatedBound = dilatedBoundary(boundaries[i][j], dilationLevel);
                    for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
                        if (triangleValidity[i][tri_idx] && triangleValidity[j][tri_idx]) {
                            bool intersectsI = triangleIntersectsBoundary(dilatedBound,
                                triangulations[i].triangles[tri_idx]);
                            bool intersectsJ = triangleIntersectsBoundary(dilatedBound,
                                triangulations[j].triangles[tri_idx]);
                            if (intersectsI && intersectsJ) {
                                validBoundaryConstraints += 3;
                            }
                        }
                    }
                }
            }
        }
    }

    int numEquations = validOverlapConstraints + validOriginalConstraints +
        validSmoothConstraints + validBoundaryConstraints;

    cout << "\n=== Equation Count Analysis ===" << endl;
    cout << "  Valid overlap constraints: " << validOverlapConstraints << endl;
    cout << "  Valid original constraints: " << validOriginalConstraints << endl;
    cout << "  Valid smooth constraints: " << validSmoothConstraints << endl;
    cout << "  Valid boundary constraints: " << validBoundaryConstraints << endl;
    cout << "  Total equations: " << numEquations << endl;
    cout << "  Total parameters: " << totalParams << endl;

    if (numEquations == 0) {
        cout << "Error: No valid equations to solve!" << endl;
        return;
    }

    // ========== 構建稀疏矩陣 ==========
    typedef Eigen::Triplet<float> T;
    vector<T> coefficients;
    coefficients.reserve(numEquations * 10);
    VectorXf b(numEquations);
    b.setZero();

    int eq_idx = 0;

    // ========== 統計實際權重範圍（用於調試） ==========
    float min_overlap_weight = FLT_MAX, max_overlap_weight = 0.0f;
    float min_original_weight = FLT_MAX, max_original_weight = 0.0f;
    float min_smooth_weight = FLT_MAX, max_smooth_weight = 0.0f;
    float min_boundary_weight = FLT_MAX, max_boundary_weight = 0.0f;

    // ========== 1. 重疊約束 ==========
    cout << "\nAdding overlap constraints..." << endl;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!overlaps[i][j].empty()) {
                for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
                    bool validInI = hasValidPixelsInOverlap(images[i], overlaps[i][j],
                        imageMasks[i], triangulations[i].triangles[tri_idx]);
                    bool validInJ = hasValidPixelsInOverlap(images[j], overlaps[i][j],
                        imageMasks[j], triangulations[j].triangles[tri_idx]);

                    if (validInI && validInJ) {
                        float area = triangleAreas[i][tri_idx];
                        float area_weight = computeTriangleWeight(area, overlap_base_weight,
                            overlapWeightedAreaSum,
                            overlapTriangleCount);

                        min_overlap_weight = min(min_overlap_weight, area_weight);
                        max_overlap_weight = max(max_overlap_weight, area_weight);

                        vector<float> mean_i = calculateTriangleMean(images[i], overlaps[i][j],
                            imageMasks[i], triangulations[i].triangles[tri_idx]);
                        vector<float> mean_j = calculateTriangleMean(images[j], overlaps[i][j],
                            imageMasks[j], triangulations[j].triangles[tri_idx]);

                        int idx_i = (i * trianglesPerImage + tri_idx) * paramsPerTriangle;
                        int idx_j = (j * trianglesPerImage + tri_idx) * paramsPerTriangle;

                        for (int c = 0; c < 3; c++) {
                            coefficients.push_back(T(eq_idx, idx_i + c * 2, mean_i[c] * area_weight));
                            coefficients.push_back(T(eq_idx, idx_i + c * 2 + 1, area_weight));
                            coefficients.push_back(T(eq_idx, idx_j + c * 2, -mean_j[c] * area_weight));
                            coefficients.push_back(T(eq_idx, idx_j + c * 2 + 1, -area_weight));
                            b(eq_idx) = 0;
                            eq_idx++;
                        }
                    }
                }
            }
        }
    }

    // ========== 2. 原始約束 ==========
    cout << "Adding original constraints..." << endl;

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            if (triangleValidity[i][tri_idx]) {
                float area = triangleAreas[i][tri_idx];
                float area_weight = computeTriangleWeight(area, original_base_weight,
                    originalWeightedAreaSum,
                    originalTriangleCount);

                min_original_weight = min(min_original_weight, area_weight);
                max_original_weight = max(max_original_weight, area_weight);

                vector<float> mean = calculateTriangleMean(images[i], Mat(),
                    imageMasks[i], triangulations[i].triangles[tri_idx]);
                int idx = (i * trianglesPerImage + tri_idx) * paramsPerTriangle;

                for (int c = 0; c < 3; c++) {
                    coefficients.push_back(T(eq_idx, idx + c * 2, mean[c] * area_weight));
                    coefficients.push_back(T(eq_idx, idx + c * 2 + 1, area_weight));
                    b(eq_idx) = mean[c] * area_weight;
                    eq_idx++;
                }
            }
        }
    }

    // ========== 3. 平滑約束 ==========
    cout << "Adding smooth constraints..." << endl;

    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            if (triangleValidity[i][tri_idx]) {
                const Triangle& currentTriangle = triangulations[i].triangles[tri_idx];
                int curr_idx = (i * trianglesPerImage + tri_idx) * paramsPerTriangle;

                for (int neighbor_idx : currentTriangle.neighbor_triangles) {
                    if (neighbor_idx < trianglesPerImage && triangleValidity[i][neighbor_idx]) {
                        float avg_area = (triangleAreas[i][tri_idx] +
                            triangleAreas[i][neighbor_idx]) / 2.0f;
                        float area_weight = computeTriangleWeight(avg_area, smooth_base_weight,
                            smoothWeightedAreaSum,
                            smoothPairCount);

                        min_smooth_weight = min(min_smooth_weight, area_weight);
                        max_smooth_weight = max(max_smooth_weight, area_weight);

                        int neigh_idx = (i * trianglesPerImage + neighbor_idx) * paramsPerTriangle;

                        for (uchar testValue : testValues) {
                            for (int channel = 0; channel < 3; channel++) {
                                coefficients.push_back(T(eq_idx, curr_idx + channel * 2,
                                    testValue * area_weight));
                                coefficients.push_back(T(eq_idx, curr_idx + channel * 2 + 1,
                                    area_weight));
                                coefficients.push_back(T(eq_idx, neigh_idx + channel * 2,
                                    -testValue * area_weight));
                                coefficients.push_back(T(eq_idx, neigh_idx + channel * 2 + 1,
                                    -area_weight));
                                b(eq_idx) = 0;
                                eq_idx++;
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 4. 邊界約束 ==========
    cout << "Adding boundary constraints..." << endl;
    int boundaryConstraintCount = 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (!boundaries[i][j].empty()) {
                for (int dilationLevel : dilationLevels) {
                    Mat dilatedBound = dilatedBoundary(boundaries[i][j], dilationLevel);

                    for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
                        if (triangleValidity[i][tri_idx] && triangleValidity[j][tri_idx]) {
                            bool intersectsI = triangleIntersectsBoundary(dilatedBound,
                                triangulations[i].triangles[tri_idx]);
                            bool intersectsJ = triangleIntersectsBoundary(dilatedBound,
                                triangulations[j].triangles[tri_idx]);

                            if (intersectsI && intersectsJ) {
                                float area = triangleAreas[i][tri_idx];
                                float area_weight = computeTriangleWeight(area, boundary_base_weight,
                                    boundaryWeightedAreaSum,
                                    boundaryTriangleCount * dilationLevels.size());

                                min_boundary_weight = min(min_boundary_weight, area_weight);
                                max_boundary_weight = max(max_boundary_weight, area_weight);

                                vector<float> boundary_mean_i = calculateTriangleBoundaryMean(
                                    images[i], dilatedBound, imageMasks[i],
                                    triangulations[i].triangles[tri_idx]);
                                vector<float> boundary_mean_j = calculateTriangleBoundaryMean(
                                    images[j], dilatedBound, imageMasks[j],
                                    triangulations[j].triangles[tri_idx]);

                                int idx_i = (i * trianglesPerImage + tri_idx) * paramsPerTriangle;
                                int idx_j = (j * trianglesPerImage + tri_idx) * paramsPerTriangle;

                                for (int c = 0; c < 3; c++) {
                                    coefficients.push_back(T(eq_idx, idx_i + c * 2,
                                        boundary_mean_i[c] * area_weight));
                                    coefficients.push_back(T(eq_idx, idx_i + c * 2 + 1,
                                        area_weight));
                                    coefficients.push_back(T(eq_idx, idx_j + c * 2,
                                        -boundary_mean_j[c] * area_weight));
                                    coefficients.push_back(T(eq_idx, idx_j + c * 2 + 1,
                                        -area_weight));
                                    b(eq_idx) = 0;
                                    eq_idx++;
                                }

                                if (dilationLevel == 0) {
                                    boundaryConstraintCount++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 顯示權重範圍統計 ==========
    cout << "\n=== Actual Weight Ranges ===" << endl;
    if (area_influence_alpha < 1e-6) {
        cout << "  Using FIXED weights (all triangles have same weight per constraint type)" << endl;
        cout << "  Overlap weight: " << overlap_base_weight << " (constant)" << endl;
        cout << "  Original weight: " << original_base_weight << " (constant)" << endl;
        cout << "  Smooth weight: " << smooth_base_weight << " (constant)" << endl;
        cout << "  Boundary weight: " << boundary_base_weight << " (constant)" << endl;
    }
    else {
        cout << "  Overlap weights: [" << min_overlap_weight << ", " << max_overlap_weight << "]" << endl;
        if (max_overlap_weight > 1e-6) {
            cout << "    Weight ratio: " << (max_overlap_weight / min_overlap_weight) << "x" << endl;
        }
        cout << "  Original weights: [" << min_original_weight << ", " << max_original_weight << "]" << endl;
        if (max_original_weight > 1e-6) {
            cout << "    Weight ratio: " << (max_original_weight / min_original_weight) << "x" << endl;
        }
        cout << "  Smooth weights: [" << min_smooth_weight << ", " << max_smooth_weight << "]" << endl;
        if (max_smooth_weight > 1e-6) {
            cout << "    Weight ratio: " << (max_smooth_weight / min_smooth_weight) << "x" << endl;
        }
        cout << "  Boundary weights: [" << min_boundary_weight << ", " << max_boundary_weight << "]" << endl;
        if (max_boundary_weight > 1e-6) {
            cout << "    Weight ratio: " << (max_boundary_weight / min_boundary_weight) << "x" << endl;
        }
    }

    // ========== 求解 ==========
    cout << "\n=== Solving System ===" << endl;

    SparseMatrix<float> A(numEquations, totalParams);
    A.setFromTriplets(coefficients.begin(), coefficients.end());

    float regularization = 1e-6;
    SparseMatrix<float> I(totalParams, totalParams);
    I.setIdentity();
    SparseMatrix<float> ATA = A.transpose() * A + regularization * I;
    VectorXf ATb = A.transpose() * b;

    Eigen::SimplicialLDLT<SparseMatrix<float>> solver;
    solver.compute(ATA);

    if (solver.info() != Eigen::Success) {
        cout << "Error: Matrix decomposition failed!" << endl;
        return;
    }

    VectorXf x = solver.solve(ATb);

    if (solver.info() != Eigen::Success) {
        cout << "Error: Solving failed!" << endl;
        return;
    }

    float residual = (A * x - b).norm();
    cout << "Solution residual: " << residual << endl;

    // ========== 更新係數 ==========
    int updatedTriangles = 0;
    for (int i = 0; i < n; i++) {
        for (int tri_idx = 0; tri_idx < trianglesPerImage; tri_idx++) {
            int idx = (i * trianglesPerImage + tri_idx) * paramsPerTriangle;

            if (triangleValidity[i][tri_idx]) {
                for (int c = 0; c < 6; c++) {
                    triangulations[i].triangles[tri_idx].coefficients[c] = x(idx + c);
                }
                updatedTriangles++;
            }
            else {
                triangulations[i].triangles[tri_idx].coefficients =
                { 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
            }
        }
    }

    cout << "Updated coefficients for " << updatedTriangles << " valid triangles" << endl;
    cout << "===========================================" << endl;
}

// 應用三角形校正
void applyTriangleCorrections(vector<Mat>& images, const vector<Mat>& imageMasks,
    const vector<DelaunayTriangulation>& triangulations) {
    vector<Mat> results;
    for (const Mat& img : images) {
        results.push_back(img.clone());
    }

    for (int i = 0; i < images.size(); i++) {
        const Mat& imageMask = imageMasks[i];
        const DelaunayTriangulation& triangulation = triangulations[i];

        for (int y = 0; y < images[i].rows; y++) {
            for (int x = 0; x < images[i].cols; x++) {
                if (imageMask.at<uchar>(y, x) != 0) {
                    // 找到包含此像素的三角形
                    int triangleIdx = triangulation.findTriangleContaining(Point2f(x, y));

                    if (triangleIdx >= 0) {
                        const Triangle& triangle = triangulation.triangles[triangleIdx];
                        Vec3b& pixel = results[i].at<Vec3b>(y, x);

                        for (int c = 0; c < 3; c++) {
                            float val = pixel[c] * triangle.coefficients[c * 2] +
                                triangle.coefficients[c * 2 + 1];
                            pixel[c] = saturate_cast<uchar>(val);
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < images.size(); i++) {
        images[i] = results[i];
    }
}

// 主要的三角形色彩校正處理函數
void processTriangleColorCorrection(vector<Mat>& images, const vector<Mat>& imageMasks,
    const vector<vector<Mat>>& overlaps,
    const vector<ImageCorrespondences>& correspondences,
    const vector<vector<Point2f>>& nonOverlapFeatures) {  // 新增參數

    if (images.empty()) {
        cout << "Error: No images to process" << endl;
        return;
    }

    cout << "Starting triangle-based color correction..." << endl;

    vector<DelaunayTriangulation> triangulations;
    Size imageSize = images[0].size();

    // 創建三角化時傳入非重疊特徵點
    DelaunayTriangulation baseTriangulation = createDelaunayFromCorrespondences(
        correspondences, imageSize, imageMasks, nonOverlapFeatures);

    for (int i = 0; i < images.size(); i++) {
        triangulations.push_back(baseTriangulation);
    }

    solveTriangleCoefficients(images, imageMasks, overlaps, triangulations);
    applyTriangleCorrections(images, imageMasks, triangulations);

    cout << "Triangle-based color correction completed" << endl;
}

bool processSingleDataset(const string& basePath) {
    // 從完整路徑中提取資料集名稱用於顯示
    size_t lastSlash = basePath.find_last_of("/\\");
    string datasetName = (lastSlash != string::npos) ? basePath.substr(lastSlash + 1) : basePath;
    if (datasetName.back() == '/' || datasetName.back() == '\\') {
        datasetName.pop_back();
    }

    cout << "\n===========================================" << endl;
    cout << "Processing dataset: " << datasetName << endl;
    cout << "===========================================" << endl;

    // 用於時間測量的變數
    auto datasetStartTime = chrono::high_resolution_clock::now();

    // 構建路徑 - 使用 Windows 路徑分隔符
    const string folderPath = basePath + "multiview_warp_result\\";
    const string overlapPath = basePath + "overlap\\";
    const string maskPath = basePath + "img_masks\\";
    const string globalPath = basePath + "global\\";  // 新增：global 資料夾路徑
    const string outputPath = basePath + "CCATMP_test1\\";

    // 自動創建輸出目錄，如果失敗就終止處理
    if (!createDirectory(outputPath)) {
        cout << "Error: Cannot create output directory: " << outputPath << endl;
        cout << "Dataset " << datasetName << " processing aborted." << endl;
        return false;  // 直接返回失敗
    }

    // 讀取圖片和重疊區域
    vector<Mat> images;
    vector<vector<Mat>> overlaps;
    vector<Mat> imageMasks;
    vector<int> validImageIndices; // 記錄哪些圖片索引是有效的

    cout << "Starting image processing for " << datasetName << "..." << endl;

    // 計時：讀取圖片開始
    auto readStartTime = chrono::high_resolution_clock::now();

    // 先掃描哪些圖片檔案存在
    vector<bool> imageExists(12, false);
    for (int i = 0; i <= 11; ++i) {
        string filename = folderPath + to_string(i / 10) + to_string(i % 10) + "__warped_img.png";
        if (fileExists(filename)) {
            imageExists[i] = true;
        }
    }

    // 讀取存在的圖片
    for (int i = 0; i <= 11; ++i) {
        if (!imageExists[i]) {
            //cout << "Image " << i << " does not exist, skipping..." << endl;
            continue;
        }

        string filename = folderPath + to_string(i / 10) + to_string(i % 10) + "__warped_img.png";
        //cout << "Reading image: " << filename << endl;

        Mat img = smartReadImage(filename, IMREAD_COLOR);
        if (!img.empty()) {
            images.push_back(img);
            validImageIndices.push_back(i); // 記錄這個圖片的原始索引
            //cout << "Successfully read image " << i << " (" << img.size() << ")" << endl;

            // 讀取對應的遮罩
            string maskFilename = maskPath + "mask_" + to_string(i / 10) + to_string(i % 10) + ".png";
            //cout << "Reading mask: " << maskFilename << endl;

            Mat mask = smartReadImage(maskFilename, IMREAD_GRAYSCALE);
            if (!mask.empty()) {
                imageMasks.push_back(mask);
                //cout << "Successfully read mask " << i << endl;
            }
            else {
                // 如果遮罩不存在，創建全白遮罩（表示整個圖像都有效）
                //cout << "Mask " << i << " not found, creating default mask" << endl;
                mask = Mat(img.size(), CV_8UC1, Scalar(255));
                imageMasks.push_back(mask);
            }
        }
        else {
            cout << "Failed to read image " << i << " (file exists but couldn't load)" << endl;
        }
    }

    cout << "Total images loaded: " << images.size() << endl;
    cout << "Total masks loaded: " << imageMasks.size() << endl;
    cout << "Valid image indices: ";
    for (int idx : validImageIndices) {
        cout << idx << " ";
    }
    cout << endl;

    if (images.empty()) {
        cout << "No images loaded for dataset " << datasetName << ". Skipping..." << endl;
        return false;
    }

    // 新增：載入 correspondences
    vector<ImageCorrespondences> allCorrespondences = loadAllCorrespondences(globalPath, validImageIndices);
    printCorrespondencesStatistics(allCorrespondences);

    // 新增：載入非重疊特徵點
    const string nonOverlapPath = basePath + "non_overlap_features\\";
    vector<vector<Point2f>> allNonOverlapFeatures = loadAllNonOverlapFeatures(nonOverlapPath, validImageIndices);

    // 新增：創建並可視化只包含特徵點的三角網格
    if (!allCorrespondences.empty()) {
        cout << "\n========== Creating Feature-Only Triangulation ==========" << endl;
        DelaunayTriangulation featureOnlyTriangulation = createFeatureOnlyTriangulation(
            allCorrespondences, images[0].size(), allNonOverlapFeatures);

        visualizeFeatureOnlyTriangulation(featureOnlyTriangulation, images[0].size(),
            outputPath, datasetName);
        cout << "=========================================================" << endl;
    }

    // 原有的完整三角化（包含輪廓點和角點）
    if (!allCorrespondences.empty()) {
        DelaunayTriangulation triangulation = createDelaunayFromCorrespondences(
            allCorrespondences, images[0].size(), imageMasks, allNonOverlapFeatures);

        visualizeDelaunayTriangulation(triangulation, images[0].size(), outputPath, datasetName,
            imageMasks, allCorrespondences, allNonOverlapFeatures);
    }

    // 讀取重疊區域 - 只讀取有效圖片之間的重疊
    overlaps.resize(images.size(), vector<Mat>(images.size()));
    int overlap_count = 0;

    for (int i = 0; i < images.size(); ++i) {
        for (int j = 0; j < images.size(); ++j) {
            if (i == j) continue; // 跳過自己與自己的重疊

            int original_i = validImageIndices[i];
            int original_j = validImageIndices[j];

            string filename = overlapPath + to_string(original_i / 10) + to_string(original_i % 10) + "__" +
                to_string(original_j / 10) + to_string(original_j % 10) + "__overlap.png";

            // 檢查檔案是否存在再讀取
            if (fileExists(filename)) {
                Mat overlap = smartReadImage(filename, IMREAD_GRAYSCALE);
                if (!overlap.empty()) {
                    overlaps[i][j] = overlap;
                    overlap_count++;
                    cout << "Read overlap mask " << original_i << "-" << original_j << " (internal: " << i << "-" << j << ")" << endl;
                }
            }
            else {
                cout << "Overlap file does not exist: " << filename << endl;
            }
        }
    }

    // 計時：讀取圖片結束
    auto readEndTime = chrono::high_resolution_clock::now();
    auto readDuration = chrono::duration_cast<chrono::milliseconds>(readEndTime - readStartTime).count();
    cout << "Total overlap masks loaded: " << overlap_count << endl;
    cout << "Time for loading images and masks: " << readDuration / 1000.0 << " seconds" << endl;

    // 計時：色彩校正開始
    auto correctionStartTime = chrono::high_resolution_clock::now();

    // 執行網格色彩校正
    try {
        processTriangleColorCorrection(images, imageMasks, overlaps, allCorrespondences, allNonOverlapFeatures);  // 加入第五個參數
        auto correctionEndTime = chrono::high_resolution_clock::now();
        auto correctionDuration = chrono::duration_cast<chrono::milliseconds>(correctionEndTime - correctionStartTime).count();
        cout << "color correction completed successfully" << endl;
        cout << "Time for color correction: " << correctionDuration / 1000.0 << " seconds" << endl;
    }
    catch (const exception& e) {
        cout << "Error during color correction for " << datasetName << ": " << e.what() << endl;
        return false;
    }

    // 計時：保存圖片開始
    auto saveStartTime = chrono::high_resolution_clock::now();

    // 保存結果 - 使用原始的圖片索引命名
    int saved_count = 0;
    for (int i = 0; i < images.size(); i++) {
        int original_index = validImageIndices[i];
        string outname = outputPath + to_string(original_index / 10) + to_string(original_index % 10) + "__warped_img.png";
        cout << "Saving image: " << outname << endl;

        bool success = imwrite(outname, images[i]);
        if (success) {
            cout << "Successfully saved image " << original_index << endl;
            saved_count++;
        }
        else {
            cout << "Failed to save image " << original_index << endl;
        }
    }

    // 計時：保存圖片結束和總時間
    auto saveEndTime = chrono::high_resolution_clock::now();
    auto saveDuration = chrono::duration_cast<chrono::milliseconds>(saveEndTime - saveStartTime).count();
    auto datasetEndTime = chrono::high_resolution_clock::now();
    auto datasetDuration = chrono::duration_cast<chrono::milliseconds>(datasetEndTime - datasetStartTime).count();

    cout << "Dataset " << datasetName << " completed. Total images saved: " << saved_count << endl;
    cout << "Time for saving images: " << saveDuration / 1000.0 << " seconds" << endl;
    cout << "Total time for dataset " << datasetName << ": " << datasetDuration / 1000.0 << " seconds" << endl;

    return true;
}