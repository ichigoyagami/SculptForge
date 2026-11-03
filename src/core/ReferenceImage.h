// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/Image.h"
#include "core/Json.h"
#include <glm/glm.hpp>

namespace sf {
// Screen-space reference, saved inside the project so moving the original file is safe.
struct ReferenceImage {
    Image image;
    std::string source, pngHex;
    bool visible = true, inFront = false, flipX = false;
    float opacity = 0.75f, scale = 1.0f, rotation = 0.0f;
    glm::vec2 offset{0.0f}; // fractions of the canvas, positive Y down
    bool load(const std::string& path, std::string& error);
    bool setImage(Image pixels, std::string path, std::string& error);
    void resetPlacement();
    glm::vec2 fittedSize(float width, float height) const;
    Json toJson() const;
    bool fromJson(const Json& json, std::string& error);
};
} // namespace sf
