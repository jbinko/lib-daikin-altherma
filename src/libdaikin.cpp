#include <time.h>
#include <string.h>
#include <errno.h>
#include <string>

#include "../include/libdaikin.h"

#include "websockets.h"
#include "trace.h"

static const char agent[] =
"libdaikin";

static const uint8_t OP_W = 1;
static const uint8_t OP_R = 2;
static const uint8_t INDEX = 0;
static const int32_t RSC_OK = 2000;
static const int32_t RSC_OK_ACT = 2001;

static bool str_to_int32(const char** s, int32_t* const v)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(*s) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    errno = 0;
    char* end;
    long l = strtol(*s, &end, 10);

    if (errno != 0)
    {
        LIBDAIKIN_TRACE("str_to_int32 failed. errno: %ld.\n", errno);
        return false;
    }

    if (*s == end)
    {
        LIBDAIKIN_TRACE("str_to_int32 failed. No number characters?\n");
        return false;
    }

    *v = (int32_t)l;
    *s = end;
    return true;
}

static bool str_to_double(const char** s, double* const v)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(*s) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    errno = 0;
    char* end;
    double d = strtod(*s, &end);

    if (errno != 0)
    {
        LIBDAIKIN_TRACE("str_to_double failed. errno: %ld.\n", errno);
        return false;
    }

    if (*s == end)
    {
        LIBDAIKIN_TRACE("str_to_double failed. No number characters?\n");
        return false;
    }

    *v = d;
    *s = end;
    return true;
}

static bool str_find_skip_token(const char** s, const char* const token, bool strict)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(*s) > 0));
    LIBDAIKIN_ASSERT((token != NULL) && (strlen(token) > 0));

    const char* p = *s;
    const char* e = strstr(p, token);

    if (e == NULL)
    {
        LIBDAIKIN_ERROR("Token '%s' not found.\n", token);
        return false;
    }

    // strict means find exactly at the start
    if (strict && (p != e))
    {
        LIBDAIKIN_ERROR("Token '%s' is not matching the begining of the string '%s'.\n", token, *s);
        return false;
    }

    *s = e + strlen(token);
    return true;
}

static std::string string_between_tokens(
    const char* s,
    const char* const start_token,
    const char* const end_token)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(s) > 0));
    LIBDAIKIN_ASSERT((start_token != NULL) && (strlen(start_token) > 0));
    LIBDAIKIN_ASSERT((end_token != NULL) && (strlen(end_token) > 0));

    // Find token somewhere in the string (strict == false)
    if (str_find_skip_token(&s, start_token, false) == false)
        return ""; // No extra error info needed

    const char* const start = s;

    // Find token somewhere in the string (strict == false)
    if (str_find_skip_token(&s, end_token, false) == false)
        return ""; // No extra error info needed

    const char* const end = s;

    return std::string(start, end - start - 1);
}

static bool is_rsc_ok(int32_t rsc)
{
    return (rsc == RSC_OK || rsc == RSC_OK_ACT);
}

static bool get_con_int32(const char* s, int32_t* const v)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(s) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    // Search for "con":
    const char token01[] = "\"con\":";
    // Search for }
    const char token02[] = "}";

    std::string str = string_between_tokens(s, token01, token02);
    if (str.length() == 0)
        return false; // No extra error info needed

    int32_t temp;
    const char* p = str.c_str();

    if (str_to_int32(&p, &temp) == false)
    {
        LIBDAIKIN_ERROR("Number not found in the string: '%s'.\n", str.c_str());
        return false;
    }

    *v = temp;
    return true;
}

static bool get_con_float(const char* s, float* const v)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(s) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    // Search for "con":
    const char token01[] = "\"con\":";
    // Search for }
    const char token02[] = "}";

    std::string str = string_between_tokens(s, token01, token02);
    if (str.length() == 0)
        return false; // No extra error info needed

    double temp;
    const char* p = str.c_str();

    if (str_to_double(&p, &temp) == false)
    {
        LIBDAIKIN_ERROR("Number not found in the string: '%s'.\n", str.c_str());
        return false;
    }

    *v = (float)temp;
    return true;
}

static bool get_con_power_state(const char* s, daikin_power_state_t* const v)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(s) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    // Search for "con":"
    const char token01[] = "\"con\":\"";
    // Search for "
    const char token02[] = "\"";

    std::string str = string_between_tokens(s, token01, token02);
    if (str.length() == 0)
        return false; // No extra error info needed

    if (str == "on")
        *v = daikin_power_state_t::PS_ON;
    else if (str == "standby")
        *v = daikin_power_state_t::PS_STANDBY;
    else
    {
        *v = daikin_power_state_t::PS_UNKNOWN;
        LIBDAIKIN_TRACE("Unknown power state: '%s'.\n", str.c_str());
    }

    return true;
}

static std::string create_request_id()
{
    srand((unsigned)clock());

    char buf[5];
    const uint8_t len = (uint8_t)sizeof(buf);
    for (uint8_t i = 0; i < len; i++)
        buf[i] = (char)((rand() % 9) + 49); // Just numbers 1-9

    return std::string(buf, len);
}

static std::string create_request_json(
    uint8_t op,
    uint8_t index,
    const char* const field_path,
    std::string& req_id,
    const char* const con_val)
{
    LIBDAIKIN_ASSERT(op == OP_W || op == OP_R);
    LIBDAIKIN_ASSERT(index == INDEX);
    LIBDAIKIN_ASSERT((field_path != NULL) && (strlen(field_path) > 0));
    //LIBDAIKIN_ASSERT(con_val != NULL); con_val Can be NULL

    req_id = create_request_id();

    std::string req = std::string("{\"m2m:rqp\":{\"fr\":\"");
    req += agent;
    req += "\",\"rqi\":\"";
    req += req_id;
    req += "\",\"op\":";
    req += (char)(48 + op); // number to string
    req += ",\"to\":\"/[";
    req += (char)(48 + index); // number to string
    req += "]/";
    req += field_path;

    if (op == OP_R)
    {
        req += "\"";
    }
    else if (op == OP_W)
    {
        // ,"ty":4,"pc":{"m2m:cin":{"con":
        req += "\",\"ty\":4,\"pc\":{\"m2m:cin\":{\"con\":";
        req += con_val;
        // ,"cnf":"text/plain:0"}}
        req += ",\"cnf\":\"text/plain:0\"}}";
    }

    req += "}}";
    return req;
}

static bool parse_query_response(
    const std::string& response,
    const char* const field_path,
    int32_t* const rsc,
    int32_t* const rqi,
    int32_t* const idx,
    const char** reminder)
{
    LIBDAIKIN_ASSERT(response.size() > 0);
    LIBDAIKIN_ASSERT((field_path != NULL) && (strlen(field_path) > 0));
    LIBDAIKIN_ASSERT(rsc != NULL);
    LIBDAIKIN_ASSERT(rqi != NULL);
    LIBDAIKIN_ASSERT(idx != NULL);
    LIBDAIKIN_ASSERT(reminder != NULL);

    const char* s = response.c_str();

    // Search for {"m2m:rsp":{"rsc":
    const char token01[] = "{\"m2m:rsp\":{\"rsc\":";
    if (str_find_skip_token(&s, token01, true) == false)
        return false; // No extra error info needed

    // Search for DDDD - rsc (2000,4004,????)
    if (str_to_int32(&s, rsc) == false)
    {
        LIBDAIKIN_ERROR("Not valid rsc code.\n");
        return false;
    }

    // Search for ,"rqi":"
    const char token02[] = ",\"rqi\":\"";
    if (str_find_skip_token(&s, token02, true) == false)
        return false; // No extra error info needed

    // Search for DDDDD - rqi
    if (str_to_int32(&s, rqi) == false)
    {
        LIBDAIKIN_ERROR("Not valid rqi code.\n");
        return false;
    }

    // Search for ","to":"
    const char token03[] = "\",\"to\":\"";
    if (str_find_skip_token(&s, token03, true) == false)
        return false; // No extra error info needed

    // Search for libdaikin
    if (str_find_skip_token(&s, agent, true) == false)
        return false; // No extra error info needed

    // Search for ","fr":"/[
    const char token04[] = "\",\"fr\":\"/[";
    if (str_find_skip_token(&s, token04, true) == false)
        return false; // No extra error info needed

    // Search for D - INDEX
    if (str_to_int32(&s, idx) == false)
    {
        LIBDAIKIN_ERROR("Not valid idx code.\n");
        return false;
    }

    // Search for ]/
    const char token05[] = "]/";
    if (str_find_skip_token(&s, token05, true) == false)
        return false; // No extra error info needed

    // Search for FIELD PATH
    if (str_find_skip_token(&s, field_path, true) == false)
        return false; // No extra error info needed

    *reminder = s;
    return true;
}

static bool send_query(
    const daikin_t* const daikin,
    uint8_t op,
    const char* const field_path,
    std::string& response,
    int32_t* const rsc,
    const char** reminder,
    const char* const con_val)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT(op == OP_W || op == OP_R);
    LIBDAIKIN_ASSERT((field_path != NULL) && (strlen(field_path) > 0));
    // LIBDAIKIN_ASSERT(rsc != NULL); // rsc Can be NULL
    LIBDAIKIN_ASSERT(reminder != NULL);
    //LIBDAIKIN_ASSERT(con_val != NULL); con_val Can be NULL

    std::string req_id;
    if (daikin_ws_request(daikin, create_request_json(op, INDEX, field_path, req_id, con_val), response) == false)
    {
        LIBDAIKIN_ERROR("Query '%s' failed.\n", field_path);
        return false;
    }

    int32_t rsc_temp, rqi, idx;
    if (parse_query_response(response, field_path, &rsc_temp, &rqi, &idx, reminder) == false)
    {
        LIBDAIKIN_ERROR("Parsing response '%s' failed.\n", response.c_str());
        return false;
    }

    if (rsc != NULL)
        *rsc = rsc_temp; // Caller will handle
    else // Otherwise handle rsc here
    {
        if (is_rsc_ok(rsc_temp) == false)
        {
            LIBDAIKIN_ERROR("Error rsc code: %d indicates error for the query '%s'.\n", rsc_temp, field_path);
            return false;
        }
    }

    if (req_id != std::to_string(rqi))
    {
        LIBDAIKIN_ERROR("rqi code %d doesn't match with the expected code: %s.\n", rqi, req_id.c_str());
        return false;
    }

    if (idx != ((int32_t)INDEX))
    {
        LIBDAIKIN_ERROR("Index code %d doesn't match with the expected code: %u.\n", idx, INDEX);
        return false;
    }

    return true;
}

static bool send_query_con_float(
    const daikin_t* const daikin,
    const char* const field_path,
    std::string& response,
    int32_t* const rsc,
    float* const v)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT((field_path != NULL) && (strlen(field_path) > 0));
    // LIBDAIKIN_ASSERT(rsc != NULL); // rsc Can be NULL
    LIBDAIKIN_ASSERT(v != NULL);

    const char* reminder;
    if (send_query(daikin, OP_R, field_path, response, rsc, &reminder, NULL) == false)
        return false; // No extra error info needed

    if (rsc == NULL || (rsc != NULL && is_rsc_ok(*rsc)))
    {
        if (get_con_float(reminder, v) == false)
        {
            LIBDAIKIN_ERROR("Parsing value for the field '%s' failed.\n", field_path);
            return false;
        }
    }

    return true;
}

static bool send_query_con_int32(
    const daikin_t* const daikin,
    const char* const field_path,
    std::string& response,
    int32_t* const v)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT((field_path != NULL) && (strlen(field_path) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    const char* reminder;
    if (send_query(daikin, OP_R, field_path, response, NULL, &reminder, NULL) == false)
        return false; // No extra error info needed
    if (get_con_int32(reminder, v) == false)
    {
        LIBDAIKIN_ERROR("Parsing value for the field '%s' failed.\n", field_path);
        return false;
    }

    return true;
}

static bool send_query_con_power_state(
    const daikin_t* const daikin,
    const char* const field_path,
    std::string& response,
    daikin_power_state_t* const v)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT((field_path != NULL) && (strlen(field_path) > 0));
    LIBDAIKIN_ASSERT(v != NULL);

    const char* reminder;
    if (send_query(daikin, OP_R, field_path, response, NULL, &reminder, NULL) == false)
        return false; // No extra error info needed
    if (get_con_power_state(reminder, v) == false)
    {
        LIBDAIKIN_ERROR("Parsing value for the field '%s' failed.\n", field_path);
        return false;
    }

    return true;
}

bool daikin_open(daikin_t* const daikin)
{
    LIBDAIKIN_ASSERT(daikin != NULL);

    if (daikin == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument daikin.\n");
        return false;
    }

    return daikin_ws_open(daikin);
}

void daikin_close(daikin_t* const daikin)
{
    LIBDAIKIN_ASSERT(daikin != NULL);

    if (daikin == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument daikin.\n");
        return;
    }

    daikin_ws_close(daikin);
}

bool daikin_get_device_info(
    const daikin_t* const daikin,
    daikin_device_info_t* const info)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT(info != NULL);

    if (daikin == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument daikin.\n");
        return false;
    }

    if (info == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument info.\n");
        return false;
    }

    float temp;
    int32_t rsc;
    std::string response;

    const char INDOOR_TEMP[] = "MNAE/1/Sensor/IndoorTemperature/la";
    if (send_query_con_float(daikin, INDOOR_TEMP, response, NULL, &info->indoor_temp) == false)
        return false; // No extra error info needed

    const char OUTDOOR_TEMP[] = "MNAE/1/Sensor/OutdoorTemperature/la";
    if (send_query_con_float(daikin, OUTDOOR_TEMP, response, NULL, &info->outdoor_temp) == false)
        return false; // No extra error info needed

    const char LW_TEMP[] = "MNAE/1/Sensor/LeavingWaterTemperatureCurrent/la";
    if (send_query_con_float(daikin, LW_TEMP, response, NULL, &info->leaving_water_temp) == false)
        return false; // No extra error info needed

    info->temp_target = 0;
    const char TARGET_TEMP[] = "MNAE/1/Operation/TargetTemperature/la";
    if (send_query_con_float(daikin, TARGET_TEMP, response, &rsc, &temp) == false)
        return false; // No extra error info needed
    if (is_rsc_ok(rsc))
    {
        LIBDAIKIN_TRACE("Target Temperature mode\n");
        info->temp_mode = daikin_temperature_mode_t::TM_TARGET;
        info->temp_target = (uint8_t)temp;
    }

    info->temp_offset = 0;
    const char LW_TEMP_OFFSET[] = "MNAE/1/Operation/LeavingWaterTemperatureOffsetHeating/la";
    if (send_query_con_float(daikin, LW_TEMP_OFFSET, response, &rsc, &temp) == false)
        return false; // No extra error info needed
    if (is_rsc_ok(rsc))
    {
        LIBDAIKIN_TRACE("Leaving Water Temperature Offset Heating mode\n");
        info->temp_mode = daikin_temperature_mode_t::TM_OFFSET;
        info->temp_offset = (uint8_t)temp;
    }

    const char PWR_STATE[] = "MNAE/1/Operation/Power/la";
    if (send_query_con_power_state(daikin, PWR_STATE, response, &info->power_state) == false)
        return false; // No extra error info needed

    const char EM_STATE[] = "MNAE/1/UnitStatus/EmergencyState/la";
    if (send_query_con_int32(daikin, EM_STATE, response, &info->emergency_state) == false)
        return false; // No extra error info needed

    const char ER_STATE[] = "MNAE/1/UnitStatus/ErrorState/la";
    if (send_query_con_int32(daikin, ER_STATE, response, &info->error_state) == false)
        return false; // No extra error info needed

    const char WR_STATE[] = "MNAE/1/UnitStatus/WarningState/la";
    if (send_query_con_int32(daikin, WR_STATE, response, &info->warning_state) == false)
        return false; // No extra error info needed

    return true;
}

bool daikin_set_temp_offset(const daikin_t* const daikin, int8_t temp_offset)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT(temp_offset >= -10 && temp_offset <= 10);

    if (daikin == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument daikin.\n");
        return false;
    }

    if (temp_offset < -10 || temp_offset > 10)
    {
        LIBDAIKIN_ERROR(
            "Invalid input argument temp_offset: %d. Value must be between -10 and 10.\n",
            temp_offset);
        return false;
    }

    std::string response;
    const char* reminder;
    const char TEMP_OFFSET_OP[] = "MNAE/1/Operation/LeavingWaterTemperatureOffsetHeating";

    return send_query(daikin, OP_W, TEMP_OFFSET_OP, response,
        NULL, &reminder, std::to_string(temp_offset).c_str());
}

bool daikin_set_temp_target(const daikin_t* const daikin, uint8_t temp_target)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT(temp_target >= 16 && temp_target <= 30);

    if (daikin == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument daikin.\n");
        return false;
    }

    if (temp_target < 16 || temp_target > 30)
    {
        LIBDAIKIN_ERROR(
            "Invalid input argument temp_target: %u. Value must be between 16 and 30.\n",
            temp_target);
        return false;
    }

    std::string response;
    const char* reminder;
    const char TEMP_TARGET_OP[] = "MNAE/1/Operation/TargetTemperature";

    return send_query(daikin, OP_W, TEMP_TARGET_OP, response,
        NULL, &reminder, std::to_string(temp_target).c_str());
}

bool daikin_set_power_state(const daikin_t* const daikin, daikin_power_state_t power_state)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT(power_state == daikin_power_state_t::PS_ON || power_state == daikin_power_state_t::PS_STANDBY);

    if (daikin == NULL)
    {
        LIBDAIKIN_ERROR("Invalid input argument daikin.\n");
        return false;
    }

    if (!(power_state == daikin_power_state_t::PS_ON || power_state == daikin_power_state_t::PS_STANDBY))
    {
        LIBDAIKIN_ERROR("Invalid input argument power_state: %d\n", power_state);
        return false;
    }

    std::string response;
    const char* reminder;
    const char POWER_OP[] = "MNAE/1/Operation/Power";

    return send_query(daikin, OP_W, POWER_OP, response,
        NULL, &reminder, power_state == daikin_power_state_t::PS_ON ? "\"on\"" : "\"standby\"");
}
