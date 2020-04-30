/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/*#######################################################################
#   Copyright [2015] [ARRIS Corporation]
#
#   Licensed under the Apache License, Version 2.0 (the \"License\");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an \"AS IS\" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#######################################################################*/

/** @file gw_prov_epon.h
 *  @brief This file defines the apis which are called
 *    by the gateway provisioning application during initialization.
 */
 
#ifndef _GW_GWPROV_ETHWAN_H_
#define _GW_GWPROV_ETHWAN_H_
#include<stdint.h>
#if 0	
/* 
* Define callback function pointers which needs to be called
* from provisioning abstraction layer when any provisioning
* event occurs.
*/
typedef void ( *fpEthWanLink_Up ) ( ) ; 	   /* RDKB expects this callback once EthWan link is UP */
typedef void ( *fpEthWanLink_Down ) ( ) ;  /* RDKB expects this callback once EthWan link is Down */

/*! \var typedef struct __appCallBack 
*	\brief struct of pointers to the function pointers of callback functions.
*/

typedef struct __appCallBack
{
	fpEthWanLink_Up 		   pGWP_act_EthWanLinkUP;
	fpEthWanLink_Down	   pGWP_act_EthWanLinkDown;
}appCallBack;
#endif
#ifdef AUTOWAN_ENABLE
#define INFO  0
#define WARNING  1
#define ERROR 2

#define DEBUG_INI_NAME  "/etc/debug.ini"
#define COMP_NAME "LOG.RDK.GWPROVETHWAN"
#define LOG_INFO 4

#ifdef FEATURE_SUPPORT_RDKLOG
#define GWPROVETHWANLOG(fmt ...)    {\
                                    char log_buff[1024];\
				    				snprintf(log_buff, 1023, fmt);\
                                    RDK_LOG(LOG_INFO, COMP_NAME, "%s", log_buff);\
                                    fflush(stdout);\
                                 }
#else
#define GWPROVETHWANLOG printf
#endif
#endif

typedef struct
{
  uint8_t  hw[6];
} macaddr_t;
void startWebUIProcess();
typedef unsigned char       Uint8;

void getWanMacAddress(macaddr_t* macAddr);
#endif
