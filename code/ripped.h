#pragma once

#include <hexrays.hpp>
#include "helpers.h"
#include "defs.h"

signed int __cdecl log2ceil(__int64 a1);

//extern typestring t_char;
//extern typestring t_byte;

#if IDA_SDK_VERSION >= 630

//v6 0x170C8AA0, ... v61 0x17035E90
/*extern C<qstring  __cdecl (typestring *a2, int offset), 0x17035E90> create_field_name;*/

//v6 0x170C96D0 ... v61 0x17034C50
extern C<typestring  __cdecl (const type_t *), 0x17034C50> dummy_plist_for;

//V6 0x170C95C0 ... V61 17034B00
extern C<typestring __cdecl (int a2), 0x17034B00> make_dt_;
#endif
extern tinfo_t make_dt(int a2);

//C<int __cdecl (type_t *, uint , const char *a3), 0x170C7230> make_dtname;
extern int make_dtname(type_t * vysl, int max, const char *name);

#pragma pack(push, 1)
struct mba_t_
{
  _DWORD dword0;
  _DWORD ea;
  _BYTE f8[20];
  _DWORD dword1C;
  _DWORD dword20;
  int f24;
  int field_28;
  int field_2C;
  int field_30;
  _DWORD dword34;
  char f38[64];
  char field_78;
  char field_79;
  char field_7A;
  char field_7B;
  int field_7C;
  int field_80;
  char field_84[32];
  tinfo_t typestring;
  char gap_B0[4];
  lvar_t *lvar_array;
  _DWORD max_idx;
  int field_BC;
  int field_C0;
  int field_C4;
  int field_C8;
  int field_CC;
  int field_D0;
  int field_D4;
  int field_D8;
  int field_DC;
  int field_E0;
  int field_E4;
  int field_E8;
  int field_EC;
  int field_F0;
  int field_F4;
  int field_F8;
  int field_FC;
  int field_100;
  int field_104;
  int field_108;
  int field_10C;
  int field_110;
  int field_114;
  int field_118;
  char gap_11C[32];
  int field_13C;
  int field_140[4];
  int field_150;
  int field_154;
  int field_158;
  int field_15C;
};
#pragma pack(pop)
