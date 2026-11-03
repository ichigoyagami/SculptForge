// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/ReferenceImage.h"
#include <algorithm>
#include <cmath>

namespace sf {
void ReferenceImage::resetPlacement() { scale = 1.0f; rotation = 0.0f; offset = glm::vec2(0); flipX = false; }
glm::vec2 ReferenceImage::fittedSize(float width, float height) const {
    if (image.empty() || width <= 0 || height <= 0) return glm::vec2(0);
    const float fit = 0.8f * std::min(width / image.width, height / image.height) * scale;
    return glm::vec2(image.width * fit, image.height * fit);
}
bool ReferenceImage::setImage(Image pixels, std::string path, std::string& error) {
    if (pixels.empty()) { error = "The reference image is empty"; return false; }
    pixels = pixels.downscaled(4096);
    std::vector<uint8_t> png;
    if (!pixels.encodePng(png)) { error = "Could not embed the reference image"; return false; }
    std::string hex;
    hex.reserve(png.size() * 2);
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t b : png) { hex += digits[b >> 4]; hex += digits[b & 15]; }
    image = std::move(pixels); source = std::move(path); pngHex = std::move(hex);
    resetPlacement(); visible = true;
    return true;
}
bool ReferenceImage::load(const std::string& path, std::string& error) {
    Image pixels;
    return Image::load(path, pixels, error) && setImage(std::move(pixels), path, error);
}
Json ReferenceImage::toJson() const {
    Json j = Json::object();
    j.set("source", source); j.set("png_hex", pngHex);
    j.set("visible", visible); j.set("in_front", inFront); j.set("flip_x", flipX);
    j.set("opacity", double(opacity)); j.set("scale", double(scale)); j.set("rotation", double(rotation));
    j.set("offset_x", double(offset.x)); j.set("offset_y", double(offset.y));
    return j;
}
bool ReferenceImage::fromJson(const Json& j, std::string& error) {
    ReferenceImage next;
    if (!j.isObject() || j["png_hex"].asString().empty()) { *this = std::move(next); return true; }
    const std::string& hex = j["png_hex"].asString();
    if (hex.size() % 2 || hex.size() > 160u * 1024u * 1024u) { error = "Invalid embedded reference size"; return false; }
    auto nibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    std::vector<uint8_t> png(hex.size() / 2);
    for (size_t i = 0; i < png.size(); ++i) {
        int a = nibble(hex[i * 2]), b = nibble(hex[i * 2 + 1]);
        if (a < 0 || b < 0) { error = "Invalid embedded reference data"; return false; }
        png[i] = uint8_t(a * 16 + b);
    }
    if (!Image::decode(png.data(), png.size(), next.image, error)) return false;
    next.image = next.image.downscaled(4096);
    next.source = j["source"].asString(); next.pngHex = hex;
    next.visible = j["visible"].asBool(true); next.inFront = j["in_front"].asBool(false); next.flipX = j["flip_x"].asBool(false);
    auto bounded = [&](const char* key, float fallback, float lo, float hi) {
        double n = j[key].asNumber(fallback);
        return std::isfinite(n) ? std::clamp(float(n), lo, hi) : fallback;
    };
    next.opacity = bounded("opacity", 0.75f, 0, 1); next.scale = bounded("scale", 1, 0.05f, 4);
    next.rotation = bounded("rotation", 0, -180, 180);
    next.offset = {bounded("offset_x", 0, -2, 2), bounded("offset_y", 0, -2, 2)};
    *this = std::move(next);
    return true;
}
} // namespace sf
