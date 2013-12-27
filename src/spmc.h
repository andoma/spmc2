#pragma once

#define PLUGINID_MAX_LEN          128

#define SPMC_USER_ADMIN           0x1
#define SPMC_USER_AUTOAPPROVE     0x2

uint32_t parse_version_int(const char *str);
