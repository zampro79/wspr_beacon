#pragma once

#if defined(WSPR_PROFILE_HISTORICAL)
  #if defined(WSPR_PROFILE_DEVELOPMENT) || defined(WSPR_PROFILE_PUBLIC)
    #error "Historical builds cannot also select a DEVELOPMENT or PUBLIC profile"
  #endif
#elif defined(WSPR_PROFILE_DEVELOPMENT) == defined(WSPR_PROFILE_PUBLIC)
  #error "Define exactly one of WSPR_PROFILE_DEVELOPMENT or WSPR_PROFILE_PUBLIC"
#endif

enum class WsprBuildProfile : unsigned char {
  DEVELOPMENT,
  PUBLIC,
  HISTORICAL
};

#if defined(WSPR_PROFILE_DEVELOPMENT)
constexpr WsprBuildProfile WSPR_BUILD_PROFILE = WsprBuildProfile::DEVELOPMENT;
constexpr const char WSPR_BUILD_PROFILE_NAME[] = "DEVELOPMENT";
  #define WSPR_DEFAULT_CALLSIGN_LITERAL "IW2NNV"
#elif defined(WSPR_PROFILE_PUBLIC)
constexpr WsprBuildProfile WSPR_BUILD_PROFILE = WsprBuildProfile::PUBLIC;
constexpr const char WSPR_BUILD_PROFILE_NAME[] = "PUBLIC";
  #define WSPR_DEFAULT_CALLSIGN_LITERAL ""
#else
constexpr WsprBuildProfile WSPR_BUILD_PROFILE = WsprBuildProfile::HISTORICAL;
constexpr const char WSPR_BUILD_PROFILE_NAME[] = "HISTORICAL";
  #define WSPR_DEFAULT_CALLSIGN_LITERAL "IW2NNV"
#endif

#define WSPR_DEFAULT_LOCATOR_LITERAL "JN45"

constexpr const char WSPR_DEFAULT_CALLSIGN[] = WSPR_DEFAULT_CALLSIGN_LITERAL;
constexpr const char WSPR_DEFAULT_LOCATOR[] = WSPR_DEFAULT_LOCATOR_LITERAL;
