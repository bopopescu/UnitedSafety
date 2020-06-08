#ifndef BSP_GLOBAL_H
#define BSP_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(BSP_LIBRARY)
#  define BSPSHARED_EXPORT Q_DECL_EXPORT
#else
#  define BSPSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // BSP_GLOBAL_H
