// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/Mesh.h"
#include <string>
namespace sf {
class CommandRegistry;
uint64_t meshTopologyKey(const Mesh& mesh);
bool hasMorphTarget(const Mesh& mesh);
Mesh closeMeshHoles(const Mesh& mesh, std::string& error);
Mesh groupLoops(const Mesh& mesh, float inset, int loops, std::string& error);
Mesh panelLoops(const Mesh& mesh, float thickness, float inset, int loops, std::string& error);
void registerStudioTools(CommandRegistry& registry);
} // namespace sf
