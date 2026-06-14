// C++ stuff
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
// nlohmann
#include "nlohmann/json.hpp"

using json = nlohmann::json;

#pragma pack(push, 1)
struct Glyph
{
    uint16_t letter;
    int8_t x0;
    int8_t y0;
    uint8_t dx;
    uint8_t pixelWidth;
    uint8_t pixelHeight;
    uint8_t _pad; // padding
    float s0;
    float t0;
    float s1;
    float t1;
};
#pragma pack(pop)

struct FontHeader
{
    int32_t fontNameOffset;
    int32_t pixelHeight;
    int32_t glyphCount;
    int32_t materialNameOffset;
};

static bool convertFont(const char* inputPath)
{
    std::ifstream file(inputPath, std::ios::binary);
    if (!file)
    {
        std::cerr << std::format("ERROR: Couldn't open '{}'\n", inputPath);
        return false;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (data.size() < 16)
    {
        std::cerr << std::format("ERROR: Font file '{}' too small\n", inputPath);
        return false;
    }

    FontHeader header;
    std::memcpy(&header, data.data(), sizeof(FontHeader));

    const char* varData = data.data() + 16;
    size_t varSize = data.size() - 16;

    auto resolveString = [&](int32_t fileOffset) -> std::string
    {
        int32_t idx = fileOffset - 16;
        if (idx < 0 || (size_t)idx >= varSize)
            return "<invalid offset>";

        return std::string(varData + idx);
    };

    std::string fontName = resolveString(header.fontNameOffset);
    std::string materialName = resolveString(header.materialNameOffset);
    std::string glowMaterial = materialName + "_glow";

    int glyphCount = header.glyphCount;
    size_t glyphDataSize = glyphCount * sizeof(Glyph);

    if (glyphDataSize > varSize)
    {
        std::cerr << std::format("ERROR: Glyph data exceeds file size in '{}'\n", inputPath);
        return false;
    }

    std::vector<Glyph> glyphs(glyphCount);
    std::memcpy(glyphs.data(), varData, glyphDataSize);

    json out;
    out["fontName"] = fontName;
    out["pixelHeight"] = header.pixelHeight;
    out["glyphCount"] = glyphCount;
    out["material"] = materialName;
    out["glowMaterial"] = glowMaterial;

    json glyphArray = json::array();

    for (const auto& g : glyphs)
    {
        json gj;

        std::string letter;

        if (g.letter < 0x80)
            letter = std::string(1, (char)g.letter);
        else if (g.letter < 0x800)
        {
            letter += (char)(0xC0 | (g.letter >> 6));
            letter += (char)(0x80 | (g.letter & 0x3F));
        }
        else
        {
            letter += (char)(0xE0 | (g.letter >> 12));
            letter += (char)(0x80 | ((g.letter >> 6) & 0x3F));
            letter += (char)(0x80 | (g.letter & 0x3F));
        }

        gj["letter"] = letter;
        gj["x0"] = g.x0;
        gj["y0"] = g.y0;
        gj["dx"] = g.dx;
        gj["width"] = g.pixelWidth;
        gj["height"] = g.pixelHeight;
        gj["s0"] = g.s0;
        gj["t0"] = g.t0;
        gj["s1"] = g.s1;
        gj["t1"] = g.t1;

        glyphArray.push_back(gj);
    }

    out["glyphs"] = glyphArray;

    std::string outPath = std::string(inputPath) + ".json";
    std::ofstream outFile(outPath);
    if (!outFile)
    {
        std::cerr << std::format("ERROR: Couldn't write '{}'\n", outPath);
        return false;
    }

    outFile << out.dump(2) << "\n";
    std::cout << std::format("  OK  {} -> {}\n", inputPath, outPath);
    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: fontbin2json <fontbin>\n";
        return 1;
    }

    int ok = 0, fail = 0;

    for (int i = 1; i < argc; i++)
    {
        if (convertFont(argv[i]))
            ok++;
        else
            fail++;
    }

    if (argc > 2)
        std::cout << std::format("\nDone: {} converted, {} failed\n", ok, fail);

    return fail > 0 ? 1 : 0;
}
