#include "Render/Models/BmdModelFile.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 2)
        return 2;
    std::ifstream input(std::filesystem::path(argv[1]), std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0 || input.tellg() > Render::Models::BmdModelFile::MaxFileBytes)
        return 2;
    const auto size = static_cast<std::size_t>(input.tellg());
    input.seekg(0);
    std::vector<std::uint8_t> file(size), body;
    if (!input.read(reinterpret_cast<char*>(file.data()), size))
        return 2;
    if (!Render::Models::BmdModelFile::Decode(file, body))
        return 1;
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    std::cout.write(reinterpret_cast<const char*>(body.data()), body.size());
    return std::cout ? 0 : 2;
}
