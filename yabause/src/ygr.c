/*  Copyright 2014-2016 James Laird-Wah
    Copyright 2004-2006, 2013 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file ygr.c
    \brief YGR implementation (CD block LSI)
*/

#include "core.h"
#include "sh7034.h"
#include "assert.h"
#include "memory.h"
#include "debug.h"
#include <stdarg.h>

void Cs2Exec(u32 timing);

//#define YGR_SH1_RW_DEBUG
#ifdef YGR_SH1_RW_DEBUG
#define YGR_SH1_RW_LOG(...) DebugPrintf(MainLog, __FILE__, __LINE__, __VA_ARGS__)
#else
#define YGR_SH1_RW_LOG(...)
#endif

// ygr connections
// ygr <=> cd signal processor (on cd drive board)
// ygr <=> sh1 (aka sh7034)
// ygr <=> a-bus (sh2 interface)
// ygr <=> vdp2 (mpeg video)
// ygr <=> scsp (cd audio)

struct Ygr
{
   struct Regs
   {
      u32 DTR;
      u16 UNKNOWN;
      u16 HIRQ;
      u16 HIRQMASK; // Masks bits from HIRQ -only- when generating A-bus interrupts
      u16 CR1;
      u16 CR2;
      u16 CR3;
      u16 CR4;
      u16 MPEGRGB;
   }regs;

   int fifo_ptr;
   u16 fifo[4];
   u16 transfer_ctrl;

   u16 cdirq_flags;

   int mbx_status;
   u16 fake_fifo;
}ygr_cxt = { 0 };

u8 ygr_sh1_read_byte(u32 addr)
{
   CDTRACE("rblsi: %08X\n", addr);
   YGR_SH1_RW_LOG("ygr_sh1_read_byte 0x%08x", addr );
   return 0;
}

u16 ygr_sh1_read_word(u32 addr)
{
   CDTRACE("rwlsi: %08X\n", addr);
   switch (addr & 0xffff) {
   case 0:
      ygr_cxt.fifo_ptr++;
      ygr_cxt.fifo_ptr &= 3;
      return ygr_cxt.fifo[ygr_cxt.fifo_ptr];
   case 2:
      return ygr_cxt.transfer_ctrl;
   case 4:
      return ygr_cxt.mbx_status;
   case 0x6:
      return ygr_cxt.cdirq_flags;
   case 8:
      return ygr_cxt.regs.UNKNOWN;
   case 0xa:
      return ygr_cxt.regs.HIRQMASK;
   case 0x10: // CR1
      return ygr_cxt.regs.CR1;
   case 0x12: // CR2
      return ygr_cxt.regs.CR2;
   case 0x14: // CR3
      return ygr_cxt.regs.CR3;
   case 0x16: // CR4
      return ygr_cxt.regs.CR4;
   }
   YGR_SH1_RW_LOG("ygr_sh1_read_word 0x%08x", addr);
   return 0;
}

u32 ygr_sh1_read_long(u32 addr)
{
   CDTRACE("rllsi: %08X\n", addr);
   YGR_SH1_RW_LOG("ygr_sh1_read_long 0x%08x", addr);
   return 0;
}

void ygr_sh1_write_byte(u32 addr,u8 data)
{
   CDTRACE("wblsi: %08X %02X\n", addr, data);
   YGR_SH1_RW_LOG("ygr_sh1_write_byte 0x%08x 0x%02x", addr, data);
}

void ygr_sh1_write_word(u32 addr, u16 data)
{
   CDTRACE("wwlsi: %08X %04X\n", addr, data);
   switch (addr & 0xffff) {
   case 0:
      ygr_cxt.fake_fifo = data;
      return;
   case 2:
      ygr_cxt.transfer_ctrl = data;
      return;
   case 4:
      ygr_cxt.mbx_status = data;
      return;
   case 0x6:
      ygr_cxt.cdirq_flags = data;
      return;
   case 8:
      ygr_cxt.regs.UNKNOWN = data & 3;
      return;
   case 0xa:
      ygr_cxt.regs.HIRQMASK = data & 0x70;
      return;
   case 0x10: // CR1
      ygr_cxt.regs.CR1 = data;
      return;
   case 0x12: // CR2
      ygr_cxt.regs.CR2 = data;
      return;
   case 0x14: // CR3
      ygr_cxt.regs.CR3 = data;
      return;
   case 0x16: // CR4
      ygr_cxt.regs.CR4 = data;
      return;
   case 0x1e:
      ygr_cxt.regs.HIRQ |= data;
      return;
   }
   YGR_SH1_RW_LOG("ygr_sh1_write_word 0x%08x 0x%04x", addr, data);
}

void ygr_sh1_write_long(u32 addr, u32 data)
{
   CDTRACE("wblsi: %08X %08X\n", addr, data);
   YGR_SH1_RW_LOG("ygr_sh1_write_long 0x%08x 0x%08x", addr, data);
}

//////////////////////////////////////////////////////////////////////////////

void lle_trace_log(const char * format, ...)
{
   static int started = 0;
   static FILE* fp = NULL;
   va_list l;

   if (!started)
   {
      fp = fopen("C:/yabause/lle_log.txt", "w");

      if (!fp)
      {
         return;
      }
      started = 1;
   }

   va_start(l, format);
   vfprintf(fp, format, l);
   va_end(l);
}

//#define WANT_LLE_TRACE

#ifdef WANT_LLE_TRACE
#define LLECDLOG(...) lle_trace_log(__VA_ARGS__)
#else
#define LLECDLOG(...)
#endif

void ygr_a_bus_cd_cmd_log(void);


//replacements for Cs2ReadWord etc
u16 FASTCALL ygr_a_bus_read_word(SH2_struct * sh, u32 addr) {
   u16 val = 0;
   addr &= 0xFFFFF; // fix me(I should really have proper mapping)

   switch (addr) {
   case 0x90008:
   case 0x9000A:
      LLECDLOG("Cs2ReadWord %08X %04X\n", addr, ygr_cxt.regs.HIRQ);
      return ygr_cxt.regs.HIRQ;
   case 0x9000C:
   case 0x9000E:
      LLECDLOG("Cs2ReadWord %08X %04X\n", addr, ygr_cxt.regs.HIRQMASK);
      return ygr_cxt.regs.HIRQMASK;
   case 0x90018:
   case 0x9001A: 
      LLECDLOG("Cs2ReadWord %08X %04X\n", addr, ygr_cxt.regs.CR1);
      return ygr_cxt.regs.CR1;
   case 0x9001C:
   case 0x9001E: 
      LLECDLOG("Cs2ReadWord %08X %04X\n", addr, ygr_cxt.regs.CR2);
      return ygr_cxt.regs.CR2;
   case 0x90020:
   case 0x90022: 
      LLECDLOG("Cs2ReadWord %08X %04X\n", addr, ygr_cxt.regs.CR3);
      return ygr_cxt.regs.CR3;
   case 0x90024:
   case 0x90026: 
      LLECDLOG("Cs2ReadWord %08X %04X\n", addr, ygr_cxt.regs.CR4);
      ygr_cxt.mbx_status |= 2;//todo test this
      CDLOG("abus cdb response: %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
      return ygr_cxt.regs.CR4;
   case 0x90028:
   case 0x9002A: 
      return ygr_cxt.regs.MPEGRGB;
   case 0x98000:
      // transfer info
      sh1_dreq_asserted(1);
      return ygr_cxt.fake_fifo;
      break;
   default:
      LOG("ygr\t: Undocumented register read %08X\n", addr);
      break;
   }

   return val;
}

//////////////////////////////////////////////////////////////////////////////
#ifdef CDDEBUG
void ygr_a_bus_cd_cmd_log(void) 
{
	u16 instruction=ygr_cxt.regs.CR1 >> 8;

   switch (instruction) 
   {
      case 0x00:
         CDLOG("abus cdb command: Get Status %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x01:
         CDLOG("abus cdb command: Get Hardware Info %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x02:
         CDLOG("abus cdb command: Get TOC %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x03:
         CDLOG("abus cdb command: Get Session Info %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x04:
         CDLOG("abus cdb command: Initialize CD System %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x05:
         CDLOG("abus cdb command: Open Tray %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x06:
         CDLOG("abus cdb command: End Data Transfer %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x10:
         CDLOG("abus cdb command: Play Disc %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x11:
         CDLOG("abus cdb command: Seek Disc %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x12:
         CDLOG("abus cdb command: Scan Disc %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x20:
         CDLOG("abus cdb command: Get Subcode QRW %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x30:
         CDLOG("abus cdb command: Set CD Device Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x31:
         CDLOG("abus cdb command: Get CD Device Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x32:
         CDLOG("abus cdb command: Get Last Buffer Destination %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x40:
         CDLOG("abus cdb command: Set Filter Range %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x41:
         CDLOG("abus cdb command: Get Filter Range %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x42:
         CDLOG("abus cdb command: Set Filter Subheader Conditions %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x43:
         CDLOG("abus cdb command: Get Filter Subheader Conditions %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x44:
         CDLOG("abus cdb command: Set Filter Mode %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x45:
         CDLOG("abus cdb command: Get Filter Mode %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x46:
         CDLOG("abus cdb command: Set Filter Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x47:
         CDLOG("abus cdb command: Get Filter Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x48:
         CDLOG("abus cdb command: Reset Selector %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x50:
         CDLOG("abus cdb command: Get Buffer Size %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x51:
         CDLOG("abus cdb command: Get Sector Number %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x52:
         CDLOG("abus cdb command: Calculate Actual Size %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x53:
         CDLOG("abus cdb command: Get Actual Size %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x54:
         CDLOG("abus cdb command: Get Sector Info %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x55:
         CDLOG("abus cdb command: Execute FAD Search %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x56:
         CDLOG("abus cdb command: Get FAD Search Results %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x60:
         CDLOG("abus cdb command: Set Sector Length %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x61:
         CDLOG("abus cdb command: Get Sector Data %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x62:
         CDLOG("abus cdb command: Delete Sector Data %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x63:
         CDLOG("abus cdb command: Get Then Delete Sector Data %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x64:
         CDLOG("abus cdb command: Put Sector Data %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x65:
         CDLOG("abus cdb command: Copy Sector Data %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x66:
         CDLOG("abus cdb command: Move Sector Data %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x67:
         CDLOG("abus cdb command: Get Copy Error %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x70:
         CDLOG("abus cdb command: Change Directory %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x71:
         CDLOG("abus cdb command: Read Directory %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x72:
         CDLOG("abus cdb command: Get File System Scope %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x73:
         CDLOG("abus cdb command: Get File Info %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x74:
         CDLOG("abus cdb command: Read File %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x75:
         CDLOG("abus cdb command: Abort File %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x90:
         CDLOG("abus cdb command: MPEG Get Status %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x91:
         CDLOG("abus cdb command: MPEG Get Interrupt %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x92:
         CDLOG("abus cdb command: MPEG Set Interrupt Mask %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2);
         break;
      case 0x93: 
         CDLOG("abus cdb command: MPEG Init %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2);
         break;
      case 0x94:
         CDLOG("abus cdb command: MPEG Set Mode %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3);
         break;
      case 0x95:
         CDLOG("abus cdb command: MPEG Play %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR4);
         break;
      case 0x96:
         CDLOG("abus cdb command: MPEG Set Decoding Method %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR4);
         break;
		case 0x97:
			CDLOG("abus cdb command: MPEG Out Decoding Sync %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR4);
			break;
		case 0x98:
			CDLOG("abus cdb command: MPEG Get Timecode %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR4);
			break;
		case 0x99:
			CDLOG("abus cdb command: MPEG Get PTS %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR4);
			break;
      case 0x9A:
         CDLOG("abus cdb command: MPEG Set Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x9B:
         CDLOG("abus cdb command: MPEG Get Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
		case 0x9C:
			CDLOG("abus cdb command: MPEG Change Connection %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
			break;
      case 0x9D:
         CDLOG("abus cdb command: MPEG Set Stream %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0x9E:
         CDLOG("abus cdb command: MPEG Get Stream %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
		case 0x9F:
			CDLOG("abus cdb command: MPEG Get Picture Size %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
			break;
      case 0xA0:
         CDLOG("abus cdb command: MPEG Display %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA1:
         CDLOG("abus cdb command: MPEG Set Window %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA2:
         CDLOG("abus cdb command: MPEG Set Border Color %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA3:
         CDLOG("abus cdb command: MPEG Set Fade %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA4:
         CDLOG("abus cdb command: MPEG Set Video Effects %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA5:
         CDLOG("abus cdb command: MPEG Get Image %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA6:
         CDLOG("abus cdb command: MPEG Set Image %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA7:
         CDLOG("abus cdb command: MPEG Read Image %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA8:
         CDLOG("abus cdb command: MPEG Write Image %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xA9:
         CDLOG("abus cdb command: MPEG Read Sector %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xAA:
         CDLOG("abus cdb command: MPEG Write Sector %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xAE:
         CDLOG("abus cdb command: MPEG Get LSI %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xAF:
         CDLOG("abus cdb command: MPEG Set LSI %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xE0:
         CDLOG("abus cdb command: Authenticate Device %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xE1:
         CDLOG("abus cdb command: Is Device Authenticated %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      case 0xE2:
         CDLOG("abus cdb command: Get MPEG ROM %04x %04x %04x %04x %04x\n", ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
      default:
         CDLOG("abus cdb command: Unknown Command(0x%02X) %04x %04x %04x %04x %04x\n", instruction, ygr_cxt.regs.HIRQ, ygr_cxt.regs.CR1, ygr_cxt.regs.CR2, ygr_cxt.regs.CR3, ygr_cxt.regs.CR4);
         break;
	}
}
#else
#define ygr_a_bus_cd_cmd_log() 
#endif

//////////////////////////////////////////////////////////////////////////////

void FASTCALL ygr_a_bus_write_word(SH2_struct * sh, u32 addr, u16 val) {
   addr &= 0xFFFFF; // fix me(I should really have proper mapping)

   switch (addr) {
   case 0x90008:
   case 0x9000A:
      ygr_cxt.regs.HIRQ &= val;
      return;
   case 0x9000C:
   case 0x9000E: 
      ygr_cxt.regs.HIRQMASK = val;
      return;
   case 0x90018:
   case 0x9001A: 
      ygr_cxt.regs.CR1 = val;
      return;
   case 0x9001C:
   case 0x9001E: 
      ygr_cxt.regs.CR2 = val;
      return;
   case 0x90020:
   case 0x90022: 
      ygr_cxt.regs.CR3 = val;
      return;
   case 0x90024:
   case 0x90026: 
      ygr_cxt.regs.CR4 = val;
      SH2SendInterrupt(SH1, 70, (sh1_cxt.onchip.intc.iprb >> 4) & 0xf);
      ygr_a_bus_cd_cmd_log();
      return;
   case 0x90028:
   case 0x9002A: 
      ygr_cxt.regs.MPEGRGB = val;
      return;
   default:
      LOG("ygr\t:Undocumented register write %08X\n", addr);
      break;
   }
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL ygr_a_bus_read_long(SH2_struct * sh, u32 addr) {
   u32 val = 0;
   addr &= 0xFFFFF; // fix me(I should really have proper mapping)

   switch (addr) {
   case 0x90008:
      //2 copies?
      return ((ygr_cxt.regs.HIRQ << 16) | ygr_cxt.regs.HIRQ);
   case 0x9000C: 
      return ((ygr_cxt.regs.HIRQMASK << 16) | ygr_cxt.regs.HIRQMASK);
   case 0x90018: 
      return ((ygr_cxt.regs.CR1 << 16) | ygr_cxt.regs.CR1);
   case 0x9001C: 
      return ((ygr_cxt.regs.CR2 << 16) | ygr_cxt.regs.CR2);
   case 0x90020: 
      return ((ygr_cxt.regs.CR3 << 16) | ygr_cxt.regs.CR3);
   case 0x90024: 
      return ((ygr_cxt.regs.CR4 << 16) | ygr_cxt.regs.CR4);
   case 0x90028:
      return ((ygr_cxt.regs.MPEGRGB << 16) | ygr_cxt.regs.MPEGRGB);
   case 0x18000:
   {
      u32 top;
      while ((sh1_cxt.onchip.dmac.channel[1].chcr & 2) ||
             !(sh1_cxt.onchip.dmac.channel[1].chcr & 1)) {
         Cs2Exec(200);
         SH2Exec(SH1, 200);
      }

      sh1_dreq_asserted(1);
      top = ygr_cxt.fake_fifo;
      sh1_dreq_asserted(1);
      top <<= 16;
      top |= ygr_cxt.fake_fifo;
      return top;
   }
   default:
      LOG("ygr\t: Undocumented register read %08X\n", addr);
      break;
   }

   return val;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL ygr_a_bus_write_long(SH2_struct * sh, UNUSED u32 addr, UNUSED u32 val) {
   addr &= 0xFFFFF; // fix me(I should really have proper mapping)

   switch (addr)
   {
   case 0x18000:
      // transfer data
      break;
   default:
      LOG("ygr\t: Undocumented register write %08X\n", addr);
      //         T3WriteLong(Cs2Area->mem, addr, val);
      break;
   }
}

void ygr_cd_irq(u8 flags)
{
   ygr_cxt.cdirq_flags |= flags;
   if (ygr_cxt.cdirq_flags & ygr_cxt.regs.HIRQMASK)
      SH2SendInterrupt(SH1, 71, sh1_cxt.onchip.intc.iprb & 0xf);
}
