/*
 * Copyright (c) 2003, 2007-8 Matteo Frigo
 * Copyright (c) 2003, 2007-8 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#if defined(FFTW_LDOUBLE) || defined(FFTW_QUAD)
#  error "SSE/SSE2 only works in single/double precision"
#endif

#ifdef FFTW_SINGLE
#  define DS(d,s) s /* single-precision option */
#  define SUFF(name) name ## s
#else
#  define DS(d,s) d /* double-precision option */
#  define SUFF(name) name ## d
#endif

#define SIMD_SUFFIX  _sse2  /* for renaming */
#define VL DS(1,2)         /* SIMD vector length, in term of complex numbers */
#define SIMD_VSTRIDE_OKA(x) DS(1,((x) == 2))
#define SIMD_STRIDE_OKPAIR SIMD_STRIDE_OK

#if defined(__GNUC__) && !defined(FFTW_SINGLE) && !defined(__SSE2__)
#  error "compiling simd-sse2.h in double precision without -msse2"
#elif defined(__GNUC__) && defined(FFTW_SINGLE) && !defined(__SSE__)
#  error "compiling simd-sse2.h in single precision without -msse"
#endif

/* some versions of glibc's sys/cdefs.h define __inline to be empty,
   which is wrong because emmintrin.h defines several inline
   procedures */
#undef __inline

#ifdef FFTW_SINGLE
#  include <xmmintrin.h>
#else
#  include <emmintrin.h>
#endif

typedef DS(__m128d,__m128) V;
#define VADD SUFF(_mm_add_p)
#define VSUB SUFF(_mm_sub_p)
#define VMUL SUFF(_mm_mul_p)
#define VXOR SUFF(_mm_xor_p)
#define SHUF SUFF(_mm_shuffle_p)
#define UNPCKL SUFF(_mm_unpacklo_p)
#define UNPCKH SUFF(_mm_unpackhi_p)

#ifdef FFTW_SINGLE
#  define STOREH(addr, val) _mm_storeh_pi((__m64 *)(addr), val)
#  define STOREL(addr, val) _mm_storel_pi((__m64 *)(addr), val)
#else
#  define STOREH _mm_storeh_pd
#  define STOREL _mm_storel_pd
#endif

#ifdef __GNUC__
#  ifdef FFTW_SINGLE
#    define DVK(var, val) V var = __extension__ ({ \
         static const union rvec _var = { {val,val,val,val} }; _var.v; })
#  else
#    define DVK(var, val) V var = __extension__ ({ \
         static const union rvec _var = { {val,val} }; _var.v; })
#  endif
#  define LDK(x) x
#else
#  define DVK(var, val) const R var = K(val)
#  define LDK(x) DS(_mm_set1_pd,_mm_set_ps1)(x)
#endif

union rvec {
     R r[DS(2,4)];
     V v;
};

union uvec {
     unsigned u[4];
     V v;
};

#define SHUFVAL(fp0,fp1,fp2,fp3) \
   (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

static inline V LDA(const R *x, INT ivs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     (void)ivs; /* UNUSED */
     return *(const V *)x;
}

static inline void STA(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     (void)ovs; /* UNUSED */
     *(V *)x = v;
}

#ifdef FFTW_SINGLE

static inline V LD(const R *x, INT ivs, const R *aligned_like)
{
     V var;
     (void)aligned_like; /* UNUSED */
#  ifdef __GNUC__
     /* We use inline asm because gcc generates slow code for
	_mm_loadh_pi().  gcc insists upon having an existing variable for
	VAL, which is however never used.  Thus, it generates code to move
	values in and out the variable.  Worse still, gcc-4.0 stores VAL on
	the stack, causing valgrind to complain about uninitialized reads. */  
     __asm__("movlps %1, %0\n\tmovhps %2, %0"
	     : "=x"(var) : "m"(x[0]), "m"(x[ivs]));
#  else
#    define LOADH(addr, val) _mm_loadh_pi(val, (const __m64 *)(addr))
#    define LOADL0(addr, val) _mm_loadl_pi(val, (const __m64 *)(addr))
     var = LOADL0(x, var);
     var = LOADH(x + ivs, var);
#  endif
     return var;
}

static inline void ST(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     /* WARNING: the extra_iter hack depends upon STOREL occurring
	after STOREH */
     STOREH(x + ovs, v);
     STOREL(x, v);
}

#else /* ! FFTW_SINGLE */
#  define LD LDA
#  define ST STA
#endif

#define STM2 DS(STA,ST)
#define STN2(x, v0, v1, ovs) /* nop */

#ifdef FFTW_SINGLE
#  define STM4(x, v, ovs, aligned_like) /* no-op */
/* STN4 is a macro, not a function, thanks to Visual C++ developers
   deciding "it would be infrequent that people would want to pass more
   than 3 [__m128 parameters] by value."  3 parameters ought to be enough
   for anybody. */
#  define STN4(x, v0, v1, v2, v3, ovs)			\
{							\
     V xxx0, xxx1, xxx2, xxx3;				\
     xxx0 = UNPCKL(v0, v2);				\
     xxx1 = UNPCKH(v0, v2);				\
     xxx2 = UNPCKL(v1, v3);				\
     xxx3 = UNPCKH(v1, v3);				\
     STA(x, UNPCKL(xxx0, xxx2), 0, 0);			\
     STA(x + ovs, UNPCKH(xxx0, xxx2), 0, 0);		\
     STA(x + 2 * ovs, UNPCKL(xxx1, xxx3), 0, 0);	\
     STA(x + 3 * ovs, UNPCKH(xxx1, xxx3), 0, 0);	\
}
#else /* !FFTW_SINGLE */
static inline void STM4(R *x, V v, INT ovs, const R *aligned_like)
{
     (void)aligned_like; /* UNUSED */
     STOREL(x, v);
     STOREH(x + ovs, v);
}
#  define STN4(x, v0, v1, v2, v3, ovs) /* nothing */
#endif


static inline V FLIP_RI(V x)
{
     return SHUF(x, x, DS(1, SHUFVAL(1, 0, 3, 2)));
}

extern const union uvec X(sse2_pm);
static inline V VCONJ(V x)
{
     return VXOR(X(sse2_pm).v, x);
}

static inline V VBYI(V x)
{
     x = VCONJ(x);
     x = FLIP_RI(x);
     return x;
}

#ifdef FFTW_SINGLE
#  define UNPCKR(x) SHUF(x, x, SHUFVAL(0, 0, 2, 2))
#  define UNPCKI(x) SHUF(x, x, SHUFVAL(1, 1, 3, 3))
#else
#  define UNPCKR(x) UNPCKL(x, x)
#  define UNPCKI(x) UNPCKH(x, x)
#endif

static inline V VZMUL(V tx, V sr)
{
     V tr = UNPCKR(tx);
     V ti = UNPCKI(tx);
     tr = VMUL(sr, tr);
     sr = VBYI(sr);
     return VADD(tr, VMUL(ti, sr));
}

static inline V VZMULJ(V tx, V sr)
{
     V tr = UNPCKR(tx);
     V ti = UNPCKI(tx);
     tr = VMUL(sr, tr);
     sr = VBYI(sr);
     return VSUB(tr, VMUL(ti, sr));
}

static inline V VZMULI(V tx, V sr)
{
     V tr = UNPCKR(tx);
     V ti = UNPCKI(tx);
     ti = VMUL(ti, sr);
     sr = VBYI(sr);
     return VSUB(VMUL(tr, sr), ti);
}

static inline V VZMULIJ(V tx, V sr)
{
     V tr = UNPCKR(tx);
     V ti = UNPCKI(tx);
     ti = VMUL(ti, sr);
     sr = VBYI(sr);
     return VADD(VMUL(tr, sr), ti);
}

/* twiddle storage #1: compact, slower */
#ifdef FFTW_SINGLE
#  define VTW1(v,x)  \
  {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_SIN, v, x}, {TW_SIN, v+1, x}
static inline V BYTW1(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V tx = twp[0];
     V tr = UNPCKL(tx, tx);
     V ti = UNPCKH(tx, tx);
     tr = VMUL(tr, sr);
     sr = VBYI(sr);
     return VADD(tr, VMUL(ti, sr));
}
static inline V BYTWJ1(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V tx = twp[0];
     V tr = UNPCKL(tx, tx);
     V ti = UNPCKH(tx, tx);
     tr = VMUL(tr, sr);
     sr = VBYI(sr);
     return VSUB(tr, VMUL(ti, sr));
}
#else /* !FFTW_SINGLE */
#  define VTW1(v,x) {TW_CEXP, v, x}
static inline V BYTW1(const R *t, V sr)
{
     V tx = LD(t, 1, t);
     return VZMUL(tx, sr);
}
static inline V BYTWJ1(const R *t, V sr)
{
     V tx = LD(t, 1, t);
     return VZMULJ(tx, sr);
}
#endif
#define TWVL1 (VL)

/* twiddle storage #2: twice the space, faster (when in cache) */
#ifdef FFTW_SINGLE
#  define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+1, x},	\
  {TW_SIN, v, -x}, {TW_SIN, v, x}, {TW_SIN, v+1, -x}, {TW_SIN, v+1, x}
#else /* !FFTW_SINGLE */
#  define VTW2(v,x)							\
  {TW_COS, v, x}, {TW_COS, v, x}, {TW_SIN, v, -x}, {TW_SIN, v, x}
#endif
#define TWVL2 (2 * VL)
static inline V BYTW2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
     return VADD(VMUL(tr, sr), VMUL(ti, si));
}
static inline V BYTWJ2(const R *t, V sr)
{
     const V *twp = (const V *)t;
     V si = FLIP_RI(sr);
     V tr = twp[0], ti = twp[1];
     return VSUB(VMUL(tr, sr), VMUL(ti, si));
}

/* twiddle storage #3 */
#ifdef FFTW_SINGLE
#  define VTW3(v,x) {TW_CEXP, v, x}, {TW_CEXP, v+1, x}
#  define TWVL3 (VL)
#else
#  define VTW3(v,x) VTW1(v,x)
#  define TWVL3 TWVL1
#endif

/* twiddle storage for split arrays */
#ifdef FFTW_SINGLE
#  define VTWS(v,x)							  \
    {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_COS, v+2, x}, {TW_COS, v+3, x}, \
    {TW_SIN, v, x}, {TW_SIN, v+1, x}, {TW_SIN, v+2, x}, {TW_SIN, v+3, x}
#else
#  define VTWS(v,x)							  \
    {TW_COS, v, x}, {TW_COS, v+1, x}, {TW_SIN, v, x}, {TW_SIN, v+1, x}
#endif
#define TWVLS (2 * VL)

/* FMA macros */
#define VFMA(a, b, c) VADD(c, VMUL(a, b))
#define VFNMS(a, b, c) VSUB(c, VMUL(a, b))
#define VFMS(a, b, c) VSUB(VMUL(a, b), c)
#define VFMAI(b, c) VADD(c, VBYI(b))
#define VFNMSI(b, c) VSUB(c, VBYI(b))
#define VFMACONJ(b,c)  VADD(VCONJ(b),c)
#define VFMSCONJ(b,c)  VSUB(VCONJ(b),c)
#define VFNMSCONJ(b,c) VSUB(c, VCONJ(b))

#define VLEAVE() /* nothing */

#include "simd-common.h"
