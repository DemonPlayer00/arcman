#include <iostream>
#include <fstream>
#include <filesystem>
#include "handler.hpp"

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
	const char padding[0x18] = { 0 };
};
struct PACKED ArcStruct {
    const char head[12] = { 'B','U','R','I','K','O',' ','A','R','C','2','0' };
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
        for (const auto& entry : fs::directory_iterator(input)) {
            if (entry.is_regular_file()) {
                arc.count++;
            }
            else if (entry.is_directory()) {
                std::cerr << "W: Unsupported subdirectory: " << entry.path() << std::endl;
            }
        }
        std::cout << "Found " << arc.count << " valid file" << (arc.count > 1 ? "s" : "") << std::endl;
        unsigned int processed_count = 0;
        unsigned int offset = 0;
        for (const auto& entry : fs::directory_iterator(input)) {
            std::cout << "\033[2K\r" << std::flush;

            if (entry.is_regular_file()) {
                const fs::path& file_path = entry.path();
                std::ifstream fileStream(file_path, std::ios::binary);

                if (fileStream.is_open()) {
                    processed_count++;
                    std::cout << "Processing " << processed_count << "/" << arc.count << ": " << file_path.filename() << std::flush;
                    unsigned int file_size = fs::file_size(file_path);

                    IndexStruct index;
                    strncpy_s(index.name, sizeof(index.name), file_path.filename().string().c_str(), _TRUNCATE);
                    index.start = offset;
                    offset += file_size;
                    index.size = file_size;
                    arc.file_list.push_back(index);

                    char* buffer = new char[file_size];
                    fileStream.read(buffer, file_size);
                    arc.data.insert(arc.data.end(), buffer, buffer+file_size);
                    fileStream.close();
                }
                else {
                    std::cout << "\033[2K\r" << std::flush;
                    std::cerr << "E: Failed to open file: " << file_path << std::endl;
                    exit(2);
                }
            }
        }
        std::cout << "\033[2K\rWriting output file." << std::flush;
        std::ofstream outstream(output, std::ios::binary);
        if (!outstream.is_open()) {
            std::cout << "\033[2K\r" << std::flush;
            std::cerr << "E: Failed to open file: " << output << std::endl;
        }
        outstream.write(arc.head, sizeof(arc.head));
        arc.count = arc.file_list.size();
        outstream.write(reinterpret_cast<const char*>(&arc.count), sizeof(arc.count));
        outstream.write(reinterpret_cast<const char*>(arc.file_list.data()), sizeof(IndexStruct) * arc.file_list.size());
        outstream.write(reinterpret_cast<const char*>(arc.data.data()), sizeof(char) * arc.data.size());

        std::cout << "\033[2K\rSuccessfully processed " << processed_count << " valid file" << (processed_count > 1 ? "s" : "") << std::endl;
        outstream.close();
        exit(0);
    }

    void unpack(const char* input, const char* output) {
        namespace fs = std::filesystem;
        try {
            fs::create_directories(output);
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "E: Failed to create output directory: " << e.what() << std::endl;
            exit(2);
        }

        std::ifstream inStream(input, std::ios::binary);
        if (!inStream.is_open()) {
            std::cerr << "E: Failed to open input file: " << input << std::endl;
            exit(2);
        }

        char head[12];
        inStream.read(head, sizeof(head));
        ArcStruct tempArc;
        if (memcmp(head, tempArc.head, sizeof(head)) != 0) {
            std::cerr << "E: Invalid file format - incorrect header" << std::endl;
            exit(3);
        }

        unsigned int count = 0;
        inStream.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (count == 0) {
            std::cout << "W: No files found in archive" << std::endl;
            return;
        }

        std::vector<IndexStruct> file_list(count);
        inStream.read(reinterpret_cast<char*>(file_list.data()),
            sizeof(IndexStruct) * count);

        std::streampos data_start = inStream.tellg();
        if (data_start == std::streampos(-1)) {
            std::cerr << "E: Failed to determine data position" << std::endl;
            exit(4);
        }

        for (unsigned int i = 0; i < count; ++i) {
            const IndexStruct& index = file_list[i];
            std::cout << "\033[2K\rExtracting " << (i + 1) << "/" << count << ": " << index.name << std::flush;

            if (index.name[0] == '\0') {
                std::cerr << "\nW: Skipping file with empty name" << std::endl;
                continue;
            }

            fs::path output_path = fs::path(output) / index.name;

            if (index.size == 0) {
                std::cerr << "\nW: Skipping empty file: " << index.name << std::endl;
                continue;
            }

            inStream.seekg(data_start + static_cast<std::streamoff>(index.start));
            if (!inStream) {
                std::cerr << "\nE: Failed to seek to data for: " << index.name << std::endl;
                continue;
            }

            char* buffer = new char[index.size];
            inStream.read(buffer, index.size);

            if (inStream.gcount() != static_cast<std::streamsize>(index.size)) {
                std::cerr << "\nE: Incomplete data for: " << index.name << " (read "
                    << inStream.gcount() << "/" << index.size << " bytes)" << std::endl;
                delete[] buffer;
                continue;
            }

            std::ofstream outStream(output_path, std::ios::binary);
            if (outStream.is_open()) {
                outStream.write(buffer, index.size);
                outStream.close();
            }
            else {
                std::cerr << "\nE: Failed to create output file: " << output_path << std::endl;
            }

            delete[] buffer;
        }

        std::cout << "\033[2K\rSuccessfully extracted " << count << " file"
            << (count > 1 ? "s" : "") << std::endl;
        inStream.close();
        exit(0);
    }
}
