#ifndef _GW_GWPROV_AUTOWAN_H_
#define _GW_GWPROV_AUTOWAN_H_
#if defined (_COSA_BCM_ARM_)
#define DOCSIS_INF_NAME "cm0"
#if defined (_XB7_PRODUCT_REQ_)
#define ETHWAN_INF_NAME "eth3"
#else
#define ETHWAN_INF_NAME "eth0"
#endif
#elif defined (INTEL_PUMA7)
#define ETHWAN_INF_NAME "nsgmii0"
#endif
#define WAN_PHY_NAME "erouter0"
 
void AutoWAN_main();
#endif
