#pragma once

#include <hexrays.hpp>
#define CHECKED_BUILD
#include <pro.h>
#include <fpro.h>
#include <windows.h>
#include <ida.hpp>
#include <idp.hpp>
#include <ieee.h>
#include <loader.hpp>
#include <kernwin.hpp>
#include <typeinf.hpp>
#include <set>
#include <map>
#include <deque>
#include <queue>
#include <algorithm>
#pragma pack(push, 4)
#ifdef __VC__
#pragma warning(push)
#pragma warning(disable:4062) // enumerator 'x' in switch of enum 'y' is not handled
#pragma warning(disable:4265) // virtual functions without virtual destructor
#endif

const type_t *hexapi remove_typedef(const type_t *type, const p_list **fields=NULL);

inline const type_t *remove_typedef(const type_t *type, const p_list **fields)
{
  return (const type_t *)hexdsp(hx_remove_typedef, type, fields);
}

inline int partial_type_num(const type_t *type)
{
  return (int)hexdsp(hx_partial_type_num, type);
}



//--------------------------------------------------------------------------
inline bool is_type_small_struni(const type_t *ptr)
{
  return (bool)(size_t)hexdsp(hx_is_type_small_struni, ptr);
}


//------------------------------------------------------------------------
inline const type_t *skip_ptr_type_header(const type_t *type)
{
  if ( get_type_flags(*type++) == BTMT_CLOSURE
    && *type++ != RESERVED_BYTE ) type++; // skip reserved byte (and possibly sizeof(ptr))
  return type;
}

inline const type_t *skip_array_type_header(const type_t *type)
{
  if ( get_type_flags(*type++) & BTMT_NONBASED )
  {
    int n = get_dt(&type);
    if ( n < 0 )
      type = NULL;
  }
  else
  {
    ulong num, base;
    if ( !get_da(&type, &num, &base) )
      type = NULL;
  }
  return type;
}

/// Skip pointer or array type header.
/// \return pointer to the pointed object or array element.
inline const type_t *skip_ptr_or_array_header(const type_t *t)
{
  if ( is_type_ptr(*t) )
    return skip_ptr_type_header(t);
  else
    return skip_array_type_header(t);
}

inline type_t *typend(const type_t *ptr)  { return (type_t *)strchr((char *)ptr, '\0'); }
inline size_t typlen(const type_t *ptr)  { return strlen((const char *)ptr); }
inline type_t *typncpy(type_t *dst, const type_t *src, size_t size)
        { return (type_t *)qstrncpy((char *)dst, (const char *)src, size); }
inline type_t *tppncpy(type_t *dst, const type_t *src, size_t size)
        { return (type_t *)qstpncpy((char *)dst, (const char *)src, size); }
inline int     typcmp(const type_t *dst, const type_t *src)
        { return strcmp((const char *)dst, (const char *)src); }
inline type_t *typdup(const type_t *src)
        { return (type_t *)qstrdup((const char *)src); }

/// Type string.
/// All types in the decompiler are kept in instances of this class.
/// The contents of the type string are described in typeinf.hpp\n
/// In fact, type information consists of two strings: the type string
/// itself and another string with the field names. The first string (pure
/// type string) keeps information about the type without user-defined names.
/// The second string keeps user-defined names. The second string is required for
/// structures and unions (to keep the field names), enumerations (to keep the
/// constant names), and functions (to keep the argument names). In all other cases
/// the field string can be empty or missing.
struct typestring : public qtype
{
  const type_t *u_str(void) const
  {
    return (const type_t *)c_str();
  }
  typestring(void) {}
  typestring(const type_t *str) : qtype(str ? str : (type_t*)"")
  {
  }
  typestring(const type_t *str, int n) : qtype(str ? str : (type_t*)"", n)
  {
  }
  typestring(const qtype &str) : qtype(str)
  {
  }
  typestring(const qstring &str) : qtype((type_t *)str.c_str())
  {
  }
  const char *idaapi dstr(void) const;
  const char *idaapi dstr(const p_list *fields, const char *name=NULL) const;
  const char *idaapi dstr(const qtype &fields, const char *name=NULL) const
    { return dstr(fields.c_str(), name); }
  size_t hexapi print(char *buf, size_t bufsize) const;
  qstring multiprint(const p_list *fields, const char *name=NULL, int prflags=0) const;
  int size(void) const
  {
    return get_type_size0(idati, u_str());
  }
  int noarray_size(void) const;
  type_sign_t get_sign(void) const
  {
    return get_type_sign(idati, u_str());
  }
  bool hexapi change_sign(type_sign_t sign);
  cm_t hexapi get_cc(void) const;
  bool is_user_cc(void) const { return ::is_user_cc(get_cc()); }
  bool is_vararg(void) const { return ::is_vararg_cc(get_cc()); }
  typestring hexapi get_nth_arg(int n) const;
  DECLARE_COMPARISON_OPERATORS(typestring)
  int compare(const typestring &r) const { return typcmp(u_str(), r.u_str()); }
  bool common_type(const typestring &x);
  bool is_ptr_or_array(void) const
  {
    const type_t *t2 = resolve();
    return t2 != NULL && ::is_ptr_or_array(t2[0]);
  }
  bool remove_ptr_or_array(void)
  {
    const type_t *t2 = resolve();
    if ( t2 == NULL || !::is_ptr_or_array(t2[0]) )
      return false;
    t2 = skip_ptr_or_array_header(t2);
    *this = t2;
    return true;
  }
  bool is_paf(void) const
  {
    const type_t *t2 = resolve();
    return t2 != NULL && ::is_paf(t2[0]);
  }
  bool is_ptr(void) const      { const type_t *ptr = resolve(); return ptr != NULL && is_type_ptr(*ptr); }
  bool is_enum(void) const     { const type_t *ptr = resolve(); return ptr != NULL && is_type_enum(*ptr); }
  bool is_func(void) const     { const type_t *ptr = resolve(); return ptr != NULL && is_type_func(*ptr); }
  bool is_void(void) const     { const type_t *ptr = resolve(); return ptr != NULL && is_type_void(*ptr); }
  bool is_array(void) const    { const type_t *ptr = resolve(); return ptr != NULL && is_type_array(*ptr); }
  bool is_float(void) const    { const type_t *ptr = resolve(); return ptr != NULL && is_type_float(*ptr); }
  bool is_union(void) const    { const type_t *ptr = resolve(); return ptr != NULL && is_type_union(*ptr); }
  bool is_struct(void) const   { const type_t *ptr = resolve(); return ptr != NULL && is_type_struct(*ptr); }
  bool is_struni(void) const   { const type_t *ptr = resolve(); return ptr != NULL && is_type_struni(*ptr); }
  bool is_double(void) const   { const type_t *ptr = resolve(); return ptr != NULL && is_type_double(*ptr); }
  bool is_ldouble(void) const  { const type_t *ptr = resolve(); return ptr != NULL && is_type_ldouble(*ptr); }
  bool is_floating(void) const { const type_t *ptr = resolve(); return ptr != NULL && is_type_floating(*ptr); }
  bool is_correct(void) const { return empty() || is_type_correct(u_str()); }
  const type_t *resolve(void) const { return remove_typedef(u_str()); }
  const type_t *resolve_func_type(const p_list **fields=NULL) const;
  bool is_scalar(void) const { return is_type_scalar2(idati, u_str()); }
  bool is_small_struni(void) const { return is_type_small_struni(u_str()); }
  bool is_like_scalar(void) const
  {
    const type_t *ptr = resolve();
    return is_type_scalar2(idati, ptr) || is_type_small_struni(ptr);
  }
  bool is_pvoid(void) const
  {
    const type_t *ptr = resolve();
    if ( !is_type_ptr(*ptr) )
      return false;
    ptr = skip_ptr_type_header(ptr);
    if ( !is_restype_void(idati, ptr) )
      return false;
    return true;
  }
  bool is_partial_ptr(void) const
  {
    const type_t *ptr = u_str();
    if ( !is_type_ptr(*ptr) )
      return false;
    ptr = skip_ptr_type_header(ptr);
    if ( !is_type_partial(*ptr) )
      return false;
    return true;
  }
  bool is_well_defined(void) const
  {
    if ( empty() )
      return false;
    return !is_type_partial((*this)[0]);
  }
  bool requires_cot_ref(void) const
  {
    // arrays and functions do not require &
    const type_t *t2 = resolve();
    if ( t2 == NULL )
      return true;
    type_t t = *t2;
    return !is_type_array(t) && !is_type_func(t);
  }
  int partial_type_num(void) const { return ::partial_type_num(u_str()); }
};

//--------------------------------------------------------------------------
inline typestring make_array(const typestring &type, int nelems)
{
  typestring retval;
  hexdsp(hx_make_array, &retval, &type, nelems);
  return retval;
}

//--------------------------------------------------------------------------
inline typestring make_pointer(const typestring &type)
{
  typestring retval;
  hexdsp(hx_make_pointer, &retval, &type);
  return retval;
}


struct meminfo_t
{
  DEFINE_MEMORY_ALLOCATION_FUNCS()
  uval_t offset;
  int size;               // not used by build_struct_type, just for speed
  typestring type;
  typestring fields;
  qstring name;
};

struct strtype_info_t : public qvector<meminfo_t>
{
  type_t basetype;
  int N;                // N from the type string
  bool hexapi build_base_type(typestring *restype) const;
  bool hexapi build_udt_type(typestring *restype, typestring *resfields);
};
