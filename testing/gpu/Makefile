############################################################################
# aapps/testing/gpu/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

PROGNAME = gpu_test
PRIORITY = $(CONFIG_TESTING_GPU_PRIORITY)
STACKSIZE = $(CONFIG_TESTING_GPU_STACKSIZE)
MODULE = $(CONFIG_TESTING_GPU)

MAINSRC = gpu_main.c

ifneq ($(CONFIG_TESTING_GPU_VG_LITE_INCLUDE), "")
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../$(CONFIG_TESTING_GPU_VG_LITE_INCLUDE)
endif

ifneq ($(CONFIG_LIB_PNG),)
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../external/libpng
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/../external/libpng/libpng
endif

CSRCS += $(wildcard vg_lite/*.c)
CSRCS += $(filter-out gpu_main.c, $(wildcard *.c))

include $(APPDIR)/Application.mk
