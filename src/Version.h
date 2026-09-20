#pragma once

// Single source for the app version and brand. Bump the version before a release.
#define CLL_VERSION "2.1.5"
#define CLL_USER_AGENT "QualityOfLifeModManager/" CLL_VERSION

// Brand strings. CLL_* macro names are kept so the Cod LAN Launcher upstream
// stays easy to merge; the values are this fork's.
#define QOL_APP_NAME        "Quality of Life Mod Manager"
#define QOL_BRAND_TITLE     "Quality of Life"
#define QOL_BRAND_SUBTITLE  "Mod manager"
#define QOL_AUTHOR          "DavidHiFi"
#define QOL_INI_NAME        "QualityOfLife.ini"
#define QOL_REPO_URL        "https://github.com/DavidHiFi/QualityOfLifeModManager"
#define QOL_SERIES_URL      "https://github.com/DavidHiFi/Plutonium-QoL-Series"
#define QOL_AUTHOR_URL      "https://github.com/DavidHiFi"
#define QOL_RAW_BASE        "https://raw.githubusercontent.com/DavidHiFi/QualityOfLifeModManager/main/"
#define QOL_HOME_FEED_URL   QOL_RAW_BASE "qol_home.json"
#define QOL_UPDATE_FEED_URL QOL_RAW_BASE "qol_update.json"
#define QOL_LAUNCHER_ASSET  "QualityOfLifeModManager.exe"
// Cod LAN Launcher, the upstream this app is forked from (LGPL-3.0).
#define CLL_UPSTREAM_URL    "https://github.com/MestreTM/CLL-Cod-Lan-Launcher"
#define CLL_PU_DAT_URL      "https://github.com/MestreTM/CLL-CodLanLauncher/releases/download/v0.1/pu.dat"
