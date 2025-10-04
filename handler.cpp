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

	}
}
