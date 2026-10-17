#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#endif
#include <string>

namespace glass {
    
struct WidgetStyle {
    float smoothing = 0.003f;
    
    int blurRadius = 12;
    
    float tintR = 0.0f, tintG = 0.0f, tintB = 0.0f, tintA = 0.0f;
    
    float refThickness = 20.0f;        
    float refFactor = 1.4f;            
    float refDispersion = 7.0f;        
    float refFresnelRange = 30.0f;     
    float refFresnelHardness = 20.0f;  
    float refFresnelFactor = 20.0f;    
    float glareRange = 30.0f;          
    float glareHardness = 20.0f;       
    
    float shadowExpand = 30.0f;
    float shadowFactor = 0.25f;
    float shadowOffsetX = 0.0f;        
    float shadowOffsetY = 2.0f;
    float shadowR = 0.0f, shadowG = 0.0f, shadowB = 0.0f, shadowA = 1.0f;

    
    bool moveable = false;             
    bool clickable = false;            
};
    
struct Widget {
    int id = 0;
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    float cornerRadius = 8.0f;
    WidgetStyle style;

    std::wstring label;                
    float labelAlpha = 0.85f;

    
    bool hover = false;                
    bool focus = false;                
    float hoverK = 0.0f;               
    float focusK = 0.0f;               
    float seed = 0.0f;                 

    float sdf(float px, float py) const;
};

struct GlobalParams {
    float hoverScalePx = 1.5f;
    float focusScalePx = 2.5f;
    float focusBorderWidthPx = 2.0f;
    float focusBorderIntensity = 0.75f;
    float focusBorderSpeed = 1.6f;
    float pixelGridSize = 8.0f;
    float debugStep = 9.0f;            
};

} 
