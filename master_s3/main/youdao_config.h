#pragma once

/* Keep real credentials in the ignored, local-only header. */
#if __has_include("youdao_secrets.h")
#include "youdao_secrets.h"
#endif
#ifndef YOUDAO_APP_ID
#define YOUDAO_APP_ID ""
#endif
#ifndef YOUDAO_APP_SECRET
#define YOUDAO_APP_SECRET ""
#endif
#define YOUDAO_ASR_URL "https://openapi.youdao.com/asrapi"
#define YOUDAO_ASR_LANGUAGE "zh-CHS"
/* The official /asrapi SDK examples use v2 for the truncated-audio signature. */
#define YOUDAO_ASR_SIGN_TYPE "v2"
