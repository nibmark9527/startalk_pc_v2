#ifndef CUSTOMUI_GLOBAL_H
#define CUSTOMUI_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(CUSTOMUI_LIBRARY)
#  define CUSTOMUISHARED_EXPORT Q_DECL_EXPORT
#else
#  define CUSTOMUISHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // CUSTOMUI_GLOBAL_H