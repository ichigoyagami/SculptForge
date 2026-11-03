// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/Mesh.h"
#include <string>
namespace sf {
class CommandRegistry;
// Keep dot(normal,p)<=offset and cap every closed cut contour, including holes.
Mesh knifePlane(const Mesh& source, glm::vec3 normal, float offset, std::string& error, bool touchedPartsOnly = false);
Mesh slicePlane(const Mesh& source, glm::vec3 normal, float offset, std::string& error);
void registerProfileTools(CommandRegistry& registry);
void registerPlaneTools(CommandRegistry& registry);
}
