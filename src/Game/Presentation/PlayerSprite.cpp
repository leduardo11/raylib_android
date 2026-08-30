#include "PlayerSprite.h"

#include "Game/Content/FileIO.h"
#include "raylib.h"

#include <stdexcept>
#include <string>

namespace Presentation {

namespace {

// The package lives beside its sheets (e.g. "entities/player/player.pkg.json"
// -> "entities/player"). Returns true and fills `dir` on success.
bool packageDir(const std::string& pkgPath, std::string& dir)
{
    size_t slash = pkgPath.find_last_of('/');
    if (slash == std::string::npos)
        return false;
    dir = pkgPath.substr(0, slash);
    return true;
}

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

bool PlayerSprite::load(const std::string& pkgPath)
{
    std::string jsonText;
    std::string dir;
    if (!packageDir(pkgPath, dir) || !loadText(pkgPath, jsonText))
        return false;

    Content::AtlasV2Package pkg;
    try
    {
        pkg = Content::parseAtlasV2(jsonText);
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_ERROR, "PlayerSprite: %s", e.what());
        return false;
    }

    Content::TexturePack textures;
    if (!Content::loadTexturePack(Content::resolveAssetPath(dir).c_str(),
                                  pkg.textures, pkg.regions, textures))
    {
        return false;
    }

    m_pkg = std::move(pkg);
    m_textures = std::move(textures);
    m_clipId.clear();
    m_elapsedMs = 0.0f;
    m_loaded = true;
    return true;
}

const Content::AnimClip* PlayerSprite::resolveClip(
    const PlayerPresentationState& s)
{
    if (s.isMoving && s.locomotion == Simulation::Locomotion::Running)
        return Content::resolveAnimation(m_pkg, "locomotion/running", "");
    if (s.isMoving)
        return Content::resolveAnimation(m_pkg, "locomotion/walking", "peace");
    return Content::resolveAnimation(m_pkg, "locomotion/standing", "peace");
}

void PlayerSprite::update(const PlayerPresentationState& s, float dtMs)
{
    if (!m_loaded) return;

    const Content::AnimClip* clip = resolveClip(s);
    if (!clip) return;

    if (clip->id != m_clipId)
    {
        m_clipId = clip->id;
        m_elapsedMs = 0.0f;
    }
    m_elapsedMs += dtMs;
}

void PlayerSprite::draw(const PlayerPresentationState& s, float tileSize)
{
    if (!m_loaded) return;

    const Content::AnimClip* clip = resolveClip(s);
    if (!clip) return;

    const Content::DirectionClip* direction =
        Content::findDirection(*clip, Content::directionName(s.facing));
    if (!direction) return;

    int frame = Content::frameIndexAt(*clip, *direction, m_elapsedMs);
    if (frame < 0 || frame >= (int)direction->frames.size()) return;

    const Content::SpriteFrame& f = direction->frames[(size_t)frame];
    if (f.region <= 0 || f.region >= (int)m_textures.pieces.size()) return;

    const auto& pieces = m_textures.pieces[(size_t)f.region];
    if (pieces.empty()) return;

    // Soft grounding shadow at the feet.
    DrawCircleV(s.position, tileSize * 0.22f, Fade(BLACK, 0.35f));

    Vector2 base{ s.position.x - (float)f.originX,
                  s.position.y - (float)f.originY };
    for (const auto& p : pieces)
    {
        if (p.w <= 0 || p.h <= 0) continue;
        if (p.texture < 0 || p.texture >= (int)m_textures.textures.size())
            continue;

        Rectangle src{ (float)p.x, (float)p.y, (float)p.w, (float)p.h };
        Vector2 dst{ base.x + (float)p.ox, base.y + (float)p.oy };
        DrawTextureRec(m_textures.textures[(size_t)p.texture], src, dst,
                       WHITE);
    }
}

} // namespace Presentation