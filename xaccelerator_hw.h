// xaccelerator_hw.h
// Auto-generated register map from Vivado HLS
// Based on actual RegMapping.txt output

#ifndef XACCELERATOR_HW_H
#define XACCELERATOR_HW_H

#include <cstdint>

// Base address would be set by device tree / platform
#define XACCELERATOR_BASEADDR 0x43C00000

// ==============================================================
// Control Registers
// ==============================================================

#define XACCELERATOR_CTRL_ADDR_AP_CTRL        0x00  // Control signals
#define XACCELERATOR_CTRL_ADDR_GIE            0x04  // Global Interrupt Enable
#define XACCELERATOR_CTRL_ADDR_IER            0x08  // Interrupt Enable
#define XACCELERATOR_CTRL_ADDR_ISR            0x0C  // Interrupt Status

// AP_CTRL bits (0x00)
#define XACCELERATOR_AP_CTRL_START            0x01  // bit 0 - ap_start (R/W/COH)
#define XACCELERATOR_AP_CTRL_DONE             0x02  // bit 1 - ap_done (R/COR)
#define XACCELERATOR_AP_CTRL_IDLE             0x04  // bit 2 - ap_idle (R)
#define XACCELERATOR_AP_CTRL_READY            0x08  // bit 3 - ap_ready (R/COR)
#define XACCELERATOR_AP_CTRL_AUTO_RESTART     0x80  // bit 7 - auto_restart (R/W)
#define XACCELERATOR_AP_CTRL_INTERRUPT        0x200 // bit 9 - interrupt (R)

// ==============================================================
// Config Input - 1216 bits (38 x 32-bit registers)
// 0x10 - 0xA4: config_in[1215:0]
// ==============================================================

#define XACCELERATOR_CONFIG_IN_BASE           0x10
#define XACCELERATOR_CONFIG_IN_WORDS          38    // 1216 bits / 32 = 38 words

// Individual config_in register offsets
#define XACCELERATOR_CONFIG_IN_0              0x10  // config_in[31:0]
#define XACCELERATOR_CONFIG_IN_1              0x14  // config_in[63:32]
#define XACCELERATOR_CONFIG_IN_2              0x18  // config_in[95:64]
#define XACCELERATOR_CONFIG_IN_3              0x1C  // config_in[127:96]
#define XACCELERATOR_CONFIG_IN_4              0x20  // config_in[159:128]
#define XACCELERATOR_CONFIG_IN_5              0x24  // config_in[191:160]
#define XACCELERATOR_CONFIG_IN_6              0x28  // config_in[223:192]
#define XACCELERATOR_CONFIG_IN_7              0x2C  // config_in[255:224]
#define XACCELERATOR_CONFIG_IN_8              0x30  // config_in[287:256]
#define XACCELERATOR_CONFIG_IN_9              0x34  // config_in[319:288]
#define XACCELERATOR_CONFIG_IN_10             0x38  // config_in[351:320]
#define XACCELERATOR_CONFIG_IN_11             0x3C  // config_in[383:352]
#define XACCELERATOR_CONFIG_IN_12             0x40  // config_in[415:384]
#define XACCELERATOR_CONFIG_IN_13             0x44  // config_in[447:416]
#define XACCELERATOR_CONFIG_IN_14             0x48  // config_in[479:448]
#define XACCELERATOR_CONFIG_IN_15             0x4C  // config_in[511:480]
#define XACCELERATOR_CONFIG_IN_16             0x50  // config_in[543:512]
#define XACCELERATOR_CONFIG_IN_17             0x54  // config_in[575:544]
#define XACCELERATOR_CONFIG_IN_18             0x58  // config_in[607:576]
#define XACCELERATOR_CONFIG_IN_19             0x5C  // config_in[639:608]
#define XACCELERATOR_CONFIG_IN_20             0x60  // config_in[671:640]
#define XACCELERATOR_CONFIG_IN_21             0x64  // config_in[703:672]
#define XACCELERATOR_CONFIG_IN_22             0x68  // config_in[735:704]
#define XACCELERATOR_CONFIG_IN_23             0x6C  // config_in[767:736]
#define XACCELERATOR_CONFIG_IN_24             0x70  // config_in[799:768]
#define XACCELERATOR_CONFIG_IN_25             0x74  // config_in[831:800]
#define XACCELERATOR_CONFIG_IN_26             0x78  // config_in[863:832]
#define XACCELERATOR_CONFIG_IN_27             0x7C  // config_in[895:864]
#define XACCELERATOR_CONFIG_IN_28             0x80  // config_in[927:896]
#define XACCELERATOR_CONFIG_IN_29             0x84  // config_in[959:928]
#define XACCELERATOR_CONFIG_IN_30             0x88  // config_in[991:960]
#define XACCELERATOR_CONFIG_IN_31             0x8C  // config_in[1023:992]
#define XACCELERATOR_CONFIG_IN_32             0x90  // config_in[1055:1024]
#define XACCELERATOR_CONFIG_IN_33             0x94  // config_in[1087:1056]
#define XACCELERATOR_CONFIG_IN_34             0x98  // config_in[1119:1088]
#define XACCELERATOR_CONFIG_IN_35             0x9C  // config_in[1151:1120]
#define XACCELERATOR_CONFIG_IN_36             0xA0  // config_in[1183:1152]
#define XACCELERATOR_CONFIG_IN_37             0xA4  // config_in[1215:1184]

// ==============================================================
// Status Output - 128 bits (4 x 32-bit registers)
// 0xAC - 0xB8: status_out[127:0]
// ==============================================================

#define XACCELERATOR_STATUS_OUT_BASE          0xAC
#define XACCELERATOR_STATUS_OUT_WORDS         4     // 128 bits / 32 = 4 words

#define XACCELERATOR_STATUS_OUT_0             0xAC  // status_out[31:0]
#define XACCELERATOR_STATUS_OUT_1             0xB0  // status_out[63:32]
#define XACCELERATOR_STATUS_OUT_2             0xB4  // status_out[95:64]
#define XACCELERATOR_STATUS_OUT_3             0xB8  // status_out[127:96]
#define XACCELERATOR_STATUS_OUT_CTRL          0xBC  // status_out control (ap_vld)

// Status control bits (0xBC)
#define XACCELERATOR_STATUS_OUT_AP_VLD        0x01  // bit 0 - status_out_ap_vld

// ==============================================================
// IRQ Clear
// ==============================================================

#define XACCELERATOR_IRQ_CLEAR_IN             0xD4  // irq_clear_in[31:0]

// ==============================================================
// Helper Macros
// ==============================================================

// Calculate offset for config_in word N
#define XACCELERATOR_CONFIG_IN_OFFSET(n)  (XACCELERATOR_CONFIG_IN_BASE + ((n) * 4))

// Calculate offset for status_out word N
#define XACCELERATOR_STATUS_OUT_OFFSET(n) (XACCELERATOR_STATUS_OUT_BASE + ((n) * 4))

#endif // XACCELERATOR_HW_H
