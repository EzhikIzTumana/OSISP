#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <windows.h>

using namespace std;

struct Task {
    string command;
    string time;
    string params;
};

wstring stringToWstring(const string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

vector<Task> loadTasks(const string& filename) {
    vector<Task> tasks;

    wstring wideFilename = stringToWstring(filename);

    HANDLE hFile = CreateFileW(
        wideFilename.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "Error: Could not open file " << filename << endl;
        return tasks;
    }

    const DWORD bufferSize = 1024;
    char buffer[bufferSize];
    DWORD bytesRead;

    string fileContent;

    while (ReadFile(hFile, buffer, bufferSize - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        fileContent += buffer;
    }

    CloseHandle(hFile);

    istringstream fileStream(fileContent);
    string line;
    while (getline(fileStream, line)) {
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        Task task;
        if (iss >> task.command >> task.time) {
            getline(iss, task.params);
            tasks.push_back(task);
        }
    }

    return tasks;
}
void executeTask(const Task& task) {
    string fullCommand = task.command + " " + task.params;
    cout << "Executing: " << fullCommand << endl;

    int result = system(fullCommand.c_str());
    if (result != 0) {
        cerr << "Error executing task: " << task.command << endl;
    }
}

chrono::system_clock::time_point parseTime(const string& timeStr) {
    time_t now = time(nullptr);
    tm tm = {};
    localtime_s(&tm, &now);

    istringstream ss(timeStr);
    ss >> get_time(&tm, "%H:%M:%S");

    if (ss.fail()) {
        cerr << "Error parsing time: " << timeStr << endl;
        return chrono::system_clock::now();
    }

    time_t taskTime_t = mktime(&tm);
    if (taskTime_t == -1) {
        cerr << "Error converting time: " << timeStr << endl;
        return chrono::system_clock::now();
    }

    return chrono::system_clock::from_time_t(taskTime_t);
}

void scheduler(const vector<Task>& tasks) {
    while (true) {
        auto now = chrono::system_clock::now();
        auto now_time_t = chrono::system_clock::to_time_t(now);
        tm now_tm = {};
        localtime_s(&now_tm, &now_time_t);

        for (const auto& task : tasks) {
            chrono::system_clock::time_point taskTime = parseTime(task.time);
            auto taskTime_t = chrono::system_clock::to_time_t(taskTime);
            tm task_tm = {};
            localtime_s(&task_tm, &taskTime_t);

            if (now_tm.tm_hour == task_tm.tm_hour && now_tm.tm_min == task_tm.tm_min && now_tm.tm_sec == task_tm.tm_sec) {
                executeTask(task);
            }
        }

        this_thread::sleep_for(chrono::seconds(1));
    }
}

int main() {
    string filename = "C:/tasks.txt";
    auto tasks = loadTasks(filename);
    scheduler(tasks);
    return 0;
}
