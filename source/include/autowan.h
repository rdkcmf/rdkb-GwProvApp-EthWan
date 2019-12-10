#ifndef _GW_GWPROV_AUTOWAN_H_
#define _GW_GWPROV_AUTOWAN_H_
#ifdef _XB7_PRODUCT_REQ_
#define ETHWAN_INF_NAME "eth3"
#else
#define ETHWAN_INF_NAME "eth0"
#endif
#define DOCSIS_INF_NAME "cm0"
 
void AutoWAN_main();
#endif
