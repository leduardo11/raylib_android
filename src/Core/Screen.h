#pragma once

namespace Core {

struct Screen {
    virtual ~Screen() = default;
    virtual void onEnter() {}
    virtual void onExit()  {}
    virtual void update(float dt) = 0;
    virtual void render() = 0;
};

} // namespace Core
