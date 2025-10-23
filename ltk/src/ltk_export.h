
#ifndef LTK_EXPORT_H
#define LTK_EXPORT_H

#ifdef LTK_STATIC_DEFINE
#define LTK_EXPORT
#define LTK_NO_EXPORT
#else
#ifndef LTK_EXPORT
#ifdef LTK_EXPORTS
/* We are building this library */
#define LTK_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#define LTK_EXPORT __declspec(dllimport)
#    endif
#  endif

#ifndef LTK_NO_EXPORT
#define LTK_NO_EXPORT
#  endif
#endif

#ifndef LTK_DEPRECATED
#define LTK_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef LTK_DEPRECATED_EXPORT
#define LTK_DEPRECATED_EXPORT LTK_EXPORT LTK_DEPRECATED
#endif

#ifndef LTK_DEPRECATED_NO_EXPORT
#define LTK_DEPRECATED_NO_EXPORT LTK_NO_EXPORT LTK_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef LTK_NO_DEPRECATED
#define LTK_NO_DEPRECATED
#  endif
#endif

#endif /* LTK_EXPORT_H */
