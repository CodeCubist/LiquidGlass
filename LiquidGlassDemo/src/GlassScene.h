#pragma once
#include "GlassTypes.h"
#include <functional>
#include <vector>

namespace glass {

class Scene {
public:
    
    static float smoothToward(float current, float target, double dt, float tau);

    std::vector<Widget>& widgets() { return widgets_; }
    const std::vector<Widget>& widgets() const { return widgets_; }

    void update(double dt, float mouseX, float mouseY, bool mouseInside);
    void onMouseDown(float x, float y);
    void onMouseUp(float x, float y);
    void setClickHandler(std::function<void(Widget&)> handler) { onClick_ = std::move(handler); }

    Widget* hovered() { return hovered_; }
    const GlobalParams& params() const { return params_; }
    GlobalParams& params() { return params_; }

    bool anyAnimActive() const;

private:
    Widget* pick(float x, float y) const;   
    void beginDrag(Widget& w, float x, float y);

    std::vector<Widget> widgets_;
    GlobalParams params_;

    Widget* hovered_ = nullptr;
    Widget* pressed_ = nullptr;
    Widget* dragged_ = nullptr;
    bool dragMoved_ = false;
    float pressX_ = 0.0f, pressY_ = 0.0f;
    float dragOffX_ = 0.0f, dragOffY_ = 0.0f;
    std::function<void(Widget&)> onClick_;
};

} 
