#include "TexturePack.h"

#include "FileIO.h"

#include <algorithm>
#include <vector>

namespace Content {

std::vector<int> splitPoints(const std::vector<AtlasRegion>& regions,
                             int textureIndex, int sheetDim, int maxDim,
                             bool horizontal)
{
    std::vector<int> cuts;
    cuts.push_back(0);

    if (sheetDim <= maxDim || maxDim <= 0)
    {
        cuts.push_back(sheetDim);
        return cuts;
    }

    int cur = 0;
    while (cur < sheetDim)
    {
        int next = cur + maxDim;
        if (next >= sheetDim) break;

        bool  pushed = true;
        int   guard  = 0;
        while (pushed && guard++ < (int)regions.size() + 1)
        {
            pushed = false;
            for (const auto& r : regions)
            {
                if (r.texture != textureIndex || r.w <= 0) continue;
                int lo = horizontal ? r.x : r.y;
                int hi = (horizontal ? r.x + r.w : r.y + r.h);
                // A cut would slice this region: push it past the region's end.
                if (lo < next && next < hi && hi > next)
                {
                    next = hi;
                    pushed = true;
                }
            }
        }

        if (next >= sheetDim) break;
        cuts.push_back(next);
        cur = next;
    }

    cuts.push_back(sheetDim);
    return cuts;
}

bool loadTexturePack(const char* dir,
                     const std::vector<std::string>& files,
                     const std::vector<AtlasRegion>& inRegions,
                     TexturePack& out)
{
    out.regions = inRegions;

    for (size_t tex = 0; tex < files.size(); ++tex)
    {
        std::string path = resolveAssetPath(std::string(dir) + "/" + files[tex]);
        Image sheet = LoadImage(path.c_str());
        if (sheet.data == nullptr)
        {
            unloadTexturePack(out);
            out.regions.clear();
            return false;
        }

        std::vector<int> xs = splitPoints(inRegions, (int)tex, sheet.width,
                                          MAX_TEXTURE_DIM, true);
        std::vector<int> ys = splitPoints(inRegions, (int)tex, sheet.height,
                                          MAX_TEXTURE_DIM, false);

        for (size_t row = 0; row + 1 < ys.size(); ++row)
        {
            for (size_t col = 0; col + 1 < xs.size(); ++col)
            {
                Rectangle rec{ (float)xs[col], (float)ys[row],
                               (float)(xs[col + 1] - xs[col]),
                               (float)(ys[row + 1] - ys[row]) };

                Image chunk = ImageFromImage(sheet, rec);
                Texture2D texture = LoadTextureFromImage(chunk);
                UnloadImage(chunk);
                out.textures.push_back(texture);

                int chunkIndex = (int)out.textures.size() - 1;
                for (auto& r : out.regions)
                {
                    if (r.texture != (int)tex || r.w <= 0) continue;
                    if (r.x >= xs[col] && r.x < xs[col + 1] &&
                        r.y >= ys[row] && r.y < ys[row + 1])
                    {
                        r.texture = chunkIndex;
                        r.x -= xs[col];
                        r.y -= ys[row];
                    }
                }
            }
        }

        UnloadImage(sheet);
    }

    return true;
}

void unloadTexturePack(TexturePack& pack)
{
    for (auto& t : pack.textures)
        if (t.id > 0)
            UnloadTexture(t);
    pack.textures.clear();
}

} // namespace Content