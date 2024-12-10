#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

void processData(int* data, size_t size) {
    std::sort(data, data + size);
}

void threadFunction(int* data, size_t start, size_t end) {
    processData(data + start, end - start);
}

void processMemoryMappedFile(const std::string& filename, size_t numThreads) {
    HANDLE file = CreateFileA(
        filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file: " << GetLastError() << std::endl;
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(file, &fileSize)) {
        std::cerr << "Error getting file size: " << GetLastError() << std::endl;
        CloseHandle(file);
        return;
    }

    HANDLE fileMapping = CreateFileMappingA(
        file, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (fileMapping == NULL) {
        std::cerr << "Error creating file mapping: " << GetLastError() << std::endl;
        CloseHandle(file);
        return;
    }

    void* mapView = MapViewOfFile(
        fileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (mapView == NULL) {
        std::cerr << "Error mapping view of file: " << GetLastError() << std::endl;
        CloseHandle(fileMapping);
        CloseHandle(file);
        return;
    }

    int* data = static_cast<int*>(mapView);
    size_t dataSize = fileSize.QuadPart / sizeof(int);
    size_t chunkSize = dataSize / numThreads;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? dataSize : start + chunkSize;
        threads.emplace_back(threadFunction, data, start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Memory-mapped file processing time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
              << " ms" << std::endl;

    UnmapViewOfFile(mapView);
    CloseHandle(fileMapping);
    CloseHandle(file);
}

void processTraditional(const std::string& filename, size_t numThreads) {
    HANDLE file = CreateFileA(
        filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file: " << GetLastError() << std::endl;
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(file, &fileSize)) {
        std::cerr << "Error getting file size: " << GetLastError() << std::endl;
        CloseHandle(file);
        return;
    }

    size_t dataSize = fileSize.QuadPart / sizeof(int);
    std::vector<int> data(dataSize);

    DWORD bytesRead;
    if (!ReadFile(file, data.data(), static_cast<DWORD>(dataSize * sizeof(int)), &bytesRead, NULL)) {
        std::cerr << "Error reading file: " << GetLastError() << std::endl;
        CloseHandle(file);
        return;
    }
    CloseHandle(file);

    size_t chunkSize = dataSize / numThreads;

    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? dataSize : start + chunkSize;
        threads.emplace_back(threadFunction, data.data(), start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Traditional processing time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
              << " ms" << std::endl;

    HANDLE outputFile = CreateFileA(
        "output.bin", GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (outputFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error creating output file: " << GetLastError() << std::endl;
        return;
    }

    DWORD bytesWritten;
    if (!WriteFile(outputFile, data.data(), static_cast<DWORD>(dataSize * sizeof(int)), &bytesWritten, NULL)) {
        std::cerr << "Error writing file: " << GetLastError() << std::endl;
    }
    CloseHandle(outputFile);
}

void processAsyncIO(const std::string& filename, size_t numThreads) {
    HANDLE file = CreateFileA(
        filename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file: " << GetLastError() << std::endl;
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(file, &fileSize)) {
        std::cerr << "Error getting file size: " << GetLastError() << std::endl;
        CloseHandle(file);
        return;
    }

    size_t dataSize = fileSize.QuadPart / sizeof(int);
    std::vector<int> data(dataSize);
    size_t chunkSize = dataSize / numThreads;

    auto startTime = std::chrono::high_resolution_clock::now();

    OVERLAPPED overlapped = {0};
    DWORD bytesRead = 0;
    if (!ReadFile(file, data.data(), static_cast<DWORD>(dataSize * sizeof(int)), &bytesRead, &overlapped)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            std::cerr << "Error reading file asynchronously: " << GetLastError() << std::endl;
            CloseHandle(file);
            return;
        }
        WaitForSingleObject(file, INFINITE);
    }

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? dataSize : start + chunkSize;
        threads.emplace_back(threadFunction, data.data(), start, end);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "Asynchronous I/O processing time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
              << " ms" << std::endl;

    CloseHandle(file);
}

int main() {
    const std::string filename = "data.bin";
    size_t numThreads = 4;

    std::cout << "Processing with memory-mapped file:" << std::endl;
    processMemoryMappedFile(filename, numThreads);

    std::cout << "Processing with traditional approach:" << std::endl;
    processTraditional(filename, numThreads);

    std::cout << "Processing with asynchronous I/O:" << std::endl;
    processAsyncIO(filename, numThreads);

    return 0;
}
