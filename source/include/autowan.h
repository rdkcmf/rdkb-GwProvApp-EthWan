/*
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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

#ifndef _GW_GWPROV_AUTOWAN_H_
#define _GW_GWPROV_AUTOWAN_H_
#if defined (_COSA_BCM_ARM_)
#define DOCSIS_INF_NAME "cm0"
#if defined (_XB7_PRODUCT_REQ_)
#define ETHWAN_INF_NAME "eth3"
#elif defined (_CBR2_PRODUCT_REQ_)
#define ETHWAN_INF_NAME "eth5"
#else
#define ETHWAN_INF_NAME "eth0"
#endif
#elif defined (INTEL_PUMA7)
#define ETHWAN_INF_NAME "nsgmii0"
#endif
#define WAN_PHY_NAME "erouter0"
 
void AutoWAN_main();
#endif
