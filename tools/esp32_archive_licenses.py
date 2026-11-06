"""Evidence-backed classifications for archives linked into ESP32 PUBLIC.

This table is deliberately tied to Arduino ESP32 2.0.17 / ESP-IDF 4.4.7 and
the Xtensa GCC 8.4.0 toolchain.  It is not a generic name-to-license guess.
"""

IDF_COMMIT = "38eeba213aa695aabfd6d89aa9f5078dbe5a94c3"
WIFI_COMMIT = "f2aae4d44ec7908013066e69d29b9948846c335c"
PHY_COMMIT = "dcfdccf6cc2fc02d0886624b7998c890d1a19b28"
BT_COMMIT = "e43c9b121f7fac89b73c56077ede5901f64643cd"
CJSON_COMMIT = "87d8f0961a01bf09bef98ff89bae9fdec42181ee"
PROTOBUF_COMMIT = "abc67a11c6db271bedbb9f58be85d6f4e2ea8389"


def record(license_id, origin, version, evidence, copyright_notice,
           redistribution, source_obligation="None beyond license/notice retention",
           status="VERIFIED_EXACT_BASELINE"):
    return {
        "license": license_id,
        "origin": origin,
        "upstream_version": version,
        "license_evidence": evidence,
        "copyright_notice": copyright_notice,
        "redistribution_conditions": redistribution,
        "source_obligation": source_obligation,
        "status": status,
    }


IDF_APACHE = record(
    "Apache-2.0", "ESP-IDF source component", f"v4.4.7 ({IDF_COMMIT})",
    f"https://github.com/espressif/esp-idf/blob/{IDF_COMMIT}/LICENSE",
    "Espressif Systems and component contributors",
    "Include Apache-2.0; retain notices; mark modified files; preserve NOTICE if present")

ARCHIVE_LICENSES = {
    name: dict(IDF_APACHE) for name in (
        "libapp_update.a", "libbootloader_support.a", "libbt.a", "libcxx.a",
        "libdriver.a", "libefuse.a", "libesp_common.a", "libesp_eth.a",
        "libesp_event.a", "libesp_hw_support.a", "libesp_ipc.a",
        "libesp_netif.a", "libesp_phy.a", "libesp_pm.a", "libesp_ringbuf.a",
        "libesp_rom.a", "libesp_system.a", "libesp_timer.a", "libesp_wifi.a",
        "libespcoredump.a", "libhal.a", "libheap.a", "liblog.a",
        "libprotocomm.a", "libpthread.a", "libsoc.a", "libspi_flash.a",
        "libtcpip_adapter.a", "libvfs.a", "libwifi_provisioning.a",
    )
}

for name in ("libcoexist.a", "libcore.a", "libmesh.a", "libnet80211.a",
             "libpp.a", "libsmartconfig.a"):
    ARCHIVE_LICENSES[name] = record(
        "Apache-2.0", "Espressif esp32-wifi-lib binary archive", WIFI_COMMIT,
        f"https://github.com/espressif/esp32-wifi-lib/blob/{WIFI_COMMIT}/LICENSE",
        "Copyright (C) 2015-2016 Espressif Systems",
        "Object-form redistribution permitted under Apache-2.0; include license and notices")

for name in ("libphy.a", "librtc.a"):
    ARCHIVE_LICENSES[name] = record(
        "Apache-2.0", "Espressif esp-phy-lib binary archive", PHY_COMMIT,
        f"https://github.com/espressif/esp-phy-lib/blob/{PHY_COMMIT}/LICENSE",
        "Copyright (C) 2015-2021 Espressif Systems",
        "Object-form redistribution permitted under Apache-2.0; include license and notices")

ARCHIVE_LICENSES["libbtdm_app.a"] = record(
    "Apache-2.0", "Espressif esp32-bt-lib binary archive", BT_COMMIT,
    f"https://github.com/espressif/esp32-bt-lib/blob/{BT_COMMIT}/LICENSE",
    "Copyright (C) 2015-2016 Espressif Systems",
    "Object-form redistribution permitted under Apache-2.0; include license and notices")

ARCHIVE_LICENSES["libjson.a"] = record(
    "MIT", "ESP-IDF cJSON component", CJSON_COMMIT,
    f"https://github.com/DaveGamble/cJSON/blob/{CJSON_COMMIT}/LICENSE",
    "Copyright (c) 2009-2017 Dave Gamble and cJSON contributors",
    "Retain copyright and MIT permission notice")
ARCHIVE_LICENSES["libprotobuf-c.a"] = record(
    "BSD-2-Clause", "ESP-IDF protobuf-c component", PROTOBUF_COMMIT,
    f"https://github.com/protobuf-c/protobuf-c/blob/{PROTOBUF_COMMIT}/LICENSE",
    "protobuf-c authors and contributors",
    "Retain copyright, conditions and disclaimer in source and binary distributions")
ARCHIVE_LICENSES["libwpa_supplicant.a"] = record(
    "BSD-3-Clause", "ESP-IDF wpa_supplicant component", f"v4.4.7 ({IDF_COMMIT})",
    f"https://github.com/espressif/esp-idf/blob/{IDF_COMMIT}/components/wpa_supplicant/COPYING",
    "Copyright (c) 2002-2016 Jouni Malinen and contributors",
    "Retain copyright, conditions and disclaimer; no endorsement")

for name in ("libc.a", "libm.a"):
    ARCHIVE_LICENSES[name] = record(
        "BSD-family AND other permissive notices", "Xtensa newlib runtime",
        "toolchain-xtensa-esp32 8.4.0+2021r2-patch5",
        "toolchain share/licenses/newlib/COPYING.NEWLIB",
        "Multiple newlib contributors; per-file notices apply",
        "Reproduce all applicable newlib notices with binary distribution",
        "Provide notices; source availability is recommended for auditability")

for name in ("libgcc.a", "libstdc++.a"):
    ARCHIVE_LICENSES[name] = record(
        "GPL-3.0-or-later WITH GCC-exception-3.1", "Xtensa GCC runtime",
        "toolchain-xtensa-esp32 8.4.0+2021r2-patch5",
        "toolchain share/licenses/gcc/COPYING3 and COPYING.RUNTIME",
        "Free Software Foundation and GCC contributors",
        "Retain GPLv3 and GCC Runtime Library Exception texts",
        "Exception permits eligible compiled programs without GPL propagation")

ARCHIVE_LICENSES["libxtensa.a"] = record(
    "MIT AND Apache-2.0", "ESP-IDF Xtensa component", f"v4.4.7 ({IDF_COMMIT})",
    f"https://github.com/espressif/esp-idf/tree/{IDF_COMMIT}/components/xtensa",
    "Cadence Design Systems, Tensilica and Espressif Systems",
    "Retain MIT and Apache-2.0 copyright/license notices")
ARCHIVE_LICENSES["libxt_hal.a"] = record(
    "MIT", "Cadence/Tensilica Xtensa HAL shipped by ESP-IDF", f"v4.4.7 ({IDF_COMMIT})",
    f"https://github.com/espressif/esp-idf/tree/{IDF_COMMIT}/components/xtensa",
    "Cadence Design Systems and Tensilica",
    "Retain copyright and MIT permission notice")

ARCHIVE_LICENSES["libesp_diagnostics.a"] = record(
    "Apache-2.0", "Espressif esp-insights esp_diagnostics component",
    "esp-rainmaker d8e93454f495bd8a414829ec5e86842b373ff555 / "
    "esp-insights 203a87f671710295579404f7d4c62a8a5e25ae7c",
    "framework tools/sdk/versions.txt; esp-rainmaker gitlink; matching CMake "
    "source list and embedded object paths; "
    "https://github.com/espressif/esp-insights/blob/"
    "203a87f671710295579404f7d4c62a8a5e25ae7c/LICENSE; archive SHA-256 "
    "f7e621272ab8b74c27a2dbd2cd818d6439cc01b0b130654e0fc9c6d3f796e940",
    "Copyright 2021 Espressif Systems (Shanghai) PTE LTD",
    "Include Apache-2.0; retain notices; mark modified files; preserve NOTICE if present")
