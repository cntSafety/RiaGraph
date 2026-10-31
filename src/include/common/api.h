#pragma once

// Helpers
#if defined _WIN32 || defined __CYGWIN__
#define RIAG_HELPER_DLL_IMPORT __declspec(dllimport)
#define RIAG_HELPER_DLL_EXPORT __declspec(dllexport)
#define RIAG_HELPER_DLL_LOCAL
#define RIAG_HELPER_DEPRECATED __declspec(deprecated)
#else
#define RIAG_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#define RIAG_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define RIAG_HELPER_DLL_LOCAL __attribute__((visibility("hidden")))
#define RIAG_HELPER_DEPRECATED __attribute__((__deprecated__))
#endif

#ifdef RIAG_STATIC_DEFINE
#define RIAG_API
#else
#ifndef RIAG_API
#ifdef RIAG_EXPORTS
/* We are building this library */
#define RIAG_API RIAG_HELPER_DLL_EXPORT
#else
/* We are using this library */
#define RIAG_API RIAG_HELPER_DLL_IMPORT
#endif
#endif
#endif

#ifndef RIAG_DEPRECATED
#define RIAG_DEPRECATED RIAG_HELPER_DEPRECATED
#endif

#ifndef RIAG_DEPRECATED_EXPORT
#define RIAG_DEPRECATED_EXPORT RIAG_API RIAG_DEPRECATED
#endif
