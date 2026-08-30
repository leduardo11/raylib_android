#include "MapRenderer.h"

#include "Game/Content/FileIO.h"
#include "raylib.h"

#include <cmath>
#include <string>

namespace Presentation {

namespace {

bool loadText(const std::string& path, std::string& out)
{
    std::string resolved = Content::resolveAssetPath(path);
    int size = 0;
    unsigned char* data = LoadFileData(resolved.c_str(), &size);
    if (!data) return false;
    out.assign((const char*)data, (size_t)size);
    UnloadFileData(data);
    return true;
}

} // namespace

bool MapRenderer::load(const std::string& dir)
{
    std::string tilemapJson, atlasJson, collisionJson, manifestJson;
    if (!loadText(dir + "/tilemap.json", tilemapJson) ||
        !loadText(dir + "/atlas.json", atlasJson) ||
        !loadText(dir + "/collision.json", collisionJson) ||
        !loadText(dir + "/manifest.json", manifestJson))
    {
        return false;
    }

    Content::TileMapData data;
    try
    {
        data = Content::parseTileMap(tilemapJson, atlasJson, collisionJson,
                                     manifestJson);
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_ERROR, "MapRenderer: %s", e.what());
        return false;
    }

    Content::TexturePack textures;
    if (!Content::loadTexturePack(
            Content::resolveAssetPath(dir + "/atlases").c_str(), data.textures,
            data.regions, textures))
    {
        return false;
    }

    m_data = std::move(data);
    m_textures = std::move(textures);
    m_loaded = true;
    return true;
}

void MapRenderer::draw(const Camera& cam) const
{
    if (!m_loaded) return;

    const int tw = m_data.width;
    const int th = m_data.height;
    const int tile = m_data.tileSize > 0 ? m_data.tileSize : 32;

    float ox = cam.origin().x;
    float oy = cam.origin().y;
    float vw = cam.viewWidth();
    float vh = cam.viewHeight();

    int x0 = (int)floorf(ox / (float)tile);
    int y0 = (int)floorf(oy / (float)tile);
    int x1 = (int)ceilf((ox + vw) / (float)tile);
    int y1 = (int)ceilf((oy + vh) / (float)tile);

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > tw) x1 = tw;
    if (y1 > th) y1 = th;

    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            int regionId = m_data.ground[(size_t)y * tw + x];
            if (regionId <= 0)
                continue;
            const auto& region = m_textures.regions[(size_t)regionId];
            if (region.w <= 0 || region.h <= 0)
                continue;
            if (region.texture < 0 ||
                region.texture >= (int)m_textures.textures.size())
                continue;

            Rectangle src{ (float)region.x, (float)region.y,
                           (float)tile, (float)tile };
            Vector2 dst{ (float)(x * tile), (float)(y * tile) };
            DrawTextureRec(m_textures.textures[region.texture], src, dst,
                           WHITE);
        }
    }
}

} // namespace Presentation