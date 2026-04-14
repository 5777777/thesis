#include "utils.h"

int main() {
    // 用於時間測量的變數
    auto totalStartTime = chrono::high_resolution_clock::now();

    // 創建輸出檔案
    ofstream outFile("multi_dataset_global_result.txt");

    // 檢查檔案是否成功打開
    if (!outFile.is_open()) {
        cerr << "Error: Cannot open output file." << endl;
        return -1;
    }

    // 創建雙重輸出對象
    DualOutput dualOut(&outFile);

    // 定義所有要處理的datasets
    vector<string> datasets = {


"D:/FloodNet_dataset_d/1/",
"D:/FloodNet_dataset_d/2/",
"D:/FloodNet_dataset_d/3/",
"D:/FloodNet_dataset_d/4/",
"D:/FloodNet_dataset_d/5/",
"D:/FloodNet_dataset_d/6/",
"D:/FloodNet_dataset_d/7/",
"D:/FloodNet_dataset_d/8/",
"D:/FloodNet_dataset_d/9/",
"D:/FloodNet_dataset_d/10/",
"D:/FloodNet_dataset_d/11/",
"D:/FloodNet_dataset_d/12/",
"D:/FloodNet_dataset_d/13/",
"D:/FloodNet_dataset_d/14/",
"D:/FloodNet_dataset_d/15/",
"D:/FloodNet_dataset_d/16/",
"D:/FloodNet_dataset_d/17/",
"D:/FloodNet_dataset_d/18/",
"D:/FloodNet_dataset_d/19/",
"D:/FloodNet_dataset_d/20/",
"D:/FloodNet_dataset_d/21/",
"D:/FloodNet_dataset_d/22/",
"D:/FloodNet_dataset_d/23/",
"D:/FloodNet_dataset_d/24/",
"D:/FloodNet_dataset_d/25/",
"D:/FloodNet_dataset_d/26/",
"D:/FloodNet_dataset_d/27/",
"D:/FloodNet_dataset_d/28/",
"D:/FloodNet_dataset_d/29/",
"D:/FloodNet_dataset_d/30/",






    };

    dualOut << "Starting multi-dataset processing..." << endl;
    dualOut << "Total datasets to process: " << datasets.size() << endl;

    // 處理統計
    int successful_datasets = 0;
    int failed_datasets = 0;
    vector<string> successful_list;
    vector<string> failed_list;

    // 逐個處理每個dataset
    for (const string& dataset : datasets) {
        bool success = processSingleDataset(dataset);
        if (success) {
            successful_datasets++;
            successful_list.push_back(dataset);
        }
        else {
            failed_datasets++;
            failed_list.push_back(dataset);
        }
    }

    // 總時間計算
    auto totalEndTime = chrono::high_resolution_clock::now();
    auto totalDuration = chrono::duration_cast<chrono::milliseconds>(totalEndTime - totalStartTime).count();

    // 輸出最終統計
    dualOut << "\n=================================================" << endl;
    dualOut << "FINAL PROCESSING SUMMARY" << endl;
    dualOut << "=================================================" << endl;
    dualOut << "Total datasets processed: " << datasets.size() << endl;
    dualOut << "Successful datasets: " << successful_datasets << endl;
    dualOut << "Failed datasets: " << failed_datasets << endl;
    dualOut << "Total processing time: " << totalDuration / 1000.0 << " seconds ("
        << totalDuration / 60000.0 << " minutes)" << endl;

    if (!successful_list.empty()) {
        dualOut << "\nSuccessfully processed datasets:" << endl;
        for (const string& dataset : successful_list) {
            dualOut << "  - " << dataset << endl;
        }
    }

    if (!failed_list.empty()) {
        dualOut << "\nFailed datasets:" << endl;
        for (const string& dataset : failed_list) {
            dualOut << "  - " << dataset << endl;
        }
    }

    // 關閉輸出檔案
    outFile.close();

    dualOut << "Multi-dataset processing completed!" << endl;
    dualOut << "Results saved to multi_dataset_global_result.txt" << endl;

    if (failed_datasets > 0) {
        dualOut << "Warning: " << failed_datasets << " datasets failed to process. Check the log file for details." << endl;
        return 1;
    }

    return 0;
}