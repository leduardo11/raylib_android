#include "AtlasV2.h"

#include "Json.h"

#include <stdexcept>
#include <utility>

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

static SpriteFrame parseFrame(const Json::Value& frame)
{
    SpriteFrame f;
    if (const Json::Value* v = frame.find("region")) f.region = v->asInt();
    if (const Json::Value* v = frame.find("origin"))
    {
        if (const Json::Value* o = v->find("x")) f.originX = o->asInt();
        if (const Json::Value* o = v->find("y")) f.originY = o->asInt();
    }
    return f;
}

static DirectionClip parseDirection(const Json::Value& node)
{
    DirectionClip dc;
    if (const Json::Value* v = node.find("direction"))
        dc.direction = v->asString();
    if (const Json::Value* v = node.find("frames"))
    {
        for (size_t i = 0; i < v->size(); ++i)
            if (const Json::Value* f = v->at(i))
                dc.frames.push_back(parseFrame(*f));
    }
    return dc;
}

static AnimClip parseClip(const Json::Value& node)
{
    AnimClip c;
    if (const Json::Value* v = node.find("id")) c.id = v->asString();
    if (const Json::Value* v = node.find("frameTimeMs"))
        c.frameTimeMs = v->asInt();
    if (const Json::Value* v = node.find("loop")) c.loop = v->asBool();
    if (const Json::Value* v = node.find("holdLastFrame"))
        c.holdLastFrame = v->asBool();
    if (const Json::Value* v = node.find("directions"))
    {
        for (size_t i = 0; i < v->size(); ++i)
            if (const Json::Value* d = v->at(i))
                c.directions.push_back(parseDirection(*d));
    }
    return c;
}

static SemanticBinding parseState(const Json::Value& node)
{
    SemanticBinding b;
    if (const Json::Value* v = node.find("semantic")) b.semantic = v->asString();
    if (const Json::Value* v = node.find("variant")) b.variant = v->asString();
    if (const Json::Value* v = node.find("animation")) b.animation = v->asString();
    return b;
}

AtlasV2Package parseAtlasV2(const std::string& jsonText)
{
    using namespace Json;

    const Value root = parse(jsonText);

    AtlasV2Package pkg;

    const Value* format = root.find("format");
    if (!format || format->asString() != "atlas@2")
        throw std::runtime_error("atlas@2: not an atlas@2 package");

    if (const Value* v = root.find("name")) pkg.name = v->asString();

    const Value* atlas = root.find("atlas");
    if (atlas && atlas->isObject())
    {
        if (const Value* t = atlas->find("textures"))
        {
            for (size_t i = 0; i < t->size(); ++i)
                if (const Value* s = t->at(i))
                    pkg.textures.push_back(s->asString());
        }
        if (const Value* r = atlas->find("regions"))
        {
            pkg.regions.reserve(r->size());
            for (size_t i = 0; i < r->size(); ++i)
                if (const Value* reg = r->at(i))
                    pkg.regions.push_back(parseRegion(*reg));
        }
    }

    if (const Value* a = root.find("animations"))
    {
        pkg.clips.reserve(a->size());
        for (size_t i = 0; i < a->size(); ++i)
            if (const Value* c = a->at(i))
                pkg.clips.push_back(parseClip(*c));
    }

    if (const Value* s = root.find("states"))
    {
        pkg.states.reserve(s->size());
        for (size_t i = 0; i < s->size(); ++i)
            if (const Value* st = s->at(i))
                pkg.states.push_back(parseState(*st));
    }

    return pkg;
}

const char* directionName(Simulation::Direction d)
{
    switch (d)
    {
        case Simulation::Direction::N:  return "north";
        case Simulation::Direction::NE: return "northeast";
        case Simulation::Direction::E:  return "east";
        case Simulation::Direction::SE: return "southeast";
        case Simulation::Direction::S:  return "south";
        case Simulation::Direction::SW: return "southwest";
        case Simulation::Direction::W:  return "west";
        case Simulation::Direction::NW: return "northwest";
        default:                        return "south";
    }
}

const AnimClip* resolveAnimation(const AtlasV2Package& pkg,
                                 const char* semantic, const char* variant)
{
    for (const auto& b : pkg.states)
    {
        if (b.semantic != semantic) continue;
        if (!variant || variant[0] == '\0' || b.variant == variant)
        {
            for (const auto& c : pkg.clips)
                if (c.id == b.animation) return &c;
            return nullptr;
        }
    }
    return nullptr;
}

const DirectionClip* findDirection(const AnimClip& clip,
                                   const char* directionName)
{
    for (const auto& d : clip.directions)
        if (d.direction == directionName) return &d;
    return nullptr;
}

int frameIndexAt(const AnimClip& clip, const DirectionClip& direction,
                 float elapsedMs)
{
    size_t n = direction.frames.size();
    if (n == 0) return -1;

    float ft = (float)(clip.frameTimeMs > 0 ? clip.frameTimeMs : 1);

    if (clip.loop)
        return (int)(elapsedMs / ft) % (int)n;

    // Non-looping clips freeze on the final frame once the clip completes.
    long long i = (long long)(elapsedMs / ft);
    return (int)(i < (long long)n ? i : n - 1);
}

} // namespace Content