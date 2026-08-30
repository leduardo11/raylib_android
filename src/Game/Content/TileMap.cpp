#include "TileMap.h"

#include "Json.h"

#include <stdexcept>

namespace Content {

static AtlasRegion parseRegion(const Json::Value& region)
{
    AtlasRegion r;
    if (const Json::Value* v = region.find("texture")) r.texture = v->asInt();
    if (const Json::Value* v = region.find("x")) r.x = v->asInt();
    if (const Json::Value* v = region.find("y")) r.y = v->asInt();
    if (const Json::Value* v = region.find("w")) r.w = v->asInt();
    if (const Json::Value* v = region.find("h")) r.h = v->asInt();
    return r;
}

TileMapData parseTileMap(const std::string& tilemapJson,
                         const std::string& atlasJson,
                         const std::string& collisionJson,
                         const std::string& manifestJson)
{
    using namespace Json;

    const Value tilemap = parse(tilemapJson);
    const Value atlas   = parse(atlasJson);
    const Value coll    = parse(collisionJson);
    const Value manifest = parse(manifestJson);

    TileMapData out;

    out.width    = tilemap.find("width")    ? tilemap.find("width")->asInt()  : 0;
    out.height   = tilemap.find("height")   ? tilemap.find("height")->asInt() : 0;
    out.tileSize = tilemap.find("tileSize") ? tilemap.find("tileSize")->asInt() : 32;

    if (out.width <= 0 || out.height <= 0)
        throw std::runtime_error("tilemap: bad map dimensions");

    // Ground layer (the cs_rpg/HelbreathAtlasPacker contract has exactly one
    // grid layer named "ground"; placement/shadow layers carry decorations).
    out.ground.assign((size_t)out.width * out.height, 0);
    const Value* layers = tilemap.find("layers");
    if (layers && layers->isArray())
    {
        for (size_t li = 0; li < layers->size(); ++li)
        {
            const Value* layer = layers->at(li);
            const std::string type = layer ? layer->find("type")->asString() : "";
            if (type != "grid") continue;

            const Value* tiles = layer->find("tiles");
            if (!tiles || !tiles->isArray())
                throw std::runtime_error("tilemap: grid layer has no tiles");
            if (tiles->size() != out.ground.size())
                throw std::runtime_error("tilemap: ground layer size mismatch");
            for (size_t i = 0; i < tiles->size(); ++i)
                out.ground[i] = tiles->at(i)->asInt();
        }
    }

    // Collision: cell == 0 is walkable (matches cs_rpg Grid.IsWalkable).
    const int cw = coll.find("width") ? coll.find("width")->asInt() : 0;
    const int ch = coll.find("height") ? coll.find("height")->asInt() : 0;
    const Value* cells = coll.find("cells");
    if (!cells || !cells->isArray() ||
        (int)cells->size() != cw * ch || cw != out.width || ch != out.height)
        throw std::runtime_error("tilemap: collision grid mismatch");

    out.walkable.assign((size_t)out.width * out.height, true);
    for (size_t i = 0; i < cells->size(); ++i)
        out.walkable[i] = (cells->at(i)->asInt() == 0);

    // Spawn (manifest.json playerSpawn), falling back to map center.
    if (const Value* ps = manifest.find("playerSpawn"))
    {
        const Value* x = ps->find("x");
        const Value* y = ps->find("y");
        if (x && y)
        {
            out.spawnTileX = x->asInt();
            out.spawnTileY = y->asInt();
        }
    }
    else
    {
        out.spawnTileX = out.width / 2;
        out.spawnTileY = out.height / 2;
    }

    // Texture atlas.
    if (const Value* t = atlas.find("textures"))
    {
        for (size_t i = 0; i < t->size(); ++i)
            out.textures.push_back(t->at(i)->asString());
    }
    if (const Value* r = atlas.find("regions"))
    {
        out.regions.reserve(r->size());
        for (size_t i = 0; i < r->size(); ++i)
            out.regions.push_back(parseRegion(*r->at(i)));
    }

    return out;
}

} // namespace Content