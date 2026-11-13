#pragma once
// Compatibility types for the renderer interface. Population implementation,
// asset loading, routes and preparation belong to the separate Living add-on.
namespace rvk::living {
struct Scene {};
inline constexpr bool ReleaseContentAvailable = false;
inline constexpr bool AdditionalAircraftAvailable = false;
inline constexpr const char* UnavailableReason = "Living is a separate optional add-on";
}
