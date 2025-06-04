/****************************************************************************
 *  Copyright (C) 2023 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <uchar.h>

#include <android/binder_manager.h>

bool AParcelUtils_btCommonAllocator(void** data, uint32_t size);
bool AParcelUtils_stringAllocator(void* stringData, int32_t length, char** buffer);
bool AParcelUtils_byteArrayAllocator(void* arrayData, int32_t length, int8_t** outBuffer);
