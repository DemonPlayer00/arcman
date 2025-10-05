#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cstdlib>

#define ERROR_DIR_CREATE 2
#define ERROR_FILE_OPEN  2
#define ERROR_FORMAT     3
#define ERROR_DATA_POS   4
#define ERROR_SEEK       5
#define ERROR_READ_DATA  6

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

struct PACKED IndexStruct {
    char name[0x60] = {0};
    unsigned int start = 0;
    unsigned int size = 0;
    const char padding[0x18] = {0};
};

struct ArcStruct {
    const char head[12] = {'B','U','R','I','K','O',' ','A','R','C','2','0'};
    unsigned int count = 0;
    std::vector<IndexStruct> file_list;
    std::vector<char> data;
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef PACKED

namespace handler {
    void pack(const char* input, const char* output) {
        namespace fs = std::filesystem;
        ArcStruct arc;

        try {
            for (const auto& entry : fs::directory_iterator(input)) {
                if (entry.is_regular_file()) {
                    arc.count++;
                } else if (entry.is_directory()) {
                    std::cerr << "W: Unsupported subdirectory: " << entry.path() << std::endl;
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "E: Failed to scan input directory: " << e.what() << std::endl;
            exit(ERROR_DIR_CREATE);
        }
        std::cout << "Found " << arc.count << " valid file" << (arc.count > 1 ? "s" : "") << std::endl;

        unsigned int processed_count = 0;
        unsigned int offset = 0;

        try {
            for (const auto& entry : fs::directory_iterator(input)) {
                if (entry.is_regular_file()) {
                    const fs::path& file_path = entry.path();
                    std::ifstream fileStream(file_path, std::ios::binary);

                    if (!fileStream.is_open()) {
                        std::cerr << "\033[2K\rE: Failed to open file: " << file_path << std::endl;
                        exit(ERROR_FILE_OPEN);
                    }

                    unsigned int file_size;
                    try {
                        file_size = static_cast<unsigned int>(fs::file_size(file_path));
                    } catch (const fs::filesystem_error& e) {
                        std::cerr << "\033[2K\rE: Failed to get size of file: " << file_path << " (" << e.what() << ")" << std::endl;
                        fileStream.close();
                        exit(ERROR_FILE_OPEN);
                    }

                    if (file_size > 0xFFFFFFFF) {
                        std::cerr << "\033[2K\rE: File too large (max 4GB): " << file_path << std::endl;
                        fileStream.close();
                        exit(ERROR_FILE_OPEN);
                    }

                    IndexStruct index;
                    strncpy(index.name, file_path.filename().string().c_str(), sizeof(index.name) - 1);
                    index.start = offset;
                    offset += file_size;
                    index.size = file_size;
                    arc.file_list.push_back(index);

                    char* buffer = new char[file_size];
                    fileStream.read(buffer, file_size);

                    if (!fileStream) {
                        std::cerr << "\033[2K\rE: Failed to read data from file: " << file_path << std::endl;
                        delete[] buffer;
                        fileStream.close();
                        exit(ERROR_READ_DATA);
                    }

                    arc.data.insert(arc.data.end(), buffer, buffer + file_size);
                    delete[] buffer;
                    fileStream.close();
                    processed_count++;

                    std::cout << "\033[2K\r" << "Processing " << processed_count << "/" << arc.count << ": " << file_path.filename() << std::flush;
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "\033[2K\rE: Failed to process input directory: " << e.what() << std::endl;
            exit(ERROR_DIR_CREATE);
        }

        std::cout << "\033[2K\rWriting output file..." << std::flush;
        std::ofstream outstream(output, std::ios::binary);
        if (!outstream.is_open()) {
            std::cerr << "\033[2K\rE: Failed to open output file: " << output << std::endl;
            exit(ERROR_FILE_OPEN);
        }

        outstream.write(arc.head, sizeof(arc.head));
        if (!outstream) {
            std::cerr << "\033[2K\rE: Failed to write header to output file: " << output << std::endl;
            outstream.close();
            exit(ERROR_FILE_OPEN);
        }

        arc.count = arc.file_list.size();
        outstream.write(reinterpret_cast<const char*>(&arc.count), sizeof(arc.count));
        if (!outstream) {
            std::cerr << "\033[2K\rE: Failed to write file count to output file: " << output << std::endl;
            outstream.close();
            exit(ERROR_FILE_OPEN);
        }

        outstream.write(reinterpret_cast<const char*>(arc.file_list.data()), sizeof(IndexStruct) * arc.file_list.size());
        if (!outstream) {
            std::cerr << "\033[2K\rE: Failed to write index list to output file: " << output << std::endl;
            outstream.close();
            exit(ERROR_FILE_OPEN);
        }

        outstream.write(reinterpret_cast<const char*>(arc.data.data()), arc.data.size() * sizeof(char));
        if (!outstream) {
            std::cerr << "\033[2K\rE: Failed to write data to output file: " << output << std::endl;
            outstream.close();
            exit(ERROR_FILE_OPEN);
        }

        outstream.close();
        std::cout << "\033[2K\rSuccessfully processed " << processed_count << " valid file" << (processed_count > 1 ? "s" : "") << std::endl;
        exit(0);
    }

    void unpack(const char* input, const char* output) {
        namespace fs = std::filesystem;

        try {
            fs::create_directories(output);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "E: Failed to create output directory: " << e.what() << std::endl;
            exit(ERROR_DIR_CREATE);
        }

        std::ifstream inStream(input, std::ios::binary);
        if (!inStream.is_open()) {
            std::cerr << "E: Failed to open input file: " << input << std::endl;
            exit(ERROR_FILE_OPEN);
        }

        char head[12];
        inStream.read(head, sizeof(head));
        if (!inStream) {
            std::cerr << "E: Failed to read header from input file: " << input << std::endl;
            inStream.close();
            exit(ERROR_FORMAT);
        }
        ArcStruct tempArc;
        if (memcmp(head, tempArc.head, sizeof(head)) != 0) {
            std::cerr << "E: Invalid file format - incorrect header (not BURIKO ARC20)" << std::endl;
            inStream.close();
            exit(ERROR_FORMAT);
        }

        unsigned int count = 0;
        inStream.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (!inStream) {
            std::cerr << "E: Failed to read file count from input file: " << input << std::endl;
            inStream.close();
            exit(ERROR_FORMAT);
        }
        if (count == 0) {
            std::cout << "W: No files found in archive" << std::endl;
            inStream.close();
            exit(0);
        }

        std::vector<IndexStruct> file_list(count);
        inStream.read(reinterpret_cast<char*>(file_list.data()), sizeof(IndexStruct) * count);
        if (!inStream) {
            std::cerr << "E: Failed to read index list from input file: " << input << std::endl;
            inStream.close();
            exit(ERROR_FORMAT);
        }

        std::streampos data_start = inStream.tellg();
        if (data_start == std::streampos(-1)) {
            std::cerr << "E: Failed to determine data position in input file: " << input << std::endl;
            inStream.close();
            exit(ERROR_DATA_POS);
        }

        for (unsigned int i = 0;i < count;++i) {
            const IndexStruct& index = file_list[i];
            std::cout << "\033[2K\rExtracting " << (i + 1) << "/" << count << ": " << index.name << std::flush;

            if (index.name[0] == '\0') {
                std::cerr << "\nE: Empty filename found in index (invalid archive)" << std::endl;
                inStream.close();
                exit(ERROR_FORMAT);
            }

            if (index.size == 0) {
                std::cerr << "\nE: Zero-size file found in index (invalid archive): " << index.name << std::endl;
                inStream.close();
                exit(ERROR_FORMAT);
            }

            fs::path output_path = fs::path(output) / index.name;

            inStream.seekg(data_start + static_cast<std::streamoff>(index.start));
            if (!inStream) {
                std::cerr << "\nE: Failed to seek to data for file: " << index.name << std::endl;
                inStream.close();
                exit(ERROR_SEEK);
            }

            char* buffer = new char[index.size];
            inStream.read(buffer, index.size);
            if (!inStream || inStream.gcount() != static_cast<std::streamsize>(index.size)) {
                std::cerr << "\nE: Incomplete data for file: " << index.name << " (read "
                          << inStream.gcount() << "/" << index.size << " bytes)" << std::endl;
                delete[] buffer;
                inStream.close();
                exit(ERROR_READ_DATA);
            }

            std::ofstream outStream(output_path, std::ios::binary);
            if (!outStream.is_open()) {
                std::cerr << "\nE: Failed to create output file: " << output_path << std::endl;
                delete[] buffer;
                inStream.close();
                exit(ERROR_FILE_OPEN);
            }
            outStream.write(buffer, index.size);
            if (!outStream) {
                std::cerr << "\nE: Failed to write data to output file: " << output_path << std::endl;
                delete[] buffer;
                outStream.close();
                inStream.close();
                exit(ERROR_FILE_OPEN);
            }

            delete[] buffer;
            outStream.close();
        }

        std::cout << "\033[2K\rSuccessfully extracted " << count << " file"
                  << (count > 1 ? "s" : "") << std::endl;
        inStream.close();
        exit(0);
    }
}
