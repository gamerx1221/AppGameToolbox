#pragma once

namespace appgametoolbox {

struct Color { float r = 1.0f; float g = 1.0f; float b = 1.0f; float a = 1.0f; };
struct Size { double width = 0.0; double height = 0.0; };
struct Point { double x = 0.0; double y = 0.0; };
struct Rect { Point origin; Size size; };
struct Vector3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

} // namespace appgametoolbox
