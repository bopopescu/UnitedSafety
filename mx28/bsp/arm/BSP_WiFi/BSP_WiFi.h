#ifndef BSP_WIFI_H
#define BSP_WIFI_H

#include "bsp_global.h"
#include "QBSP_WiFi.h"

class BSPSHARED_EXPORT BSP_WiFi {
public:
    BSP_WiFi();

	nsWIFI::QBsp *m_bsp;
};

#endif // BSP_WIFI_H
