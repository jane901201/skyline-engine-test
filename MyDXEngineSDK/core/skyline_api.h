#pragma once

#if defined(_WIN32) && !defined(MYDXENGINESDK_STATIC)
#if defined(MYDXENGINESDK_BUILD)
#define SKYLINE_API __declspec(dllexport)
#else
#define SKYLINE_API __declspec(dllimport)
#endif
#else
#define SKYLINE_API
#endif
