#pragma once
namespace rvk::living {
// The recovered engine accepts only a verified cache generated from this AO
// installation. Original resources are never part of the public payload.
inline constexpr bool ReleaseContentAvailable = true;
inline constexpr bool AdditionalAircraftAvailable = true;
inline constexpr const char* UnavailableReason =
    "prepare original assets inside this AO installation; legacy private caches are disabled";
}
