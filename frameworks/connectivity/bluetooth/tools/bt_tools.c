/****************************************************************************
 *  Copyright (C) 2022 Xiaomi Corporation
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
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__NuttX__)
#include <system/readline.h>
#endif

#include "bluetooth.h"
#include "bt_adapter.h"
#include "bt_tools.h"
#include "utils.h"

#define CONFIG_BLUETOOTH_TOOL_THREAD_STACK_SIZE 8192
#define CONFIG_BLUETOOTH_TOOL_THREAD_PRIORITY   103

static void usage(void);
static int usage_cmd(void* handle, int argc, char** argv);
static int enable_cmd(void* handle, int argc, char** argv);
static int disable_cmd(void* handle, int argc, char** argv);
static int discovery_cmd(void* handle, int argc, char** argv);
static int get_state_cmd(void* handle, int argc, char** argv);
static int set_adapter_cmd(void* handle, int argc, char** argv);
static int get_adapter_cmd(void* handle, int argc, char** argv);
static int set_scanmode_cmd(void* handle, int argc, char** argv);
static int get_scanmode_cmd(void* handle, int argc, char** argv);
static int set_iocap_cmd(void* handle, int argc, char** argv);
static int get_iocap_cmd(void* handle, int argc, char** argv);
static int get_local_addr_cmd(void* handle, int argc, char** argv);
static int get_appearance_cmd(void* handle, int argc, char** argv);
static int set_appearance_cmd(void* handle, int argc, char** argv);
static int set_le_addr_cmd(void* handle, int argc, char** argv);
static int get_le_addr_cmd(void* handle, int argc, char** argv);
static int set_identity_addr_cmd(void* handle, int argc, char** argv);
static int set_scan_parameters_cmd(void* handle, int argc, char** argv);
static int get_local_name_cmd(void* handle, int argc, char** argv);
static int set_local_name_cmd(void* handle, int argc, char** argv);
static int get_local_cod_cmd(void* handle, int argc, char** argv);
static int set_local_cod_cmd(void* handle, int argc, char** argv);
static int pair_cmd(void* handle, int argc, char** argv);
static int pair_set_auto_cmd(void* handle, int argc, char** argv);
static int pair_reply_cmd(void* handle, int argc, char** argv);
static int pair_set_pincode_cmd(void* handle, int argc, char** argv);
static int pair_set_passkey_cmd(void* handle, int argc, char** argv);
static int pair_set_confirm_cmd(void* handle, int argc, char** argv);
static int pair_set_tk_cmd(void* handle, int argc, char** argv);
static int pair_set_oob_cmd(void* handle, int argc, char** argv);
static int pair_get_oob_cmd(void* handle, int argc, char** argv);
static int connect_cmd(void* handle, int argc, char** argv);
static int disconnect_cmd(void* handle, int argc, char** argv);
static int le_connect_cmd(void* handle, int argc, char** argv);
static int le_disconnect_cmd(void* handle, int argc, char** argv);
static int create_bond_cmd(void* handle, int argc, char** argv);
static int cancel_bond_cmd(void* handle, int argc, char** argv);
static int remove_bond_cmd(void* handle, int argc, char** argv);
static int device_show_cmd(void* handle, int argc, char** argv);
static int device_set_alias_cmd(void* handle, int argc, char** argv);
static int get_bonded_devices_cmd(void* handle, int argc, char** argv);
static int get_connected_devices_cmd(void* handle, int argc, char** argv);
static int search_cmd(void* handle, int argc, char** argv);
static int start_service_cmd(void* handle, int argc, char** argv);
static int stop_service_cmd(void* handle, int argc, char** argv);
static int set_phy_cmd(void* handle, int argc, char** argv);
static int dump_cmd(void* handle, int argc, char** argv);
static int quit_cmd(void* handle, int argc, char** argv);

bt_instance_t* g_bttool_ins = NULL;
static void* adapter_callback = NULL;
static bool g_cmd_had_inited = false;
bool g_auto_accept_pair = true;
bond_state_t g_bond_state = BOND_STATE_NONE;

static struct {
    int cmd_err_code;
    const char* cmd_err_code_desc;
} cmd_err_map[] = {
    { CMD_OK, "OK" },
    { CMD_INVALID_PARAM, "Invalid Parameter" },
    { CMD_INVALID_OPT, "Invalid Option" },
    { CMD_INVALID_ADDR, "Invalid Address" },
    { CMD_PARAM_NOT_ENOUGH, "Parameter Not Enough" },
    { CMD_UNKNOWN, "Unknown Command" },
    { CMD_USAGE_FAULT, "Command Usage Fault" },
    { CMD_ERROR, "API Return Error" },
};

static struct option main_options[] = {
    { "async", 0, 0, 'a' },
    { "help", 0, 0, 'h' },
    { "version", 0, 0, 'v' },
    { 0, 0, 0, 0 }
};

static struct option le_conn_options[] = {
    { "addr", required_argument, 0, 'a' },
    { "type", required_argument, 0, 't' },
    { "defaults", no_argument, 0, 'd' },
    { "filter", required_argument, 0, 'f' },
    { "phy", required_argument, 0, 'p' },
    { "latency", required_argument, 0, 'l' },
    { "conn_interval_min", required_argument, 0, 0 },
    { "conn_interval_max", required_argument, 0, 0 },
    { "timeout", required_argument, 0, 'T' },
    { "scan_interval", required_argument, 0, 0 },
    { "scan_window", required_argument, 0, 0 },
    { "min_ce_length", required_argument, 0, 0 },
    { "max_ce_length", required_argument, 0, 0 },
    { "help", no_argument, 0, 'h' },
    { 0, 0, 0, 0 }
};

#define LE_CONN_USAGE "\n"                                                                                                      \
                      "\t -a or --addr, peer le device address\n"                                                               \
                      "\t -t or --type, peer le device address type, address type(0:public,1:random,2:public_id,3:random_id)\n" \
                      "\t -d or --default, use default parameter\n"                                                             \
                      "\t -f or --filter, connection filter policy, (0:addr,1:whitelist)\n"                                     \
                      "\t -p or --phy, init phy type, (0:1M,1:2M,2:Coded)\n"                                                    \
                      "\t -l or --latency, connection latency Range: 0x0000 to 0x01F3\n"                                        \
                      "\t --conn_interval_min, Range: 0x0006 to 0x0C80\n"                                                       \
                      "\t --conn_interval_max, Range: 0x0006 to 0x0C80\n"                                                       \
                      "\t -T or --timeout, supervision timeout Range: 0x000A to 0x0C80\n"                                       \
                      "\t --scan_interval, Range: 0x0004 to 0x4000\n"                                                           \
                      "\t --scan_window, Range: 0x0004 to 0x4000\n"                                                             \
                      "\t --min_ce_length, Range: 0x0000 to 0xFFFF\n"                                                           \
                      "\t --max_ce_length, Range: 0x0000 to 0xFFFF\n"

#define INQUIRY_USAGE "inquiry device\n"                                          \
                      "\t\t\t- start <timeout>(Range: 1-48, i.e., 1.28-61.44s)\n" \
                      "\t\t\t- stop"

#define SET_LE_PHY_USAGE "set le tx and rx phy, params: <addr><txphy><rxphy>(0:1M, 1:2M, 2:CODED)"

static bt_command_t g_cmd_tables[] = {
    { "enable", enable_cmd, 0, "enable stack" },
    { "disable", disable_cmd, 0, "disable stack" },
    { "state", get_state_cmd, 0, "get adapter state" },
    { "inquiry", discovery_cmd, 0, INQUIRY_USAGE },
    { "set", set_adapter_cmd, 0, "set adapter information, input \'set help\' show usage" },
    { "get", get_adapter_cmd, 0, "get adapter information, input \'get help\' show usage" },
    { "pair", pair_cmd, 0, "reply pair request, input \'pair help\' show usage" },
    { "connect", connect_cmd, 0, "connect classic peer device, params: <addr>" },
    { "disconnect", disconnect_cmd, 0, "disconnect peer device, params: <addr>" },
    { "leconnect", le_connect_cmd, 1, "connect le peer device, input \'leconnect -h\' show usage" },
    { "ledisconnect", le_disconnect_cmd, 0, "disconnect le peer device, params: <addr>" },
    { "createbond", create_bond_cmd, 0, "create bond, params: <addr> <transport>(0:BLE, 1:BREDR)" },
    { "cancelbond", cancel_bond_cmd, 0, "cancel bond, params: <addr>" },
    { "removebond", remove_bond_cmd, 0, "remove bond, params: <addr> <transport>(0:BLE, 1:BREDR)" },
    { "setalias", device_set_alias_cmd, 0, "set device alias, params: <addr>" },
    { "device", device_show_cmd, 0, "show device information, params: <addr>" },
    { "search", search_cmd, 0, "service serach <addr>, Not implemented" },
    { "start", start_service_cmd, 0, "start profile service, Not implemented" },
    { "stop", stop_service_cmd, 0, "stop profile service,  Not implemented" },
    { "setphy", set_phy_cmd, 0, SET_LE_PHY_USAGE },
#ifdef CONFIG_BLUETOOTH_BLE_ADV
    { "adv", adv_command_exec, 0, "advertising cmd,   input \'adv\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_BLE_SCAN
    { "scan", scan_command_exec, 0, "scan cmd,          input \'scan\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SINK
    { "a2dpsnk", a2dp_sink_command_exec, 0, "a2dp sink cmd,    input \'a2dpsnk\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SOURCE
    { "a2dpsrc", a2dp_src_command_exec, 0, "a2dp source cmd,    input \'a2dpsrc\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_AVRCP_CONTROL
    { "avrcpct", avrcp_control_command_exec, 0, "avrcp control cmd,    input \'avrcpct\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_HFP_HF
    { "hf", hfp_hf_command_exec, 0, "hands-free cmd,    input \'hf\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_HFP_AG
    { "ag", hfp_ag_command_exec, 0, "audio-gateway cmd, input \'ag\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_SPP
    { "spp", spp_command_exec, 0, "serial port cmd,   input \'spp\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_HID_DEVICE
    { "hidd", hidd_command_exec, 0, "hid device cmd,    input \'hidd\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_PAN
    { "pan", pan_command_exec, 0, "pan cmd,           input \'pan\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_GATT
    { "gattc", gattc_command_exec, 0, "gatt client cmd    input \'gattc\' show usage" },
    { "gatts", gatts_command_exec, 0, "gatt server cmd    input \'gatts\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_SERVER
    { "leas", leas_command_exec, 0, "lea server cmd, input \'leas\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_MCP
    { "mcp", lea_mcp_command_exec, 0, "leaudio mcp cmd,  input \'mcp\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_CCP
    { "ccp", lea_ccp_command_exec, 0, "lea ccp cmd, input \'ccp\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_VMICS
    { "vmics", vmics_command_exec, 0, "vcp/micp server cmd, input \'vmics\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_CLIENT
    { "leac", leac_command_exec, 0, "lea client cmd, input \'leac\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_MCS
    { "mcs", lea_mcs_command_exec, 0, "leaudio mcp cmd,  input \'mcs\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_TBS
    { "tbs", lea_tbs_command_exec, 0, "lea tbs cmd, input \'tbs\' show usage" },
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_VMICP
    { "vmicp", vmicp_command_exec, 0, "vcp/micp client cmd, input \'vmicp\' show usage" },
#endif
    { "dump", dump_cmd, 0, "dump adapter state" },
    { "log", log_command, 0, "log control command" },
    { "help", usage_cmd, 0, "Usage for bttools" },
    { "quit", quit_cmd, 0, "Quit" },
    { "q", quit_cmd, 0, "Quit" },
};

#define SET_IOCAP_USAGE "params: <io capability> (0:displayonly, 1:yes&no, 2:keyboardonly, 3:no-in/no-out 4:keyboard&display)"
#define SET_CLASS_USAGE "params: <local class of device>, range in 0x0-0xFFFFFC, the 2 least significant shall be 0b00, example: 0x00640404"
#define SET_SCANPARAMS_USAGE "set scan parameters, params: <mode>(0: INQUIRY, 1: PAGE), <type>(0: standard, 1: interlaced), <interval>(range in 18-4096), <window>(range in 17-4096)"

static bt_command_t g_set_cmd_tables[] = {
    { "scanmode", set_scanmode_cmd, 0, "params: <scan mode> (0:none, 1:connectable 2:connectable&discoverable)" },
    { "iocap", set_iocap_cmd, 0, SET_IOCAP_USAGE },
    { "name", set_local_name_cmd, 0, "params: <local name>, example \"vela-bt\"" },
    { "class", set_local_cod_cmd, 0, SET_CLASS_USAGE },
    { "appearance", set_appearance_cmd, 0, "set le adapter appearance, params: <appearance>" },
    { "leaddr", set_le_addr_cmd, 0, "set ble adapter addr, params: <leaddr>" },
    { "id", set_identity_addr_cmd, 0, "set ble identity addr, params: <identity addr> <addr type>" },
    { "scanparams", set_scan_parameters_cmd, 0, SET_SCANPARAMS_USAGE },
    { "help", NULL, 0, "show set help info" },
    //{ "", , "set " },
};

static bt_command_t g_get_cmd_tables[] = {
    { "scanmode", get_scanmode_cmd, 0, "get adapter scan mode" },
    { "iocap", get_iocap_cmd, 0, "get adapter io capability" },
    { "addr", get_local_addr_cmd, 0, "get adapter local addr" },
    { "leaddr", get_le_addr_cmd, 0, "get ble adapter addr" },
    { "name", get_local_name_cmd, 0, "get adapter local name" },
    { "appearance", get_appearance_cmd, 0, "get le adapter appearance" },
    { "class", get_local_cod_cmd, 0, "get adapter local class of device" },
    { "bonded", get_bonded_devices_cmd, 0, "get bonded devices, params:<transport>(0:BLE, 1:BREDR)" },
    { "connected", get_connected_devices_cmd, 0, "get connected devices params:<transport>(0:BLE, 1:BREDR)" },
    { "help", NULL, 0, "show get help info" },
    //{ "", , "get " },
};

#define PAIR_PASSKEY_USAGE "input ssp passkey, params: <addr> <transport>(0:BLE, 1:BREDR)<reply>(0 :reject, 1: accept)<passkey>"
#define PAIR_CONFIRM_USAGE "set ssp confirmation, params: <addr> <transport> (0:BLE, 1:BREDR)<conform>(0 :reject, 1: accept)"

static bt_command_t g_pair_cmd_tables[] = {
    { "auto", pair_set_auto_cmd, 0, "enable pair auto reply, params: <enable>(0:disable, 1:enable)" },
    { "reply", pair_reply_cmd, 0, "reply the pair request, params: <addr><accept?>(0 :reject, 1: accept)" },
    { "pin", pair_set_pincode_cmd, 0, "input pin code, params: <addr><accept?>(0 :reject, 1: accept)<pincode>" },
    { "passkey", pair_set_passkey_cmd, 0, PAIR_PASSKEY_USAGE },
    { "confirm", pair_set_confirm_cmd, 0, PAIR_CONFIRM_USAGE },
    { "set_tk", pair_set_tk_cmd, 0, "set oob temporary key for le legacy pairing: <addr><tk_val>" },
    { "set_oob", pair_set_oob_cmd, 0, "set remote oob data for le sc pairing: <addr><c_val><r_val>" },
    { "get_oob", pair_get_oob_cmd, 0, "get local oob data for le sc pairing: <addr>" },
    { "help", NULL, 0, "show pair help info" },
    //{ "", , "set " },
};

static void bt_tool_init(void* handle)
{
#ifdef CONFIG_BLUETOOTH_BLE_SCAN
    scan_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SINK
    a2dp_sink_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SOURCE
    a2dp_src_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_AVRCP_CONTROL
    avrcp_control_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_HF
    hfp_hf_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_AG
    hfp_ag_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_SPP
    spp_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HID_DEVICE
    hidd_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_PAN
    pan_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_GATT
    gattc_command_init(handle);
    gatts_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_SERVER
    leas_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_CLIENT
    leac_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_MCP
    lea_mcp_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_MCS
    lea_mcs_commond_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_CCP
    lea_ccp_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_TBS
    lea_tbs_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_VMICS
    lea_vmics_command_init(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_VMICP
    lea_vmicp_command_init(handle);
#endif
    g_cmd_had_inited = true;
}

static void bt_tool_uninit(void* handle)
{
    if (!g_cmd_had_inited)
        return;

#ifdef CONFIG_BLUETOOTH_BLE_SCAN
    scan_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SINK
    a2dp_sink_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_A2DP_SOURCE
    a2dp_src_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_AVRCP_CONTROL
    avrcp_control_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_HF
    hfp_hf_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HFP_AG
    hfp_ag_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_SPP
    spp_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_HID_DEVICE
    hidd_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_PAN
    pan_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_GATT
    gattc_command_uninit(handle);
    gatts_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_SERVER
    leas_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_MCP
    lea_mcp_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_MCS
    lea_mcs_commond_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_CCP
    lea_ccp_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_TBS
    lea_tbs_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_VMICS
    lea_vmics_command_uninit(handle);
#endif
#ifdef CONFIG_BLUETOOTH_LEAUDIO_VMICP
    lea_vmicp_command_uninit(handle);
#endif
    g_cmd_had_inited = false;
}

static const char* cmd_err_str(int err_code)
{
    for (int i = 0; i < ARRAY_SIZE(cmd_err_map); i++) {
        if (cmd_err_map[i].cmd_err_code == err_code)
            return cmd_err_map[i].cmd_err_code_desc;
    }

    return "Correct code ?";
}

static int enable_cmd(void* handle, int argc, char** argv)
{
    bt_adapter_enable(handle);
    return CMD_OK;
}

static int disable_cmd(void* handle, int argc, char** argv)
{
    bt_adapter_disable(handle);
    return CMD_OK;
}

static int get_state_cmd(void* handle, int argc, char** argv)
{
    PRINT("Adapter State: %d", bt_adapter_get_state(handle));
    return CMD_OK;
}

static int discovery_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    if (!strcmp(argv[0], "start")) {
        if (argc < 2)
            return CMD_PARAM_NOT_ENOUGH;

        int timeout = atoi(argv[1]);
        if (timeout <= 0 || timeout > 48) {
            PRINT("%s, invalid timeout value:%d", __func__, timeout);
            return CMD_INVALID_PARAM;
        }

        PRINT("start discovery timeout:%d", timeout);
        if (bt_adapter_start_discovery(handle, timeout) != BT_STATUS_SUCCESS)
            return CMD_ERROR;
    } else if (!strcmp(argv[0], "stop")) {
        if (bt_adapter_cancel_discovery(handle) != BT_STATUS_SUCCESS)
            return CMD_ERROR;
    } else {
        return CMD_USAGE_FAULT;
    }

    return CMD_OK;
}

static void set_usage(void)
{
    printf("Usage:\n"
           "\tset [options] <command> [command parameters]\n");
    printf("Options:\n"
           "\t--help\tDisplay help\n");
    printf("Commands:\n");
    for (int i = 0; i < ARRAY_SIZE(g_set_cmd_tables); i++) {
        printf("\t%-8s\t%s\n", g_set_cmd_tables[i].cmd, g_set_cmd_tables[i].help);
    }
    printf("\n"
           "For more information on the usage of each command use:\n"
           "\tset help\n");
}

static void get_usage(void)
{
    printf("Usage:\n"
           "\tget [options] <command> [command parameters]\n");
    printf("Options:\n"
           "\t--help\tDisplay help\n");
    printf("Commands:\n");
    for (int i = 0; i < ARRAY_SIZE(g_get_cmd_tables); i++) {
        printf("\t%-8s\t%s\n", g_get_cmd_tables[i].cmd, g_get_cmd_tables[i].help);
    }
    printf("\n"
           "For more information on the usage of each command use:\n"
           "\tget help\n");
}

static void pair_usage(void)
{
    printf("Usage:\n"
           "\tpair [options] <command> [command parameters]\n");
    printf("Options:\n"
           "\t--help\tDisplay help\n");
    printf("Commands:\n");
    for (int i = 0; i < ARRAY_SIZE(g_pair_cmd_tables); i++) {
        printf("\t%-8s\t%s\n", g_pair_cmd_tables[i].cmd, g_pair_cmd_tables[i].help);
    }
    printf("\n"
           "For more information on the usage of each command use:\n"
           "\tpair help\n");
}

static int set_adapter_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1) {
        set_usage();
        return CMD_PARAM_NOT_ENOUGH;
    }

    int ret = execute_command_in_table(handle, g_set_cmd_tables, ARRAY_SIZE(g_set_cmd_tables), argc, argv);
    if (ret != CMD_OK)
        set_usage();

    return ret;
}

static int get_adapter_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1) {
        get_usage();
        return CMD_PARAM_NOT_ENOUGH;
    }

    int ret = execute_command_in_table(handle, g_get_cmd_tables, ARRAY_SIZE(g_get_cmd_tables), argc, argv);
    if (ret != CMD_OK)
        get_usage();

    return ret;
}

static int set_scanmode_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    int scanmode = atoi(argv[0]);
    if (scanmode > BT_BR_SCAN_MODE_CONNECTABLE_DISCOVERABLE)
        return CMD_INVALID_PARAM;

    if (bt_adapter_set_scan_mode(handle, scanmode, 1) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Scan Mode:%d set success", scanmode);
    return CMD_OK;
}

static int get_scanmode_cmd(void* handle, int argc, char** argv)
{
    PRINT("Scan Mode:%d", bt_adapter_get_scan_mode(handle));
    return CMD_OK;
}

static int set_iocap_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    if (strlen(argv[0]) > 1) {
        return CMD_INVALID_PARAM;
    }

    int iocap = *argv[0] - '0';
    if (iocap < BT_IO_CAPABILITY_DISPLAYONLY || iocap > BT_IO_CAPABILITY_KEYBOARDDISPLAY)
        return CMD_INVALID_PARAM;

    if (bt_adapter_set_io_capability(handle, iocap) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("IO Capability:%d set success", iocap);
    return CMD_OK;
}

static int get_iocap_cmd(void* handle, int argc, char** argv)
{
    PRINT("IO Capability:%d", bt_adapter_get_io_capability(handle));
    return CMD_OK;
}

static int get_local_addr_cmd(void* handle, int argc, char** argv)
{
    bt_address_t addr;

    bt_adapter_get_address(handle, &addr);
    PRINT_ADDR("Local Address:[%s]", &addr);
    return CMD_OK;
}

static int get_appearance_cmd(void* handle, int argc, char** argv)
{
    uint16_t appearance;

    appearance = bt_adapter_get_le_appearance(handle);
    PRINT("Le appearance:0x%04x", appearance);
    return CMD_OK;
}

static int set_appearance_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    uint32_t appearance = strtoul(argv[0], NULL, 16);
    bt_adapter_set_le_appearance(handle, appearance);
    PRINT("Set Le appearance:0x%04" PRIx32 "", appearance);

    return CMD_OK;
}

static int set_le_addr_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    bt_adapter_set_le_address(handle, &addr);

    return CMD_OK;
}

static int get_le_addr_cmd(void* handle, int argc, char** argv)
{
    bt_address_t addr;
    ble_addr_type_t type;

    bt_adapter_get_le_address(handle, &addr, &type);
    PRINT_ADDR("LE Address:%s, type:%d", &addr, type);

    return CMD_OK;
}

static int set_identity_addr_cmd(void* handle, int argc, char** argv)
{
    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int type = atoi(argv[1]);
    if (type != 0 && type != 1) {
        return CMD_INVALID_PARAM;
    }

    bt_adapter_set_le_identity_address(handle, &addr, type);

    return CMD_OK;
}

static int set_scan_parameters_cmd(void* handle, int argc, char** argv)
{
    if (argc < 4)
        return CMD_PARAM_NOT_ENOUGH;

    int is_page = atoi(argv[0]);
    if (is_page != 0 && is_page != 1)
        return CMD_INVALID_PARAM;

    int type = atoi(argv[1]);
    if (type != 0 && type != 1)
        return CMD_INVALID_PARAM;

    int interval = atoi(argv[2]);
    if (interval < 0x12 || interval > 0x1000)
        return CMD_INVALID_PARAM;

    int window = atoi(argv[3]);
    if (window < 0x11 || window > 0x1000)
        return CMD_INVALID_PARAM;

    if (!is_page)
        bt_adapter_set_inquiry_scan_parameters(handle, type, interval, window);
    else
        bt_adapter_set_page_scan_parameters(handle, type, interval, window);

    return CMD_OK;
}

static int get_local_name_cmd(void* handle, int argc, char** argv)
{
    char name[64 + 1];

    bt_adapter_get_name(handle, name, 64);
    PRINT("Local Name:%s", name);

    return CMD_OK;
}

static int set_local_name_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    char* name = argv[0];
    if (strlen(name) > 63) {
        PRINT("name length to long");
        return CMD_INVALID_PARAM;
    }

    if (bt_adapter_set_name(handle, name) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Local Name:%s set success", name);
    return CMD_OK;
}

static int get_local_cod_cmd(void* handle, int argc, char** argv)
{
    uint32_t cod = bt_adapter_get_device_class(handle);
    PRINT("Local class of device: 0x%08" PRIx32 ", is HEADSET: %s", cod, IS_HEADSET(cod) ? "true" : "false");
    return CMD_OK;
}

static int set_local_cod_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    uint32_t cod = strtol(argv[0], NULL, 16);

    if (cod > 0xFFFFFF || cod & 0x3)
        return CMD_INVALID_PARAM;

    if (bt_adapter_set_device_class(handle, cod) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Local class of device:0x%08" PRIx32 " set success", cod);
    return CMD_OK;
}

static int pair_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1) {
        pair_usage();
        return CMD_PARAM_NOT_ENOUGH;
    }

    int ret = execute_command_in_table(handle, g_pair_cmd_tables, ARRAY_SIZE(g_pair_cmd_tables), argc, argv);
    if (ret != CMD_OK)
        pair_usage();

    return ret;
}

static int pair_set_auto_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;
    if (strlen(argv[0]) > 1) {
        return CMD_INVALID_PARAM;
    }
    switch (*argv[0]) {
    case '0':
        g_auto_accept_pair = false;
        break;
    case '1':
        g_auto_accept_pair = true;
        break;
    default:
        return CMD_INVALID_PARAM;
        break;
    }

    PRINT("Auto accept pair:%s", g_auto_accept_pair ? "Enable" : "Disable");

    return CMD_OK;
}

static int pair_reply_cmd(void* handle, int argc, char** argv)
{
    bt_address_t addr;

    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int reply = atoi(argv[1]);
    if (reply != 0 && reply != 1)
        return CMD_INVALID_PARAM;

    /* TODO: Check bond state*/
    if (bt_device_pair_request_reply(handle, &addr, reply) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] pair request %s", argv[0], reply ? "Accept" : "Reject");
    return CMD_OK;
}

static int pair_set_pincode_cmd(void* handle, int argc, char** argv)
{
    bt_address_t addr;
    char* pincode = NULL;
    uint8_t pincode_len = 0;

    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int reply = atoi(argv[1]);
    if (reply != 0 && reply != 1)
        return CMD_INVALID_PARAM;

    if (reply) {
        if (argc < 3)
            return CMD_PARAM_NOT_ENOUGH;
        pincode = argv[2];
        pincode_len = strlen(pincode);
    }

    /* TODO: Check bond state*/
    if (bt_device_set_pin_code(handle, &addr, reply, pincode, pincode_len) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] pincode request %s, code:%s", argv[0], reply ? "Accept" : "Reject", pincode);
    return CMD_OK;
}

static int pair_set_passkey_cmd(void* handle, int argc, char** argv)
{
    bt_address_t addr;
    int passkey = 0;

    if (argc < 3)
        return CMD_PARAM_NOT_ENOUGH;

    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int transport = atoi(argv[1]);
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE)
        return CMD_INVALID_PARAM;

    int reply = atoi(argv[2]);
    if (reply != 0 && reply != 1)
        return CMD_INVALID_PARAM;

    if (reply) {
        if (argc < 4)
            return CMD_PARAM_NOT_ENOUGH;

        char tmp[7] = { 0 };
        strncpy(tmp, argv[3], 6);
        passkey = atoi(tmp);
        if (passkey > 1000000) {
            PRINT("Invalid passkey");
            return CMD_INVALID_PARAM;
        }
    }

    /* TODO: Check bond state*/
    if (bt_device_set_pass_key(handle, &addr, transport, reply, passkey) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] passkey request %s, passkey:%d", argv[0], reply ? "Accept" : "Reject", passkey);
    return CMD_OK;
}

static int pair_set_confirm_cmd(void* handle, int argc, char** argv)
{
    if (argc < 3)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int transport = atoi(argv[1]);
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE)
        return CMD_INVALID_PARAM;

    int reply = atoi(argv[2]);
    if (reply != 0 && reply != 1)
        return CMD_INVALID_PARAM;

    /* TODO: Check bond state*/
    if (bt_device_set_pairing_confirmation(handle, &addr, transport, reply) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] ssp confirmation %s", argv[0], reply ? "Accept" : "Reject");
    return CMD_OK;
}

static void str2hex(char* src_str, uint8_t* dest_buf, uint8_t hex_number)
{
    uint8_t i;
    uint8_t lb, hb;

    for (i = 0; i < hex_number; i++) {
        lb = src_str[(i << 1) + 1];
        hb = src_str[i << 1];
        if (hb >= '0' && hb <= '9') {
            dest_buf[i] = hb - '0';
        } else if (hb >= 'A' && hb < 'G') {
            dest_buf[i] = hb - 'A' + 10;
        } else if (hb >= 'a' && hb < 'g') {
            dest_buf[i] = hb - 'a' + 10;
        } else {
            dest_buf[i] = 0;
        }

        dest_buf[i] <<= 4;
        if (lb >= '0' && lb <= '9') {
            dest_buf[i] += lb - '0';
        } else if (lb >= 'A' && lb < 'G') {
            dest_buf[i] += lb - 'A' + 10;
        } else if (lb >= 'a' && lb < 'g') {
            dest_buf[i] += lb - 'a' + 10;
        }
    }
}

static int pair_set_tk_cmd(void* handle, int argc, char** argv)
{
    bt_128key_t tk_val;

    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (strlen(argv[1]) < (sizeof(bt_128key_t) * 2)) {
        PRINT("length of temporary key is insufficient");
        return CMD_INVALID_PARAM;
    }

    str2hex(argv[1], tk_val, sizeof(bt_128key_t));

    if (bt_device_set_le_legacy_tk(handle, &addr, tk_val) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Set oob temporary key for le legacy pairing with [%s]", argv[0]);
    return CMD_OK;
}

static int pair_set_oob_cmd(void* handle, int argc, char** argv)
{
    bt_128key_t c_val;
    bt_128key_t r_val;

    if (argc < 3)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (strlen(argv[1]) < (sizeof(bt_128key_t) * 2)) {
        PRINT("length of confirmation value is insufficient");
        return CMD_INVALID_PARAM;
    }

    if (strlen(argv[2]) < (sizeof(bt_128key_t) * 2)) {
        PRINT("length of random value is insufficient");
        return CMD_INVALID_PARAM;
    }

    str2hex(argv[1], c_val, sizeof(bt_128key_t));
    str2hex(argv[2], r_val, sizeof(bt_128key_t));

    if (bt_device_set_le_sc_remote_oob_data(handle, &addr, c_val, r_val) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Set remote oob data for le secure connection pairing with [%s]", argv[0]);
    return CMD_OK;
}

static int pair_get_oob_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (bt_device_get_le_sc_local_oob_data(handle, &addr) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Get local oob data for le secure connection pairing with [%s]", argv[0]);
    return CMD_OK;
}

static int connect_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (bt_device_connect(handle, &addr) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device[%s] connecting", argv[0]);
    return CMD_OK;
}

static int disconnect_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (bt_device_disconnect(handle, &addr) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device[%s] disconnecting", argv[0]);
    return CMD_OK;
}

static int le_connect_cmd(void* handle, int argc, char** argv)
{
    int opt, index = 0;
    bt_address_t addr;
    ble_addr_type_t addrtype = BT_LE_ADDR_TYPE_PUBLIC;
    ble_connect_params_t params = {
        .use_default_params = false,
        .filter_policy = BT_LE_CONNECT_FILTER_POLICY_ADDR,
        .init_phy = BT_LE_1M_PHY,
        .scan_interval = 20, /* 12.5 ms */
        .scan_window = 20, /* 12.5 ms */
        .connection_interval_min = 24, /* 30 ms */
        .connection_interval_max = 24, /* 30 ms */
        .connection_latency = 0,
        .supervision_timeout = 18, /* 180 ms */
        .min_ce_length = 0,
        .max_ce_length = 0,
    };

    bt_addr_set_empty(&addr);
    optind = 1;
    while ((opt = getopt_long(argc, argv, "a:t:f:p:l:T:dh", le_conn_options,
                &index))
        != -1) {
        switch (opt) {
        case 'a': {
            if (bt_addr_str2ba(optarg, &addr) < 0) {
                PRINT("Invalid addr:%s", optarg);
                return CMD_INVALID_ADDR;
            }

        } break;
        case 't': {
            int32_t type = atoi(optarg);
            addrtype = type;
        } break;
        case 'd': {
            params.use_default_params = true;
        } break;
        case 'f': {
            int32_t filter = atoi(optarg);
            if (filter != BT_LE_CONNECT_FILTER_POLICY_ADDR && filter != BT_LE_CONNECT_FILTER_POLICY_WHITE_LIST) {
                PRINT("Invalid filter:%s", optarg);
                return CMD_INVALID_PARAM;
            }

            params.filter_policy = filter;
        } break;
        case 'p': {
            int32_t phy = atoi(optarg);
            if (!phy_is_vaild(phy)) {
                PRINT("Invalid phy:%s", optarg);
                return CMD_INVALID_PARAM;
            }
            params.init_phy = phy;
        } break;
        case 'l': {
            int32_t latency = atoi(optarg);
            if (latency < 0 || latency > 0x01F3) {
                PRINT("Invalid latency:%s", optarg);
                return CMD_INVALID_PARAM;
            }
            params.connection_latency = latency;
        } break;
        case 'T': {
            int32_t timeout = atoi(optarg);
            if (timeout < 0x0A || timeout > 0x0C80) {
                PRINT("Invalid supervision_timeout:%s", optarg);
                return CMD_INVALID_PARAM;
            }
            params.supervision_timeout = timeout;
        } break;
        case 'h': {
            PRINT("%s", LE_CONN_USAGE);
        } break;
        case 0: {
            const char* curopt = le_conn_options[index].name;
            int32_t val = atoi(optarg);

            if (strncmp(curopt, "conn_interval_min", strlen("conn_interval_min")) == 0) {
                if (val < 0x06 || val > 0x0C80) {
                    PRINT("Invalid conn_interval_min:%s", optarg);
                    return CMD_INVALID_PARAM;
                }
                params.connection_interval_min = val;
            } else if (strncmp(curopt, "conn_interval_max", strlen("conn_interval_max")) == 0) {
                if (val < 0x06 || val > 0x0C80) {
                    PRINT("Invalid conn_interval_max:%s", optarg);
                    return CMD_INVALID_PARAM;
                }
                params.connection_interval_max = val;
            } else if (strncmp(curopt, "scan_interval", strlen("scan_interval")) == 0) {
                if (val < 0x04 || val > 0x4000) {
                    PRINT("Invalid scan_interval:%s", optarg);
                    return CMD_INVALID_PARAM;
                }
                params.scan_interval = val;
            } else if (strncmp(curopt, "scan_window", strlen("scan_window")) == 0) {
                if (val < 0x04 || val > 0x4000) {
                    PRINT("Invalid scan_window:%s", optarg);
                    return CMD_INVALID_PARAM;
                }
                params.scan_window = val;
            } else if (strncmp(curopt, "min_ce_length", strlen("min_ce_length")) == 0) {
                if (val < 0x0A || val > 0x0C80) {
                    PRINT("Invalid min_ce_length:%s", optarg);
                    return CMD_INVALID_PARAM;
                }
                params.min_ce_length = val;
            } else if (strncmp(curopt, "max_ce_length", strlen("max_ce_length")) == 0) {
                if (val < 0x0A || val > 0x0C80) {
                    PRINT("Invalid max_ce_length:%s", optarg);
                    return CMD_INVALID_PARAM;
                }
                params.max_ce_length = val;
            } else {
                return CMD_INVALID_OPT;
            }
        } break;
        default:
            return CMD_INVALID_OPT;
        }
    }

    if (bt_addr_is_empty(&addr))
        return CMD_INVALID_ADDR;

    if (bt_device_connect_le(handle, &addr, addrtype, &params) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    return CMD_OK;
}

static int le_disconnect_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (bt_device_disconnect_le(handle, &addr) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("LE Device[%s] disconnecting", argv[0]);
    return CMD_OK;
}

static int create_bond_cmd(void* handle, int argc, char** argv)
{
    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int transport = atoi(argv[1]);
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE)
        return CMD_INVALID_PARAM;

    /* TODO: Check bond state*/
    if (bt_device_create_bond(handle, &addr, transport) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] create bond", argv[0]);
    return CMD_OK;
}

static int cancel_bond_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    /* TODO: Check bond state*/
    if (bt_device_cancel_bond(handle, &addr) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] cancel bond", argv[0]);
    return CMD_OK;
}

static int remove_bond_cmd(void* handle, int argc, char** argv)
{
    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int transport = atoi(argv[1]);
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE)
        return CMD_INVALID_PARAM;

    /* TODO: Check bond state*/
    if (bt_device_remove_bond(handle, &addr, transport) != BT_STATUS_SUCCESS)
        return CMD_ERROR;

    PRINT("Device [%s] remove bond", argv[0]);
    return CMD_OK;
}

static int set_phy_cmd(void* handle, int argc, char** argv)
{
    if (argc < 3)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    int tx_phy, rx_phy;
    tx_phy = atoi(argv[1]);
    rx_phy = atoi(argv[2]);
    if (!phy_is_vaild(tx_phy) || !phy_is_vaild(rx_phy)) {
        PRINT("Invalid phy parameter, tx:%d, rx:%d", tx_phy, rx_phy);
        return CMD_INVALID_PARAM;
    }

    bt_device_set_le_phy(handle, &addr, tx_phy, rx_phy);

    return CMD_OK;
}

static const char* bond_state_to_string(bond_state_t state)
{
    switch (state) {
    case BOND_STATE_NONE:
        return "BOND_NONE";
    case BOND_STATE_BONDING:
        return "BONDING";
    case BOND_STATE_BONDED:
        return "BONDED";
    default:
        return "UNKNOWN";
    }
}

static void device_dump(void* handle, bt_address_t* addr, bt_transport_t transport)
{
    char uuid_str[40] = { 0 };
    char name[64] = { 0 };
    bt_uuid_t* uuids = NULL;
    uint16_t uuid_cnt = 0;
    char addr_str[BT_ADDR_STR_LENGTH] = { 0 };

    bt_addr_ba2str(addr, addr_str);
    PRINT("device [%s]", addr_str);
    if (transport == BT_TRANSPORT_BREDR) {
        bt_device_get_name(handle, addr, name, 64);
        PRINT("\tName: %s", name);
        memset(name, 0, 64);
        bt_device_get_alias(handle, addr, name, 64);
        PRINT("\tAlias: %s", name);
        PRINT("\tClass: 0x%08" PRIx32 "", bt_device_get_device_class(handle, addr));
        PRINT("\tDeviceType: %d", bt_device_get_device_type(handle, addr));
        PRINT("\tIsConnected: %d", bt_device_is_connected(handle, addr, transport));
        PRINT("\tIsEnc: %d", bt_device_is_encrypted(handle, addr, transport));
        PRINT("\tIsBonded: %d", bt_device_is_bonded(handle, addr, transport));
        PRINT("\tBondState: %s", bond_state_to_string(bt_device_get_bond_state(handle, addr, transport)));
        PRINT("\tIsBondInitiateLocal: %d", bt_device_is_bond_initiate_local(handle, addr, transport));
        bt_device_get_uuids(handle, addr, &uuids, &uuid_cnt, bttool_allocator);
        if (uuid_cnt) {
            PRINT("\tUUIDs:[%d]", uuid_cnt);
            for (int i = 0; i < uuid_cnt; i++) {
                bt_uuid_to_string(uuids + i, uuid_str, 40);
                PRINT("\t\tuuid[%-2d]: %s", i, uuid_str);
            }
        }
        free(uuids);
    } else {
        PRINT("\tIsConnected: %d", bt_device_is_connected(handle, addr, transport));
        PRINT("\tIsEnc: %d", bt_device_is_encrypted(handle, addr, transport));
        PRINT("\tIsBonded: %d", bt_device_is_bonded(handle, addr, transport));
        PRINT("\tBondState: %s", bond_state_to_string(bt_device_get_bond_state(handle, addr, transport)));
        PRINT("\tIsBondInitiateLocal: %d", bt_device_is_bond_initiate_local(handle, addr, transport));
    }
}

static int device_show_cmd(void* handle, int argc, char** argv)
{
    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;

    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    device_dump(handle, &addr, BT_TRANSPORT_BREDR);

    return CMD_OK;
}

static int device_set_alias_cmd(void* handle, int argc, char** argv)
{
    if (argc < 2)
        return CMD_PARAM_NOT_ENOUGH;

    bt_address_t addr;
    if (bt_addr_str2ba(argv[0], &addr) < 0)
        return CMD_INVALID_ADDR;

    if (strlen(argv[1]) > 63) {
        PRINT("alias length too long");
        return CMD_INVALID_PARAM;
    }

    bt_device_set_alias(handle, &addr, argv[1]);
    PRINT("Device: [%s] alias:%s set success", argv[0], argv[1]);
    return CMD_OK;
}

static int get_bonded_devices_cmd(void* handle, int argc, char** argv)
{
    bt_address_t* addrs = NULL;
    int num = 0;

    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    int transport = atoi(argv[0]);
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE)
        return CMD_INVALID_PARAM;

    bt_adapter_get_bonded_devices(handle, transport, &addrs, &num, bttool_allocator);
    for (int i = 0; i < num; i++) {
        device_dump(handle, addrs + i, transport);
    }
    free(addrs);
    PRINT("bonded device cnt:%d", num);

    return CMD_OK;
}

static int get_connected_devices_cmd(void* handle, int argc, char** argv)
{
    bt_address_t* addrs = NULL;
    int num = 0;

    if (argc < 1)
        return CMD_PARAM_NOT_ENOUGH;

    int transport = atoi(argv[0]);
    if (transport != BT_TRANSPORT_BREDR && transport != BT_TRANSPORT_BLE)
        return CMD_INVALID_PARAM;

    bt_adapter_get_connected_devices(handle, transport, &addrs, &num, bttool_allocator);
    for (int i = 0; i < num; i++) {
        device_dump(handle, addrs + i, transport);
    }
    free(addrs);
    PRINT("connected device cnt:%d", num);

    return CMD_OK;
}

static int search_cmd(void* handle, int argc, char** argv)
{
    PRINT("%s", __func__);
    return CMD_OK;
}

static int start_service_cmd(void* handle, int argc, char** argv)
{
    return CMD_OK;
}

static int stop_service_cmd(void* handle, int argc, char** argv)
{
    return CMD_OK;
}

static int dump_cmd(void* handle, int argc, char** argv)
{
    return CMD_OK;
}

static int usage_cmd(void* handle, int argc, char** argv)
{
    if (argc == 2 && !strcmp(argv[1], "me!!!"))
        return -2;

    usage();

    return CMD_OK;
}

static int quit_cmd(void* handle, int argc, char** argv)
{
    return -2;
}

static void usage(void)
{
    printf("Usage:\n"
           "\tbttool [options] <command> [command parameters]\n");
    printf("Options:\n"
           "\t--help\tDisplay help\n");
    printf("Commands:\n");
    for (int i = 0; i < ARRAY_SIZE(g_cmd_tables); i++) {
        printf("\t%-8s\t%s\n", g_cmd_tables[i].cmd, g_cmd_tables[i].help);
    }
    printf("\n"
           "For more information on the usage of each command use:\n"
           "\tbttool <command> --help\n");
}

static void show_version(void)
{
    printf("Version :2.0.1");
}

static int execute_command(void* handle, int argc, char* argv[])
{
    int ret;

    for (int i = 0; i < ARRAY_SIZE(g_cmd_tables); i++) {
        if (strlen(g_cmd_tables[i].cmd) == strlen(argv[0]) && strncmp(g_cmd_tables[i].cmd, argv[0], strlen(argv[0])) == 0) {
            if (g_cmd_tables[i].func) {
                if (g_cmd_tables[i].opt)
                    ret = g_cmd_tables[i].func(handle, argc, &argv[0]);
                else
                    ret = g_cmd_tables[i].func(handle, argc - 1, &argv[1]);
                if (g_cmd_tables[i].func == quit_cmd)
                    return -2;
                return ret;
            }
        }
    }

    PRINT("UnKnow command %s", argv[0]);
    usage();

    return CMD_UNKNOWN;
}

static void on_adapter_state_changed_cb(void* cookie, bt_adapter_state_t state)
{
    PRINT("Context:%p, Adapter state changed: %d", cookie, state);
    if (state == BT_ADAPTER_STATE_ON) {
        char name[64 + 1];

        bt_tool_init(g_bttool_ins);
        /* get name */
        bt_adapter_get_name(g_bttool_ins, name, 64);
        /* get io cap */
        bt_io_capability_t cap = bt_adapter_get_io_capability(g_bttool_ins);
        /* get class */
        uint32_t class = bt_adapter_get_device_class(g_bttool_ins);
        /* get scan mode */
        bt_scan_mode_t mode = bt_adapter_get_scan_mode(g_bttool_ins);
        /* enable key derivation */
        bt_adapter_le_enable_key_derivation(g_bttool_ins, true, true);
        bt_adapter_set_page_scan_parameters(g_bttool_ins, BT_BR_SCAN_TYPE_INTERLACED, 0x400, 0x24);
        PRINT("Adapter Name: %s, Cap: %d, Class: 0x%08" PRIX32 ", Mode:%d", name, cap, class, mode);
    } else if (state == BT_ADAPTER_STATE_TURNING_OFF) {
        /* code */
        bt_tool_uninit(g_bttool_ins);
    } else if (state == BT_ADAPTER_STATE_OFF) {
        /* do something */
    }
}

static void on_discovery_state_changed_cb(void* cookie, bt_discovery_state_t state)
{
    PRINT("Discovery state: %s", state == BT_DISCOVERY_STATE_STARTED ? "Started" : "Stopped");
}

static void on_discovery_result_cb(void* cookie, bt_discovery_result_t* result)
{
    PRINT_ADDR("Inquiring: device [%s], name: %s, cod: %08" PRIx32 ", is HEADSET: %s, rssi: %d",
        &result->addr, result->name, result->cod, IS_HEADSET(result->cod) ? "true" : "false", result->rssi);
}

static void on_scan_mode_changed_cb(void* cookie, bt_scan_mode_t mode)
{
    PRINT("Adapter new scan mode: %d", mode);
}

static void on_device_name_changed_cb(void* cookie, const char* device_name)
{
    PRINT("Adapter update device name: %s", device_name);
}

static void on_pair_request_cb(void* cookie, bt_address_t* addr)
{
    if (g_auto_accept_pair)
        bt_device_pair_request_reply(g_bttool_ins, addr, true);

    PRINT_ADDR("Incoming pair request from [%s] %s", addr, g_auto_accept_pair ? "auto accepted" : "please reply");
}

#define LINK_TYPE(trans_) (trans_ == BT_TRANSPORT_BREDR ? "BREDR" : "LE")

static void on_pair_display_cb(void* cookie, bt_address_t* addr, bt_transport_t transport, bt_pair_type_t type, uint32_t passkey)
{
    uint8_t ret = 0;
    char buff[128] = { 0 };
    char buff1[64] = { 0 };
    char addr_str[BT_ADDR_STR_LENGTH] = { 0 };

    bt_addr_ba2str(addr, addr_str);
    sprintf(buff, "Pair Display [%s][%s]", addr_str, LINK_TYPE(transport));
    switch (type) {
    case PAIR_TYPE_PASSKEY_CONFIRMATION:
        if (!g_auto_accept_pair) {
            sprintf(buff1, "[SSP][CONFIRM][%" PRIu32 "] please reply:", passkey);
            break;
        }
        ret = bt_device_set_pairing_confirmation(g_bttool_ins, addr, transport, true);
        sprintf(buff1, "[SSP][CONFIRM] Auto confirm [%" PRIu32 "] %s", passkey, ret == BT_STATUS_SUCCESS ? "SUCCESS" : "FAILED");
        break;
    case PAIR_TYPE_PASSKEY_ENTRY:
        sprintf(buff1, "[SSP][ENTRY][%" PRIu32 "], please reply:", passkey);
        break;
    case PAIR_TYPE_CONSENT:
        sprintf(buff1, "[SSP][CONSENT]");
        break;
    case PAIR_TYPE_PASSKEY_NOTIFICATION:
        sprintf(buff1, "[SSP][NOTIFY][%" PRIu32 "]", passkey);
        break;
    case PAIR_TYPE_PIN_CODE:
        sprintf(buff1, "[PIN] please reply:");
        break;
    }
    strcat(buff, buff1);
    PRINT("%s", buff);
}

static void on_connect_request_cb(void* cookie, bt_address_t* addr)
{
    bt_device_connect_request_reply(g_bttool_ins, addr, true);
    PRINT_ADDR("Incoming connect request from [%s], auto accepted", addr);
}

static void on_connection_state_changed_cb(void* cookie, bt_address_t* addr, bt_transport_t transport, connection_state_t state)
{
    PRINT_ADDR("Device [%s][%s] connection state: %d", addr, LINK_TYPE(transport), state);
}

static void on_bond_state_changed_cb(void* cookie, bt_address_t* addr, bt_transport_t transport, bond_state_t state, bool is_ctkd)
{
    g_bond_state = state;
    PRINT_ADDR("Device [%s][%s] bond state: %s, is_ctkd: %d", addr, LINK_TYPE(transport), bond_state_to_string(state), is_ctkd);
}

static void on_le_sc_local_oob_data_got_cb(void* cookie, bt_address_t* addr, bt_128key_t c_val, bt_128key_t r_val)
{
    PRINT_ADDR("Generate local oob data for le secure connection pairing with [%s]:", addr);

    printf("\tConfirmation value: ");
    for (int i = 0; i < sizeof(bt_128key_t); i++) {
        printf("%02x", c_val[i]);
    }
    printf("\n");

    printf("\tRandom value: ");
    for (int i = 0; i < sizeof(bt_128key_t); i++) {
        printf("%02x", r_val[i]);
    }
    printf("\n");
}

static void on_remote_name_changed_cb(void* cookie, bt_address_t* addr, const char* name)
{
    PRINT_ADDR("Device [%s] name changed: %s", addr, name);
}

static void on_remote_alias_changed_cb(void* cookie, bt_address_t* addr, const char* alias)
{
    PRINT_ADDR("Device [%s] alias changed: %s", addr, alias);
}

static void on_remote_cod_changed_cb(void* cookie, bt_address_t* addr, uint32_t cod)
{
    PRINT_ADDR("Device [%s] class changed: 0x%08" PRIx32 "", addr, cod);
}

static void on_remote_uuids_changed_cb(void* cookie, bt_address_t* addr, bt_uuid_t* uuids, uint16_t size)
{
    char uuid_str[40] = { 0 };

    PRINT_ADDR("Device [%s] uuids changed", addr);

    if (size) {
        PRINT("UUIDs:[%d]", size);
        for (int i = 0; i < size; i++) {
            bt_uuid_to_string(uuids + i, uuid_str, 40);
            PRINT("\tuuid[%-2d]: %s", i, uuid_str);
        }
    }
}

const static adapter_callbacks_t g_adapter_cbs = {
    .on_adapter_state_changed = on_adapter_state_changed_cb,
    .on_discovery_state_changed = on_discovery_state_changed_cb,
    .on_discovery_result = on_discovery_result_cb,
    .on_scan_mode_changed = on_scan_mode_changed_cb,
    .on_device_name_changed = on_device_name_changed_cb,
    .on_pair_request = on_pair_request_cb,
    .on_pair_display = on_pair_display_cb,
    .on_connect_request = on_connect_request_cb,
    .on_connection_state_changed = on_connection_state_changed_cb,
    .on_bond_state_changed = on_bond_state_changed_cb,
    .on_le_sc_local_oob_data_got = on_le_sc_local_oob_data_got_cb,
    .on_remote_name_changed = on_remote_name_changed_cb,
    .on_remote_alias_changed = on_remote_alias_changed_cb,
    .on_remote_cod_changed = on_remote_cod_changed_cb,
    .on_remote_uuids_changed = on_remote_uuids_changed_cb,
};

int execute_command_in_table_offset(void* handle, bt_command_t* table, uint32_t table_size, int argc, char* argv[], uint8_t offset)
{
    int ret;
    bt_command_t* cmd = table;

    for (int i = 0; i < table_size; i++) {
        if (strlen(cmd->cmd) == strlen(argv[0]) && strncmp(cmd->cmd, argv[0], strlen(argv[0])) == 0) {
            if (cmd->func) {
                ret = cmd->func(handle, argc - offset, &argv[offset]);
                return ret;
            }
        }
        cmd++;
    }
    PRINT("Erroneous command %s", argv[0]);

    return CMD_UNKNOWN;
}

int execute_command_in_table(void* handle, bt_command_t* table, uint32_t table_size, int argc, char* argv[])
{
    return execute_command_in_table_offset(handle, table, table_size, argc, argv, 1);
}

static int bttool_ins_init(bttool_t* bttool)
{
    pthread_setschedprio(pthread_self(), CONFIG_BLUETOOTH_SERVICE_LOOP_THREAD_PRIORITY);
    g_bttool_ins = bluetooth_create_instance();
    if (g_bttool_ins == NULL) {
        PRINT("create instance error\n");
        return -1;
    }

    adapter_callback = bt_adapter_register_callback(g_bttool_ins, &g_adapter_cbs);
    if (bt_adapter_get_state(g_bttool_ins) == BT_ADAPTER_STATE_ON)
        bt_tool_init(g_bttool_ins);

    return 0;
}

static void bttool_ins_uninit(bttool_t* bttool)
{
    bt_tool_uninit(g_bttool_ins);
    bt_adapter_unregister_callback(g_bttool_ins, adapter_callback);
    bluetooth_delete_instance(g_bttool_ins);
    g_bttool_ins = NULL;
    adapter_callback = NULL;
}

#ifdef CONFIG_LIBUV_EXTENSION
static void handle_close_cb(uv_handle_t* handle)
{
    uv_stop(uv_handle_get_loop(handle));
}

static void bttool_execute_command_cb(uv_async_queue_t* handle, void* buffer)
{
    int ret;
    int _argc = 0;
    char* _argv[32];
    char* saveptr = NULL;
    char* tmpstr = buffer;
    bttool_t* bttool = handle->data;

    memset(_argv, 0, sizeof(_argv));

    // 1. split command
    while ((tmpstr = strtok_r(tmpstr, " ", &saveptr)) != NULL) {
        _argv[_argc] = tmpstr;
        _argc++;
        tmpstr = NULL;
    }

    // 2. execute command
    if (_argc > 0) {
        if (bttool->async_api) {
#ifdef CONFIG_BLUETOOTH_FRAMEWORK_ASYNC
            ret = execute_async_command(g_bttool_ins, _argc, _argv);
#else
            ret = CMD_INVALID_OPT;
#endif
        } else
            ret = execute_command(g_bttool_ins, _argc, _argv);
        if (ret != CMD_OK) {
            if (ret == -2) {
                if (bttool->async_api) {
#ifdef CONFIG_BLUETOOTH_FRAMEWORK_ASYNC
                    bttool_async_ins_uninit(bttool);
#endif
                } else
                    bttool_ins_uninit(bttool);
                uv_async_queue_close(handle, handle_close_cb);
            } else
                PRINT("cmd execute error: [%s]", cmd_err_str(ret));
        }
    }

    // 3. free buffer alloced by getline()
    free(buffer);
}

static void bttool_command_uvloop_run(bttool_t* bttool)
{
    int ret;

    /* This code is used to initialize the async queue. */
    ret = uv_async_queue_init(&bttool->loop, &bttool->async, bttool_execute_command_cb);
    if (ret != 0) {
        PRINT("%s async error: %d", __func__, ret);
        uv_loop_close(&bttool->loop);
        return;
    }

    bttool->async.data = bttool;
    uv_sem_post(&bttool->ready);

    /* This code is used to start the event loop until there are no more events to process. */
    uv_run(&bttool->loop, UV_RUN_DEFAULT);

    /* The assert() function is used to check the return value of uv_loop_close().
       If the return value is 0, it means that the loop is closed successfully,
       otherwise it means an error occurs.
    */
    assert(uv_loop_close(&bttool->loop) == 0);
}

static void bttool_thread(void* data)
{
    bttool_t* bttool = data;

    /* Initialize the event loop, the loop is available
       before the asynchronous instance is created.
    */
    uv_loop_init(&bttool->loop);

    /* initialize synchronous or asynchronous instance.
       and register callbacks.
    */
    if (!bttool->async_api) {
        bttool_ins_init(bttool);
    } else {
#ifdef CONFIG_BLUETOOTH_FRAMEWORK_ASYNC
        bttool_async_ins_init(bttool);
#endif
    }

    /* This code is used to start the event loop until there are no more events to process. */
    bttool_command_uvloop_run(bttool);
}

static int bttool_create_thread(bttool_t* bttool)
{
    int ret;

    ret = uv_sem_init(&bttool->ready, 0);
    if (ret != 0) {
        PRINT("%s sem init error: %d", __func__, ret);
        return ret;
    }

    uv_thread_options_t options = {
        UV_THREAD_HAS_STACK_SIZE | UV_THREAD_HAS_PRIORITY,
        CONFIG_BLUETOOTH_TOOL_THREAD_STACK_SIZE,
        CONFIG_BLUETOOTH_TOOL_THREAD_PRIORITY
    };
    ret = uv_thread_create_ex(&bttool->thread, &options, bttool_thread, (void*)bttool);

    if (ret != 0) {
        PRINT("loop thread create :%d", ret);
        return ret;
    }

    pthread_setname_np(bttool->thread, "bttool-cmd-exec");
    uv_sem_wait(&bttool->ready);
    uv_sem_destroy(&bttool->ready);

    return 0;
}

static void bttool_quit(bttool_t* bttool)
{
    char* buffer = malloc(5);

    strcpy(buffer, "quit");
    uv_async_queue_send(&bttool->async, buffer);
}

int main(int argc, char** argv)
{
    int opt;
    char* buffer = NULL;
    int ret;
    size_t len, size = 0;
    bttool_t bttool = { .async_api = false };

    while ((opt = getopt_long(argc, argv, "a-h-v-d", main_options, NULL)) != -1) {
        switch (opt) {
        case 'a':
#ifdef CONFIG_BLUETOOTH_FRAMEWORK_ASYNC
            bttool.async_api = true;
            break;
#else
            PRINT("async not supported");
            return -1;
#endif
        case 'h':
            usage();
            exit(0);
        case 'v':
            show_version();
            exit(0);
            break;
        default:
            break;
        }
    }

    // Call the bttool_create_thread function to create a new thread
    // If thread creation fails, the return value is non-zero
    ret = bttool_create_thread(&bttool);
    if (ret != 0)
        return ret;

    while (1) {
        printf("bttool> ");
        fflush(stdout);

        len = getline(&buffer, &size, stdin);
        if (-1 == len) {
            bttool_quit(&bttool);
            break;
        }

        buffer[len] = '\0';
        if (buffer[0] == '!') {
#ifdef CONFIG_SYSTEM_SYSTEM
            system(buffer + 1);
#endif
            continue;
        }

        if (buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';

        if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "q") == 0) {
            uv_async_queue_send(&bttool.async, buffer);
            break;
        }

        uv_async_queue_send(&bttool.async, buffer);

        buffer = NULL;
    }

    uv_thread_join(&bttool.thread);

    return 0;
}
#else /* CONFIG_LIBUV_EXTENSION */
int main(int argc, char** argv)
{
    int opt;
    int _argc = 0;
    char* _argv[32];
    char* buffer = NULL;
    char* saveptr;
    int ret;
    size_t len, size = 0;

    while ((opt = getopt_long(argc, argv, "h-v-d", main_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage();
            exit(0);
        case 'v':
            show_version();
            exit(0);
            break;
        default:
            break;
        }
    }

    bttool_ins_init(NULL);

    while (1) {
        printf("bttool> ");
        fflush(stdout);

        memset(_argv, 0, sizeof(_argv));
        len = getline(&buffer, &size, stdin);
        buffer[len] = '\0';
        if (len < 0)
            goto quit;

        if (buffer[0] == '!') {
#ifdef CONFIG_SYSTEM_SYSTEM
            system(buffer + 1);
#endif
            continue;
        }

        if (buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';

        saveptr = NULL;
        char* tmpstr = buffer;

        while ((tmpstr = strtok_r(tmpstr, " ", &saveptr)) != NULL) {
            _argv[_argc] = tmpstr;
            _argc++;
            tmpstr = NULL;
        }

        if (_argc > 0) {
            ret = execute_command(g_bttool_ins, _argc, _argv);
            _argc = 0;
            if (ret != CMD_OK) {
                if (ret == -2)
                    break;
                PRINT("cmd execute error: [%s]", cmd_err_str(ret));
            }
        }
    }

quit:
    bttool_ins_uninit(NULL);
    free(buffer);

    return 0;
}
#endif /* CONFIG_LIBUV_EXTENSION */