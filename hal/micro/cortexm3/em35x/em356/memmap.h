/** @file hal/micro/cortexm3/em35x/em356/memmap.h
 * @brief Definition of em356 chip specific memory map information
 *
 * THIS IS A GENERATED FILE.  DO NOT EDIT.
 *
 * <!-- Copyright 2014 Silicon Laboratories, Inc.                        *80*-->
 */

#ifndef __MEMMAP_H__
  #error This header should not be included directly, use hal/micro/cortexm3/memmap.h
#endif

#ifndef __EM356_MEMMAP_H__
#define __EM356_MEMMAP_H__

#define RAM_BOTTOM          (0x20000000u)
#define RAM_SIZE_B          (0x00003000u)
#define RAM_SIZE_W          (RAM_SIZE_B/4)
#define RAM_TOP             (RAM_BOTTOM+RAM_SIZE_B-1)

#define MFB_BOTTOM          (0x08000000u)
#define MFB_SIZE_B          (0x00030000u)          
#define MFB_SIZE_W          (MFB_SIZE_B/4)  
#define MFB_TOP             (MFB_BOTTOM+MFB_SIZE_B-1)    
#define MFB_PAGE_SIZE_B     (2048)
#define MFB_PAGE_SIZE_W     (MFB_PAGE_SIZE_B/4)
#define MFB_PAGE_MASK_B     (0xFFFFF800u)
#define MFB_REGION_SIZE     (4)  // Size of a write protect region in pages
#define MFB_ADDR_MASK       (0x0003FFFFu)

#define CIB_BOTTOM          (0x08040800u)
#define CIB_SIZE_B          (2048)
#define CIB_SIZE_W          (CIB_SIZE_B/4)
#define CIB_TOP             (CIB_BOTTOM+CIB_SIZE_B-1)
#define CIB_PAGE_SIZE_B     (2048)
#define CIB_PAGE_SIZE_W     (CIB_PAGE_SIZE_B/4)
#define CIB_OB_BOTTOM       (CIB_BOTTOM+0x00)   //bottom address of CIB option bytes
#define CIB_OB_TOP          (CIB_BOTTOM+0x0F)   //top address of CIB option bytes

#define FIB_BOTTOM          (0x08040000u)
#define FIB_SIZE_B          (2048)
#define FIB_SIZE_W          (FIB_SIZE_B/4)
#define FIB_TOP             (FIB_BOTTOM+FIB_SIZE_B-1)
#define FIB_PAGE_SIZE_B     (2048)
#define FIB_PAGE_SIZE_W     (FIB_PAGE_SIZE_B/4)

#define FPEC_KEY1           0x45670123          //magic key defined in hardware
#define FPEC_KEY2           0xCDEF89AB          //magic key defined in hardware

//Default bootloader size in bytes
#define DEFAULT_BTL_SIZE_B (MFB_REGION_SIZE*MFB_PAGE_SIZE_B)

// Size in bytes that one RAM retention bit covers
#define RAM_RETAIN_BLOCK_SIZE (0)

//Translation between page number and simee (word based) address
#define SIMEE_ADDR_TO_PAGE(x)   ((int8u)(((int16u)(x)) >> 10))
#define PAGE_TO_SIMEE_ADDR(x)   (((int16u)(x)) << 10)

//Translation between page number and code addresses, used by bootloaders
#define PROG_ADDR_TO_PAGE(x)    ((int8u)((((int32u)(x))&MFB_ADDR_MASK) >> 11))
#define PAGE_TO_PROG_ADDR(x)    ((((int32u)(x)) << 11)|MFB_BOTTOM)


#endif //__EM356_MEMMAP_H__
