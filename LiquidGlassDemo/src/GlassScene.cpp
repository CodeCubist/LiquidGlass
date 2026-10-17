#include "GlassScene.h"
#include <cmath>
#include <algorithm>

namespace glass {
namespace {
constexpr float kDragThreshold = 4.0f;   
constexpr float kHoverTau = 0.12f;       
constexpr float kFocusTau = 0.18f;
} 

float Scene::smoothToward(float current, float target, double dt, float tau) {
    if (tau <= 1e-5f) return target;
    const float a = static_cast<float>(1.0 - std::exp(-std::max(0.0, dt) / std::max(1e-4, static_cast<double>(tau))));
    const float v = current + (target - current) * a;
    if (std::abs(v - target) < 1e-4f) return target;
    return v;
}

float Widget::sdf(float px, float py) const {
    const float cx = x + 0.5f * w;
    const float cy = y + 0.5f * h;
    const float bx = 0.5f * w;
    const float by = 0.5f * h;
    const float r = std::min(cornerRadius, std::min(bx, by));
    const float qx = std::abs(px - cx) - (bx - r);
    const float qy = std::abs(py - cy) - (by - r);
    const float ax = std::max(qx, 0.0f);
    const float ay = std::max(qy, 0.0f);
    const float outside = std::hypot(ax, ay);
    const float inside = std::min(std::max(qx, qy), 0.0f);
    return outside + inside - r;
}

Widget* Scene::pick(float x, float y) const {
    
    for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it) {
        const Widget& w = *it;
        if (!w.style.clickable && !w.style.moveable) continue;
        
        
        const float expand = params_.hoverScalePx + params_.focusScalePx;
        if (w.sdf(x, y) <= expand) return const_cast<Widget*>(&w);
    }
    return nullptr;
}

void Scene::beginDrag(Widget& w, float x, float y) {
    dragOffX_ = x - w.x;
    dragOffY_ = y - w.y;
}

void Scene::onMouseDown(float x, float y) {
    pressed_ = pick(x, y);
    if (pressed_) {
        beginDrag(*pressed_, x, y);
        dragMoved_ = false;
        pressX_ = x;
        pressY_ = y;
        
        
        for (Widget& w : widgets_) w.focus = (&w == pressed_);
    }
}

void Scene::onMouseUp(float x, float y) {
    if (pressed_ && !dragMoved_) {
        Widget* released = pick(x, y);
        if (released == pressed_ && pressed_->style.clickable && onClick_) {
            onClick_(*pressed_);
        }
    }
    pressed_ = nullptr;
    dragged_ = nullptr;
}

void Scene::update(double dt, float mouseX, float mouseY, bool mouseInside) {
    
    if (pressed_ && pressed_->style.moveable) {
        const float dx = mouseX - pressX_;
        const float dy = mouseY - pressY_;
        if (!dragMoved_ && (std::abs(dx) > kDragThreshold || std::abs(dy) > kDragThreshold)) {
            dragMoved_ = true;
            dragged_ = pressed_;
        }
        if (dragMoved_) {
            pressed_->x = mouseX - dragOffX_;
            pressed_->y = mouseY - dragOffY_;
        }
    }
    
    hovered_ = mouseInside ? pick(mouseX, mouseY) : nullptr;

    
    for (size_t i = 0; i < widgets_.size(); ++i) {
        Widget& w = widgets_[i];
        w.hover = false;
        w.hoverK = smoothToward(w.hoverK, 0.0f, dt, kHoverTau);
        w.focusK = smoothToward(w.focusK, w.focus ? 1.0f : 0.0f, dt, kFocusTau);
        
        const double h = std::sin(w.x * 12.9898 + w.y * 78.233 + static_cast<double>(i) * 37.719);
        w.seed = static_cast<float>(h - std::floor(h));
    }
}

bool Scene::anyAnimActive() const {
    for (const Widget& w : widgets_) {
        if (w.hoverK > 0.0005f || w.focusK > 0.0005f) return true;
        if (w.hover && w.hoverK < 0.9995f) return true;
        if (w.focus && w.focusK < 0.9995f) return true;
    }
    return false;
}

} 
