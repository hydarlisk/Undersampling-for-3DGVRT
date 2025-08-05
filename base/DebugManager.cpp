/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 */

#include "DebugManager.hpp"

void DebugManager::prepare(VkInstance instance, VkDevice* device) {
	vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
	this->device = device;
}