# ##############################################################################
# apps/netutils/netlib/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

if(CONFIG_NETUTILS_NETLIB)

  set(CSRCS)

  list(APPEND CSRCS netlib_ipv4addrconv.c netlib_ethaddrconv.c
       netlib_parsehttpurl.c)
  list(APPEND CSRCS netlib_setifstatus.c netlib_getifstatus.c)

  # Generic URL parsing support

  if(CONFIG_NETUTILS_NETLIB_GENERICURLPARSER)
    list(APPEND CSRCS netlib_parseurl.c)
  endif()

  # IP address support

  if(CONFIG_NET_IPv4)
    list(APPEND CSRCS netlib_setipv4addr.c netlib_getipv4addr.c)
    list(APPEND CSRCS netlib_setdripv4addr.c netlib_setipv4netmask.c)
    list(APPEND CSRCS netlib_getdripv4addr.c netlib_getipv4netmask.c)
    list(APPEND CSRCS netlib_ipv4adaptor.c)

    if(CONFIG_NET_ARP)
      list(APPEND CSRCS netlib_getarp.c netlib_setarp.c netlib_delarp.c)
      if(CONFIG_NETLINK_ROUTE)
        list(APPEND CSRCS netlib_getarptab.c)
      endif()
    endif()
    if(CONFIG_NETDB_DNSCLIENT)
      list(APPEND CSRCS netlib_setipv4dnsaddr.c)
    endif()
    if(CONFIG_NET_ROUTE)
      list(APPEND CSRCS netlib_ipv4route.c netlib_ipv4router.c)
    endif()
  endif()

  if(CONFIG_NET_IPv6)
    list(APPEND CSRCS netlib_setipv6addr.c netlib_getipv6addr.c)
    list(APPEND CSRCS netlib_setdripv6addr.c netlib_setipv6netmask.c)
    list(APPEND CSRCS netlib_prefix2ipv6netmask.c netlib_ipv6netmask2prefix.c)
    list(APPEND CSRCS netlib_ipv6adaptor.c)

    if(CONFIG_NET_ICMPv6_AUTOCONF)
      list(APPEND CSRCS netlib_autoconfig.c)
    endif()
    if(CONFIG_NET_ROUTE)
      list(APPEND CSRCS netlib_ipv6route.c netlib_ipv6router.c)
    endif()
    if(CONFIG_NETLINK_ROUTE)
      list(APPEND CSRCS netlib_getnbtab.c)
    endif()
  endif()

  if(CONFIG_NETLINK_ROUTE)
    list(APPEND CSRCS netlib_getdevs.c)
    if(CONFIG_NET_ROUTE)
      list(APPEND CSRCS netlib_getroute.c)
    endif()
  endif()

  # These require TCP support

  if(CONFIG_NET_TCP AND CONFIG_NET_IPv4) # Not yet available for IPv6
    list(APPEND CSRCS netlib_server.c netlib_listenon.c)
  endif()

  # These require wireless IOCTL support */

  if(CONFIG_NETDEV_WIRELESS_IOCTL)
    list(APPEND CSRCS netlib_getessid.c netlib_setessid.c)
  endif()

  # MAC address support(Ethernet and 6LoWPAN only)

  if(CONFIG_NET_ETHERNET)
    list(APPEND CSRCS netlib_setmacaddr.c netlib_getmacaddr.c)
  endif()

  if(CONFIG_WIRELESS_IEEE802154)
    list(APPEND CSRCS netlib_seteaddr.c netlib_getpanid.c netlib_saddrconv.c
         netlib_eaddrconv.c)
  endif()

  if(CONFIG_WIRELESS_PKTRADIO)
    list(APPEND CSRCS netlib_getproperties.c netlib_getnodeaddr.c
         netlib_setnodeaddr.c)
    list(APPEND CSRCS netlib_nodeaddrconv.c)
  endif()

  # IGMP support

  if(CONFIG_NET_IGMP AND CONFIG_NET_IPv4) # Not yet available for IPv6
    list(APPEND CSRCS netlib_ipmsfilter.c)
  endif()

  target_sources(apps PRIVATE ${CSRCS})

endif()
