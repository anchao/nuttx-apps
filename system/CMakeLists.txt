# ##############################################################################
# apps/system/CMakeLists.txt
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

# add_subdirectory(cdcacm) add_subdirectory(cle) add_subdirectory(composite)
# add_subdirectory(critmon) add_subdirectory(cu) add_subdirectory(dhcpc)
# add_subdirectory(embedlog) add_subdirectory(flash_eraseall)
# add_subdirectory(hex2bin) add_subdirectory(hexed) add_subdirectory(i2c)
# add_subdirectory(lm75) add_subdirectory(lzf) add_subdirectory(mdio)
# add_subdirectory(netdb)
add_subdirectory(nsh)
# add_subdirectory(ntpc) add_subdirectory(nxplayer) add_subdirectory(nxrecorder)
# add_subdirectory(ping) add_subdirectory(ping6) add_subdirectory(popen)
# add_subdirectory(prun) add_subdirectory(ramtest)
add_subdirectory(readline)
# add_subdirectory(sched_note) add_subdirectory(setlogmask)
# add_subdirectory(stackmonitor) add_subdirectory(sudoku)
# add_subdirectory(symtab) add_subdirectory(system) add_subdirectory(taskset)
# add_subdirectory(tee) add_subdirectory(telnet) add_subdirectory(ubloxmodem)
# add_subdirectory(usbmsc) add_subdirectory(vi) add_subdirectory(zmodem)

nuttx_generate_kconfig(MENUDESC "System Libraries and NSH Add-Ons")
