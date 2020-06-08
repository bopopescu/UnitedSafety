#ifndef BSP_H
#define BSP_H

#include "bsp_global.h"

class QBsp;

class BSPSHARED_EXPORT Bsp {
public:
    Bsp();

	QBsp *m_bsp;
};

#endif // BSP_H
