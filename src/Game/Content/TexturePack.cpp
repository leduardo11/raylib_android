#include "TexturePack.h"

#include "FileIO.h"

#include <algorithm>
#include <vector>

namespace Content {

std::vector<int> gridCuts(int sheetDim, int maxDim)
{
    std::vector<int> cuts;
    cuts.push_back(0);
    if (maxDim <= 0)
    {
        cuts.push_back(sheetDim);
        return cuts;
    }
    for (int c = maxDim; c < sheetDim; c += maxDim)
        cuts.push_back(c);
    cuts.push_back(sheetDim);
    return cuts;
}

std::vector<std::vector<GpuPiece>>
buildPieces(const std::vector<AtlasRegion>& regions, int textureIndex,
            const std::vector<int>& xs, const std::vector<int>& ys,
            int ncols, int chunkBase)
{
    std::vector<std::vector<GpuPiece>> result(regions.size());

    for (size_t i = 0; i < regions.size(); ++i)
    {
        const AtlasRegion& r = regions[i];
        if (r.w <= 0 || r.h <= 0 || r.texture != textureIndex)
            continue;

        int x0 = r.x, x1 = r.x + r.w;
        int y0 = r.y, y1 = r.y + r.h;

        for (size_t cx = 0; cx + 1 < xs.size(); ++cx)
        {
            int cellX0 = xs[cx], cellX1 = xs[cx + 1];
            if (x1 <= cellX0 || x0 >= cellX1) continue;
            int piX0 = std::max(x0, cellX0);
            int piX1 = std::min(x1, cellX1);

            for (size_t cy = 0; cy + 1 < ys.size(); ++cy)
            {
                int cellY0 = ys[cy], cellY1 = ys[cy + 1];
                if (y1 <= cellY0 || y0 >= cellY1) continue;
                int piY0 = std::max(y0, cellY0);
                int piY1 = std::min(y1, cellY1);

                GpuPiece p;
                p.texture = chunkBase + (int)cy * ncols + (int)cx;
                p.x = piX0 - cellX0;
                p.y = piY0 - cellY0;
                p.w = piX1 - piX0;
                p.h = piY1 - piY0;
                p.ox = piX0 - x0;
                p.oy = piY0 - y0;
                result[i].push_back(p);
            }
        }
    }

    return result;
}

bool loadTexturePack(const char* dir,
                     const std::vector<std::string>& files,
                     const std::vector<AtlasRegion>& inRegions,
                     TexturePack& out)
{
    out.textures.clear();
    out.pieces.assign(inRegions.size(), {});
    int chunkBase = 0;

    for (size_t tex = 0; tex < files.size(); ++tex)
    {
        std::string path =
            resolveAssetPath(std::string(dir) + "/" + files[tex]);
        Image sheet = LoadImage(path.c_str());
        if (sheet.data == nullptr)
        {
            unloadTexturePack(out);
            out.pieces.clear();
            return false;
        }

        std::vector<int> xs = gridCuts(sheet.width, MAX_TEXTURE_DIM);
        std::vector<int> ys = gridCuts(sheet.height, MAX_TEXTURE_DIM);
        int ncols = (int)xs.size() - 1;
        int nrows = (int)ys.size() - 1;

        std::vector<std::vector<GpuPiece>> sheetPieces =
            buildPieces(inRegions, (int)tex, xs, ys, ncols, chunkBase);
        for (size_t i = 0; i < sheetPieces.size(); ++i)
            if (!sheetPieces[i].empty())
                out.pieces[i] = std::move(sheetPieces[i]);

        for (int row = 0; row < nrows; ++row)
        {
            for (int col = 0; col < ncols; ++col)
            {
                Rectangle rec{ (float)xs[col], (float)ys[row],
                               (float)(xs[col + 1] - xs[col]),
                               (float)(ys[row + 1] - ys[row]) };

                Image chunk = ImageFromImage(sheet, rec);
                Texture2D texture = LoadTextureFromImage(chunk);
                UnloadImage(chunk);
                out.textures.push_back(texture);
            }
        }

        UnloadImage(sheet);
        chunkBase += nrows * ncols;

        int maxW = 0, maxH = 0;
        for (int c = (int)out.textures.size() - nrows * ncols;
             c < (int)out.textures.size(); ++c)
        {
            if (out.textures[c].width > MAX_TEXTURE_DIM ||
                out.textures[c].height > MAX_TEXTURE_DIM)
                TraceLog(LOG_WARNING,
                         "TexturePack: %s chunk %d exceeds %d (got %dx%d)",
                         files[tex].c_str(), c, MAX_TEXTURE_DIM,
                         out.textures[c].width, out.textures[c].height);
            if (out.textures[c].width > maxW)  maxW = out.textures[c].width;
            if (out.textures[c].height > maxH) maxH = out.textures[c].height;
        }
        TraceLog(LOG_INFO, "TexturePack: %s -> %d chunk textures (max %dx%d)",
                 files[tex].c_str(), nrows * ncols, maxW, maxH);
    }

    return true;
}

void unloadTexturePack(TexturePack& pack)
{
    for (auto& t : pack.textures)
        if (t.id > 0)
            UnloadTexture(t);
    pack.textures.clear();
    pack.pieces.clear();
}

} // namespace Content