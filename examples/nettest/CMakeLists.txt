# ##############################################################################
# apps/examples/nettest/CMakeLists.txt
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

if(CONFIG_EXAMPLES_NETTEST)

  set(CSRCS)

  # Basic TCP networking test
  if(${CONFIG_EXAMPLES_NETTEST_INIT} MATCHES "y")
    list(APPEND CSRCS nettest_netinit.c)
  endif()

  # Target 1 Files
  if(${CONFIG_EXAMPLES_NETTEST_LOOPBACK} MATCHES "y")
    list(APPEND CSRCS nettest_server.c nettest_client.c)
  elseif(${CONFIG_EXAMPLES_NETTEST_SERVER} MATCHES "y")
    list(APPEND CSRCS nettest_server.c)
  else()
    list(APPEND CSRCS nettest_client.c)
  endif()

  # Target 2
  if(${CONFIG_EXAMPLES_NETTEST_TARGET2} MATCHES "y")
    if(${CONFIG_EXAMPLES_NETTEST_SERVER1} MATCHES "y")
      list(APPEND CSRCS nettest_client.c)
    else()
      list(APPEND CSRCS nettest_server.c)
    endif()

    nuttx_add_application(
      NAME
      nettest
      SOURCES
      nettest_cmdline.c
      nettest_target1.c
      nettest_target2.c
      ${CSRCS})

  endif()

  if(${CONFIG_EXAMPLES_NETTEST_SERVER1} MATCHES "y")

    nuttx_add_application(
      NAME
      tcpserver
      PRIORITY
      100
      STACK
      2048
      SOURCES
      nettest_cmdline.c
      nettest_target1.c
      nettest_target2.c
      ${CSRCS})

  endif()

  nuttx_add_application(
    NAME
    nettest
    SOURCES
    nettest_cmdline.c
    nettest_target1.c
    nettest_target2.c
    ${CSRCS})
endif()
