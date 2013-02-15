/*
 * Copyright (c) 2012 Rob Clark <robdclark@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "redump.h"
#include "disasm.h"


/* ************************************************************************* */
/* originally based on kernel recovery dump code: */
#include "a2xx_reg.h"
#include "freedreno_a2xx_reg.h"
#include "a3xx_reg.h"
#include "freedreno_a3xx_reg.h"
#include "adreno_pm4types.h"

typedef enum {
	true = 1, false = 0,
} bool;

static bool dump_shaders = false;

static const char *levels[] = {
		"\t",
		"\t\t",
		"\t\t\t",
		"\t\t\t\t",
		"\t\t\t\t\t",
		"\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t",
		"x",
		"x",
		"x",
		"x",
		"x",
		"x",
};

#define NAME(x)	[x] = #x

static const char *event_name[] = {
		NAME(VS_DEALLOC),
		NAME(PS_DEALLOC),
		NAME(VS_DONE_TS),
		NAME(PS_DONE_TS),
		NAME(CACHE_FLUSH_TS),
		NAME(CONTEXT_DONE),
		NAME(CACHE_FLUSH),
		NAME(VIZQUERY_START),
		NAME(VIZQUERY_END),
		NAME(SC_WAIT_WC),
		NAME(RST_PIX_CNT),
		NAME(RST_VTX_CNT),
		NAME(TILE_FLUSH),
		NAME(CACHE_FLUSH_AND_INV_TS_EVENT),
		NAME(ZPASS_DONE),
		NAME(CACHE_FLUSH_AND_INV_EVENT),
		NAME(PERFCOUNTER_START),
		NAME(PERFCOUNTER_STOP),
		NAME(VS_FETCH_DONE),
		NAME(FACENESS_FLUSH),
};

static const char *color_name[] = {
		NAME(COLORX_4_4_4_4),
		NAME(COLORX_1_5_5_5),
		NAME(COLORX_5_6_5),
		NAME(COLORX_8),
		NAME(COLORX_8_8),
		NAME(COLORX_8_8_8_8),
		NAME(COLORX_S8_8_8_8),
		NAME(COLORX_16_FLOAT),
		NAME(COLORX_16_16_FLOAT),
		NAME(COLORX_16_16_16_16_FLOAT),
		NAME(COLORX_32_FLOAT),
		NAME(COLORX_32_32_FLOAT),
		NAME(COLORX_32_32_32_32_FLOAT),
		NAME(COLORX_2_3_3),
		NAME(COLORX_8_8_8),
};

static const char *fmt_name[] = {
		NAME(FMT_1_REVERSE),
		NAME(FMT_1),
		NAME(FMT_8),
		NAME(FMT_1_5_5_5),
		NAME(FMT_5_6_5),
		NAME(FMT_6_5_5),
		NAME(FMT_8_8_8_8),
		NAME(FMT_2_10_10_10),
		NAME(FMT_8_A),
		NAME(FMT_8_B),
		NAME(FMT_8_8),
		NAME(FMT_Cr_Y1_Cb_Y0),
		NAME(FMT_Y1_Cr_Y0_Cb),
		NAME(FMT_5_5_5_1),
		NAME(FMT_8_8_8_8_A),
		NAME(FMT_4_4_4_4),
		NAME(FMT_10_11_11),
		NAME(FMT_11_11_10),
		NAME(FMT_DXT1),
		NAME(FMT_DXT2_3),
		NAME(FMT_DXT4_5),
		NAME(FMT_24_8),
		NAME(FMT_24_8_FLOAT),
		NAME(FMT_16),
		NAME(FMT_16_16),
		NAME(FMT_16_16_16_16),
		NAME(FMT_16_EXPAND),
		NAME(FMT_16_16_EXPAND),
		NAME(FMT_16_16_16_16_EXPAND),
		NAME(FMT_16_FLOAT),
		NAME(FMT_16_16_FLOAT),
		NAME(FMT_16_16_16_16_FLOAT),
		NAME(FMT_32),
		NAME(FMT_32_32),
		NAME(FMT_32_32_32_32),
		NAME(FMT_32_FLOAT),
		NAME(FMT_32_32_FLOAT),
		NAME(FMT_32_32_32_32_FLOAT),
		NAME(FMT_32_AS_8),
		NAME(FMT_32_AS_8_8),
		NAME(FMT_16_MPEG),
		NAME(FMT_16_16_MPEG),
		NAME(FMT_8_INTERLACED),
		NAME(FMT_32_AS_8_INTERLACED),
		NAME(FMT_32_AS_8_8_INTERLACED),
		NAME(FMT_16_INTERLACED),
		NAME(FMT_16_MPEG_INTERLACED),
		NAME(FMT_16_16_MPEG_INTERLACED),
		NAME(FMT_DXN),
		NAME(FMT_8_8_8_8_AS_16_16_16_16),
		NAME(FMT_DXT1_AS_16_16_16_16),
		NAME(FMT_DXT2_3_AS_16_16_16_16),
		NAME(FMT_DXT4_5_AS_16_16_16_16),
		NAME(FMT_2_10_10_10_AS_16_16_16_16),
		NAME(FMT_10_11_11_AS_16_16_16_16),
		NAME(FMT_11_11_10_AS_16_16_16_16),
		NAME(FMT_32_32_32_FLOAT),
		NAME(FMT_DXT3A),
		NAME(FMT_DXT5A),
		NAME(FMT_CTX1),
		NAME(FMT_DXT3A_AS_1_1_1_1),
};

static const char *vgt_prim_types[32] = {
		NAME(DI_PT_NONE),
		NAME(DI_PT_POINTLIST),
		NAME(DI_PT_LINELIST),
		NAME(DI_PT_LINESTRIP),
		NAME(DI_PT_TRILIST),
		NAME(DI_PT_TRIFAN),
		NAME(DI_PT_TRISTRIP),
		NAME(DI_PT_RECTLIST),
		NAME(DI_PT_QUADLIST),
		NAME(DI_PT_QUADSTRIP),
		NAME(DI_PT_POLYGON),
		NAME(DI_PT_2D_COPY_RECT_LIST_V0),
		NAME(DI_PT_2D_COPY_RECT_LIST_V1),
		NAME(DI_PT_2D_COPY_RECT_LIST_V2),
		NAME(DI_PT_2D_COPY_RECT_LIST_V3),
		NAME(DI_PT_2D_FILL_RECT_LIST),
		NAME(DI_PT_2D_LINE_STRIP),
		NAME(DI_PT_2D_TRI_STRIP),
};

static const char *vgt_source_select[4] = {
		NAME(DI_SRC_SEL_DMA),
		NAME(DI_SRC_SEL_IMMEDIATE),
		NAME(DI_SRC_SEL_AUTO_INDEX),
		NAME(DI_SRC_SEL_RESERVED),
};

static const char *dither_mode_name[4] = {
		NAME(DITHER_DISABLE),
		NAME(DITHER_ALWAYS),
		NAME(DITHER_IF_ALPHA_OFF),
};
static const char *dither_type_name[4] = {
		NAME(DITHER_PIXEL),
		NAME(DITHER_SUBPIXEL),
};

static const char *stencil_op[8] = {
		NAME(STENCIL_KEEP),
		NAME(STENCIL_ZERO),
		NAME(STENCIL_REPLACE),
		NAME(STENCIL_INCR_CLAMP),
		NAME(STENCIL_DECR_CLAMP),
		NAME(STENCIL_INVERT),
		NAME(STENCIL_INCR_WRAP),
		NAME(STENCIL_DECR_WRAP),
};

static void dump_commands(uint32_t *dwords, uint32_t sizedwords, int level);

struct buffer {
	void *hostptr;
	unsigned int gpuaddr, len;
};

static struct buffer buffers[32];
static int nbuffers;

static int buffer_contains_gpuaddr(struct buffer *buf, uint32_t gpuaddr, uint32_t len)
{
	return (buf->gpuaddr <= gpuaddr) && (gpuaddr < (buf->gpuaddr + buf->len));
}

static int buffer_contains_hostptr(struct buffer *buf, void *hostptr)
{
	return (buf->hostptr <= hostptr) && (hostptr < (buf->hostptr + buf->len));
}

#define GET_PM4_TYPE3_OPCODE(x) ((*(x) >> 8) & 0xFF)
#define GET_PM4_TYPE0_REGIDX(x) ((*(x)) & 0x7FFF)

static uint32_t gpuaddr(void *hostptr)
{
	int i;
	for (i = 0; i < nbuffers; i++)
		if (buffer_contains_hostptr(&buffers[i], hostptr))
			return buffers[i].gpuaddr + (hostptr - buffers[i].hostptr);
	return 0;
}

static void *hostptr(uint32_t gpuaddr)
{
	int i;
	for (i = 0; i < nbuffers; i++)
		if (buffer_contains_gpuaddr(&buffers[i], gpuaddr, 0))
			return buffers[i].hostptr + (gpuaddr - buffers[i].gpuaddr);
	return 0;
}

static void dump_hex(uint32_t *dwords, uint32_t sizedwords, int level)
{
	int i;
	for (i = 0; i < sizedwords; i++) {
		if ((i % 8) == 0)
			printf("%08x:%s", gpuaddr(dwords), levels[level]);
		else
			printf(" ");
		printf("%08x", *(dwords++));
		if ((i % 8) == 7)
			printf("\n");
	}
	if (i % 8)
		printf("\n");
}

static void dump_float(float *dwords, uint32_t sizedwords, int level)
{
	int i;
	for (i = 0; i < sizedwords; i++) {
		if ((i % 8) == 0)
			printf("%08x:%s", gpuaddr(dwords), levels[level]);
		else
			printf(" ");
		printf("%8f", *(dwords++));
		if ((i % 8) == 7)
			printf("\n");
	}
	if (i % 8)
		printf("\n");
}

/* I believe the surface format is low bits:
#define RB_COLOR_INFO__COLOR_FORMAT_MASK                   0x0000000fL
comments in sys2gmem_tex_const indicate that address is [31:12], but
looks like at least some of the bits above the format have different meaning..
*/
static void parse_dword_addr(uint32_t dword, uint32_t *gpuaddr,
		uint32_t *flags, uint32_t mask)
{
	*gpuaddr = dword & ~mask;
	*flags   = dword & mask;
}


#define INVALID_RB_CMD 0xaaaaaaaa

/* CP timestamp register */
#define	REG_CP_TIMESTAMP		 REG_SCRATCH_REG0

static void reg_hex(const char *name, uint32_t dword, int level)
{
	printf("%s%s: %08x (%d)\n", levels[level], name, dword, dword);
}

static inline float d2f(uint32_t d)
{
	union {
		float f;
		uint32_t d;
	} u = {
		.d = d,
	};
	return u.f;
}

static void reg_float(const char *name, uint32_t dword, int level)
{
	printf("%s%s: %f (%08x)\n", levels[level], name, d2f(dword), dword);
}

static void reg_gpuaddr(const char *name, uint32_t dword, int level)
{
	uint32_t gpuaddr, flags;
	parse_dword_addr(dword, &gpuaddr, &flags, 0xfff);
	printf("%s%s: %08x (%x)\n", levels[level], name, gpuaddr, flags);
}

static void reg_rb_copy_dest_info(const char *name, uint32_t dword, int level)
{
	/* Endian=none, Linear, Format=RGBA8888,Swap=0,!Dither,
	 *  MaskWrite:R=G=B=A=1
	 *   0x0003c008 | (format << 4)
	 */
	static const char *endian_name[8] = {
			"none", "8in16", "8in32", "16in32", "8in64", "8in128",
	};
	uint32_t endian = dword & 0x7;
	uint32_t format = (dword >> 4) & 0xf;
	uint32_t swap   = (dword >> 8) & 0x3;
	uint32_t dither_mode = (dword >> 10) & 0x3;
	uint32_t dither_type = (dword >> 12) & 0x3;
	uint32_t write_mask  = (dword >> 14) & 0xf;
	printf("%s%s: endian=%s, format=%s, swap=%x, dither-mode=%s, dither-type=%s, write-mask=%x (%08x)\n",
			levels[level], name, endian_name[endian], color_name[format], swap,
			dither_mode_name[dither_mode], dither_type_name[dither_type],
			write_mask, dword);
}

static void reg_rb_copy_dest_pitch(const char *name, uint32_t dword, int level)
{
	uint32_t p = dword << 5;
	printf("%s%s: %d (%08x)\n", levels[level], name, p, dword);
}


static void reg_pa_su_sc_mode_cntl(const char *name, uint32_t dword, int level)
{
	static const char *ptype[] = {
			"points", "lines", "triangles", "???",
	};
	printf("%s%s: %08x (front-ptype=%s, back-ptype=%s, provoking-vtx=%s%s%s%s%s%s)\n",
			levels[level], name, dword,
			ptype[(dword >> 5) & 0x3], ptype[(dword >> 8) & 0x3],
			(dword & PA_SU_SC_MODE_CNTL_PROVOKING_VTX_LAST) ? "last" : "first",
			(dword & PA_SU_SC_MODE_CNTL_VTX_WINDOW_OFFSET_ENABLE) ? ", vtx-win-off-enable" : "",
			(dword & PA_SU_SC_MODE_CNTL_CULL_FRONT) ? ", cull-front" : "",
			(dword & PA_SU_SC_MODE_CNTL_CULL_BACK) ? ", cull-back" : "",
			(dword & PA_SU_SC_MODE_CNTL_POLY_OFFSET_FRONT_ENABLE) ? ", poly-offset-front" : "",
			(dword & PA_SU_SC_MODE_CNTL_POLY_OFFSET_BACK_ENABLE) ? ", poly-offset-back" : "");
}

static void reg_sq_program_cntl(const char *name, uint32_t dword, int level)
{
	static const char *vtx_mode[] = {
			"position-1-vector", "position-2-vectors-unused", "position-2-vectors-sprite",
			"position-2-vectors-edge", "position-2-vectors-kill", "position-2-vectors-sprite-kill",
			"position-2-vectors-edge-kill", "multipass",
	};
	uint32_t vs_regs = (dword & 0xff) + 1;
	uint32_t ps_regs = ((dword >> 8) & 0xff) + 1;
	uint32_t vs_export = ((dword >> 20) & 0xf) + 1;
	uint32_t vtx = (dword >> 24) & 0x7;
	if (vs_regs == 0x81)
		vs_regs = 0;
	if (ps_regs == 0x81)
		ps_regs = 0;
	printf("%s%s: %08x (vs-regs=%u, ps-regs=%u, vs-export=%u %s%s%s%s%s)\n",
			levels[level], name, dword, vs_regs, ps_regs, vs_export, vtx_mode[vtx],
			(dword & SQ_PROGRAM_CNTL_VS_RESOURCE) ? ", vs" : "",
			(dword & SQ_PROGRAM_CNTL_PS_RESOURCE) ? ", ps" : "",
			(dword & SQ_PROGRAM_CNTL_PARAM_GEN) ? ", param-gen" : "",
			(dword & SQ_PROGRAM_CNTL_GEN_INDEX_PIX) ? ", gen-index-pix" : "");
}

static const char *gl_func[] = {
		"GL_NEVER", "GL_LESS", "GL_EQUAL", "GL_LEQUAL",
		"GL_GREATER", "GL_NOTEQUAL", "GL_GEQUAL", "GL_ALWAYS",
};

static void reg_rb_colorcontrol(const char *name, uint32_t dword, int level)
{
	printf("%s%s: %08x (func=%s%s%s%s%s%s, rop=%d, dither-mode=%s, "
			"dither-type=%s%s)\n", levels[level], name, dword,
			gl_func[dword & 0x7],
			(dword & RB_COLORCONTROL_ALPHA_TEST_ENABLE) ? ", alpha-test" : "",
			(dword & RB_COLORCONTROL_ALPHA_TO_MASK_ENABLE) ? ", alpha-to-mask" : "",
			(dword & RB_COLORCONTROL_BLEND_DISABLE) ? "" : ", blend",
			(dword & RB_COLORCONTROL_FOG_ENABLE) ? ", fog" : "",
			(dword & RB_COLORCONTROL_VS_EXPORTS_FOG) ? ", vs-exports-fog" : "",
			(dword >> 8) & 0xf,
			dither_mode_name[(dword >> 12) & 0x3],
			dither_type_name[(dword >> 14) & 0x3],
			(dword & RB_COLORCONTROL_PIXEL_FOG) ? ", pixel-fog" : "");
}

static void reg_rb_depthcontrol(const char *name, uint32_t dword, int level)
{
	printf("%s%s: %08x (%s%s%s%szfunc=%s, %ssfunc=%s, sfail=%s, szpass=%s, "
			"szfail=%s, sfunc-bf=%s, sfail-bf=%s, szpass-bf=%s, szfail-bf=%s)\n",
			levels[level], name, dword,
			(dword & RB_DEPTHCONTROL_STENCIL_ENABLE) ? "stencil, " : "",
			(dword & RB_DEPTHCONTROL_Z_ENABLE) ? "z, " : "",
			(dword & RB_DEPTHCONTROL_Z_WRITE_ENABLE) ? "z-write, " : "",
			(dword & RB_DEPTHCONTROL_EARLY_Z_ENABLE) ? "early-z, " : "",
			gl_func[(dword >> 4) & 0x7],
			(dword & RB_DEPTHCONTROL_BACKFACE_ENABLE) ? "backface, " : "",
			gl_func[(dword >> 8) & 0x7],
			stencil_op[(dword >> 11) & 0x7],
			stencil_op[(dword >> 14) & 0x7],
			stencil_op[(dword >> 17) & 0x7],
			gl_func[(dword >> 20) & 0x7],
			stencil_op[(dword >> 23) & 0x7],
			stencil_op[(dword >> 26) & 0x7],
			stencil_op[(dword >> 29) & 0x7]);
}

static void reg_rb_depth_info(const char *name, uint32_t dword, int level)
{
	static const char *depth_format[] = {
			"DEPTHX_16", "DEPTHX_24_8",
	};
	uint32_t fmt = dword & 0x1;
	uint32_t base = dword >> 12;
	printf("%s%s: %08x (format=%s, base=%d)\n",
			levels[level], name, dword, depth_format[fmt], base);
}

static void reg_rb_blend_control(const char *name, uint32_t dword, int level)
{
	static const char *op[] = {
			NAME(RB_BLEND_ZERO),
			NAME(RB_BLEND_ONE),
			NAME(RB_BLEND_SRC_COLOR),
			NAME(RB_BLEND_ONE_MINUS_SRC_COLOR),
			NAME(RB_BLEND_SRC_ALPHA),
			NAME(RB_BLEND_ONE_MINUS_SRC_ALPHA),
			NAME(RB_BLEND_DST_COLOR),
			NAME(RB_BLEND_ONE_MINUS_DST_COLOR),
			NAME(RB_BLEND_DST_ALPHA),
			NAME(RB_BLEND_ONE_MINUS_DST_ALPHA),
			NAME(RB_BLEND_CONSTANT_COLOR),
			NAME(RB_BLEND_ONE_MINUS_CONSTANT_COLOR),
			NAME(RB_BLEND_CONSTANT_ALPHA),
			NAME(RB_BLEND_ONE_MINUS_CONSTANT_ALPHA),
			NAME(RB_BLEND_SRC_ALPHA_SATURATE),
	};
	static const char *func[] = {
		"DST_PLUS_SRC", "SRC_MINUS_DST", "MIN_DST_SRC",
		"MAX_DST_SRC", "DST_MINUS_SRC", "DST_PLUS_SRC_BIAS",
	};
	uint32_t csrcblend, cfunc, cdstblend;
	uint32_t asrcblend, afunc, adstblend;

	csrcblend = (dword >> 0) & 0x1f;
	cfunc     = (dword >> 5) & 0x7;
	cdstblend = (dword >> 8) & 0x1f;

	asrcblend = (dword >> 16) & 0x1f;
	afunc     = (dword >> 21) & 0x7;
	adstblend = (dword >> 24) & 0x1f;

	printf("%s%s: %08x (color-src-blend=%s, color-func=%s, color-dst-blend=%s, "
			"alpha-src-blend=%s, alpha-func=%s, alpha-dst-blend=%s%s%s)\n",
			levels[level], name, dword,
			op[csrcblend], func[cfunc], op[cdstblend],
			op[asrcblend], func[afunc], op[adstblend],
			(dword & RB_BLENDCONTROL_BLEND_FORCE_ENABLE) ? ", force-enable" : "",
			(dword & RB_BLENDCONTROL_BLEND_FORCE) ? ", force" : "");
}

static void reg_clear_color(const char *name, uint32_t dword, int level)
{
	uint32_t a, b, g, r;
	a = (dword >> 24) & 0xff;
	b = (dword >> 16) & 0xff;
	g = (dword >>  8) & 0xff;
	r = (dword >>  0) & 0xff;
	printf("%s%s: %f %f %f %f\n", levels[level], name,
			((float)r) / 255.0, ((float)g) / 255.0,
			((float)b) / 255.0, ((float)a) / 255.0);
}

static void reg_rb_color_info(const char *name, uint32_t dword, int level)
{
	/* RB_COLOR_INFO Endian=none, Linear, Format=RGBA8888, Swap=0, Base=gmem_base
	 * (format << 0) | tmp_ctx.gmem_base;
	 * gmem base assumed 4K aligned.
	 * BUG_ON(tmp_ctx.gmem_base & 0xFFF);
	 */
	printf("%s%s: %08x (%s)\n", levels[level], name, dword,
			color_name[dword & 0xf]);
}

static void reg_rb_bc_control(const char *name, uint32_t dword, int level)
{
	printf("%s%s: %08x (%stimeout-select=%d%s%s%s%s, az-throttle-count=%d%s%s%s%s"
			", accum-alloc-mask=%x%s, accum-data-fifo-limit=%d, "
			"mem-export-timeout-select=%d%s%s%s)\n", levels[level], name, dword,
			(dword & RB_BC_CONTROL_ACCUM_LINEAR_MODE_ENABLE) ? "accum-linear-mode, " : "",
			((dword >> 1) & 0x3),
			(dword & RB_BC_CONTROL_DISABLE_EZ_FAST_CONTEXT_SWITCH) ? ", disable-ez-fast-ctx-switch" : "",
			(dword & RB_BC_CONTROL_DISABLE_EZ_NULL_ZCMD_DROP) ? ", disable-ez-null-zcmd-drop" : "",
			(dword & RB_BC_CONTROL_DISABLE_LZ_NULL_ZCMD_DROP) ? ", disable-lz-null-zcmd-drop" : "",
			(dword & RB_BC_CONTROL_ENABLE_AZ_THROTTLE) ? ", enable-az-throttle" : "",
			((dword >> 8) & 0x1f),
			(dword & RB_BC_CONTROL_ENABLE_CRC_UPDATE) ? ", enable-crc-update" : "",
			(dword & RB_BC_CONTROL_CRC_MODE) ? ", crc-update" : "",
			(dword & RB_BC_CONTROL_DISABLE_SAMPLE_COUNTERS) ? ", disable-sample-counters" : "",
			(dword & RB_BC_CONTROL_DISABLE_ACCUM) ? ", disable-accum" : "",
			((dword >> 18) & 0xf),
			(dword & RB_BC_CONTROL_LINEAR_PERFORMANCE_ENABLE) ? ", linear-performance" : "",
			((dword >> 23) & 0xf),
			((dword >> 27) & 0x3),
			(dword & RB_BC_CONTROL_MEM_EXPORT_LINEAR_MODE_ENABLE) ? ", mem-export-linear-mode" : "",
			(dword & RB_BC_CONTROL_CRC_SYSTEM) ? ", crc-system" : "",
			(dword & RB_BC_CONTROL_RESERVED6) ? ", reserved6" : "");
}

/* sign-extend a 2s-compliment signed number of nbits size */
static int32_t u2i(uint32_t val, int nbits)
{
	return ((val >> (nbits-1)) * ~((1 << nbits) - 1)) | val;
}

static void reg_pa_sc_window_offset(const char *name, uint32_t dword, int level)
{
	/* x and y are 15 bit signed numbers: */
	uint32_t x = (dword >>  0) & 0x7fff;
	uint32_t y = (dword >> 16) & 0x7fff;
	printf("%s%s: %d,%d (%08x)\n", levels[level], name, u2i(x, 15), u2i(y, 15), dword);
}

static void reg_rb_copy_dest_offset(const char *name, uint32_t dword, int level)
{
	uint32_t x = (dword >>  0) & 0x3fff;
	uint32_t y = (dword >> 13) & 0x3fff;
	printf("%s%s: %d,%d (%08x)\n", levels[level], name, x, y, dword);
}

static void reg_vgt_current_bin_id_min_max(const char *name, uint32_t dword, int level)
{
	uint32_t col = (dword >> 0) & 0x7;
	uint32_t row = (dword >> 3) & 0x7;
	uint32_t gb  = (dword >> 6) & 0x7;
	printf("%s%s: %08x (col=%d, row=%d, guard-band=%d)\n", levels[level],
			name, dword, col, row, gb);
}

static void reg_xy(const char *name, uint32_t dword, int level)
{
	/* note: window_offset is 14 bits, scissors are 13 bits (at least in
	 * r600) but unused bits are set to zero:
	 */
	uint32_t x = (dword >>  0) & 0x3fff;
	uint32_t y = (dword >> 16) & 0x3fff;
	/* bit 31 is WINDOW_OFFSET_DISABLE (at least for TL's): */
	printf("%s%s: %d,%d%s (%08x)\n", levels[level], name, x, y,
			(dword & 0x80000000) ? " (WINDOW_OFFSET_DISABLE)" : "", dword);
}

static void reg_bin_size(const char *name, uint32_t dword, int level)
{
	uint32_t x = ((dword >> 0) & 0x1f) * 32;
	uint32_t y = ((dword >> 5) & 0x1f) * 32;
	printf("%s%s: %dx%d (%08x)\n", levels[level], name, x, y, dword);
}

static uint32_t type0_reg_vals[0x7fff];

static const const struct {
	const char *name;
	void (*fxn)(const char *name, uint32_t dword, int level);
} reg_a2xx[0x7fff] = {
#define REG(x, fxn) [REG_ ## x] = { #x, fxn }
		REG(CP_CSQ_IB1_STAT, reg_hex),
		REG(CP_CSQ_IB2_STAT, reg_hex),
		REG(CP_CSQ_RB_STAT, reg_hex),
		REG(CP_DEBUG, reg_hex),
		REG(CP_IB1_BASE, reg_hex),
		REG(CP_IB1_BUFSZ, reg_hex),
		REG(CP_IB2_BASE, reg_hex),
		REG(CP_IB2_BUFSZ, reg_hex),
		REG(CP_INT_ACK, reg_hex),
		REG(CP_INT_CNTL, reg_hex),
		REG(CP_INT_STATUS, reg_hex),
		REG(CP_ME_CNTL, reg_hex),
		REG(CP_ME_RAM_DATA, reg_hex),
		REG(CP_ME_RAM_WADDR, reg_hex),
		REG(CP_ME_RAM_RADDR, reg_hex),
		REG(CP_ME_STATUS, reg_hex),
		REG(CP_PFP_UCODE_ADDR, reg_hex),
		REG(CP_PFP_UCODE_DATA, reg_hex),
		REG(CP_QUEUE_THRESHOLDS, reg_hex),
		REG(CP_RB_BASE, reg_hex),
		REG(CP_RB_CNTL, reg_hex),
		REG(CP_RB_RPTR, reg_hex),
		REG(CP_RB_RPTR_ADDR, reg_hex),
		REG(CP_RB_RPTR_WR, reg_hex),
		REG(CP_RB_WPTR, reg_hex),
		REG(CP_RB_WPTR_BASE, reg_hex),
		REG(CP_RB_WPTR_DELAY, reg_hex),
		REG(CP_STAT, reg_hex),
		REG(CP_STATE_DEBUG_DATA, reg_hex),
		REG(CP_STATE_DEBUG_INDEX, reg_hex),
		REG(CP_ST_BASE, reg_hex),
		REG(CP_ST_BUFSZ, reg_hex),

		REG(CP_PERFMON_CNTL, reg_hex),
		REG(CP_PERFCOUNTER_SELECT, reg_hex),
		REG(CP_PERFCOUNTER_LO, reg_hex),
		REG(CP_PERFCOUNTER_HI, reg_hex),

		REG(RBBM_PERFCOUNTER1_SELECT, reg_hex),
		REG(RBBM_PERFCOUNTER1_HI, reg_hex),
		REG(RBBM_PERFCOUNTER1_LO, reg_hex),

		REG(MASTER_INT_SIGNAL, reg_hex),

		REG(PA_CL_VPORT_XSCALE, reg_float),		/* half_width */
		REG(PA_CL_VPORT_XOFFSET, reg_float),	/* x + half_width */
		REG(PA_CL_VPORT_YSCALE, reg_float),		/* -half_height */
		REG(PA_CL_VPORT_YOFFSET, reg_float),	/* y + half_height */
		REG(PA_CL_VPORT_ZSCALE, reg_float),
		REG(PA_CL_VPORT_ZOFFSET, reg_float),
		REG(PA_CL_CLIP_CNTL, reg_hex),
		REG(PA_CL_VTE_CNTL, reg_hex),
		REG(PA_CL_GB_VERT_CLIP_ADJ, reg_float),
		REG(PA_CL_GB_VERT_DISC_ADJ, reg_float),
		REG(PA_CL_GB_HORZ_CLIP_ADJ, reg_float),
		REG(PA_CL_GB_HORZ_DISC_ADJ, reg_float),
		REG(PA_SC_AA_MASK, reg_hex),
		REG(PA_SC_LINE_CNTL, reg_hex),
		REG(PA_SC_SCREEN_SCISSOR_BR, reg_xy),
		REG(PA_SC_SCREEN_SCISSOR_TL, reg_xy),
		REG(PA_SC_VIZ_QUERY, reg_hex),
		REG(PA_SC_VIZ_QUERY_STATUS, reg_hex),
		REG(PA_SC_WINDOW_OFFSET, reg_pa_sc_window_offset),
		REG(PA_SC_WINDOW_SCISSOR_BR, reg_xy),
		REG(PA_SC_WINDOW_SCISSOR_TL, reg_xy),
		REG(PA_SU_FACE_DATA, reg_hex),
		REG(PA_SU_POINT_SIZE, reg_hex),
		REG(PA_SU_LINE_CNTL, reg_hex),
		REG(PA_SU_POLY_OFFSET_BACK_OFFSET, reg_hex),
		REG(PA_SU_POLY_OFFSET_FRONT_SCALE, reg_hex),
		REG(PA_SU_SC_MODE_CNTL, reg_pa_su_sc_mode_cntl),
		REG(PA_SU_VTX_CNTL, reg_hex),

		REG(PC_INDEX_OFFSET, reg_hex),

		REG(RBBM_CNTL, reg_hex),
		REG(RBBM_INT_ACK, reg_hex),
		REG(RBBM_INT_CNTL, reg_hex),
		REG(RBBM_INT_STATUS, reg_hex),
		REG(RBBM_PATCH_RELEASE, reg_hex),
		REG(RBBM_PERIPHID1, reg_hex),
		REG(RBBM_PERIPHID2, reg_hex),
		REG(RBBM_DEBUG, reg_hex),
		REG(RBBM_DEBUG_OUT, reg_hex),
		REG(RBBM_DEBUG_CNTL, reg_hex),
		REG(RBBM_PM_OVERRIDE1, reg_hex),
		REG(RBBM_PM_OVERRIDE2, reg_hex),
		REG(RBBM_READ_ERROR, reg_hex),
		REG(RBBM_SOFT_RESET, reg_hex),
		REG(RBBM_STATUS, reg_hex),

		REG(RB_COLORCONTROL, reg_rb_colorcontrol),
		REG(RB_COLOR_DEST_MASK, reg_hex),
		REG(RB_COLOR_MASK, reg_hex),

		REG(RB_COPY_CONTROL, reg_hex),
		REG(RB_COPY_DEST_BASE, reg_gpuaddr),
		REG(RB_COPY_DEST_PITCH, reg_rb_copy_dest_pitch),
		REG(RB_COPY_DEST_INFO, reg_rb_copy_dest_info),
		REG(RB_COPY_DEST_OFFSET, reg_rb_copy_dest_offset),
		REG(RB_DEPTHCONTROL, reg_rb_depthcontrol),
		REG(RB_EDRAM_INFO, reg_hex),
		REG(RB_MODECONTROL, reg_hex),
		REG(RB_SURFACE_INFO, reg_hex),	/* surface pitch */
		REG(RB_COLOR_INFO, reg_rb_color_info),
		REG(RB_SAMPLE_POS, reg_hex),
		REG(RB_BLEND_RED, reg_hex),
		REG(RB_BLEND_BLUE, reg_hex),
		REG(RB_BLEND_GREEN, reg_hex),
		REG(RB_BLEND_ALPHA, reg_hex),
		REG(RB_ALPHA_REF, reg_hex),
		REG(RB_BLEND_CONTROL, reg_rb_blend_control),
		REG(CLEAR_COLOR, reg_clear_color),
		REG(RB_BC_CONTROL, reg_rb_bc_control),

		REG(SCRATCH_ADDR, reg_hex),
		REG(SCRATCH_REG0, reg_hex),
		REG(SCRATCH_REG2, reg_hex),
		REG(SCRATCH_UMSK, reg_hex),

		REG(SQ_CF_BOOLEANS, reg_hex),
		REG(SQ_CF_LOOP, reg_hex),
		REG(SQ_GPR_MANAGEMENT, reg_hex),
		REG(SQ_FLOW_CONTROL, reg_hex),
		REG(SQ_INST_STORE_MANAGMENT, reg_hex),
		REG(SQ_INT_ACK, reg_hex),
		REG(SQ_INT_CNTL, reg_hex),
		REG(SQ_INT_STATUS, reg_hex),
		REG(SQ_PROGRAM_CNTL, reg_sq_program_cntl),
		REG(SQ_CONTEXT_MISC, reg_hex),
		REG(SQ_PS_PROGRAM, reg_hex),
		REG(SQ_VS_PROGRAM, reg_hex),
		REG(SQ_WRAPPING_0, reg_hex),
		REG(SQ_WRAPPING_1, reg_hex),

		REG(VGT_ENHANCE, reg_hex),
		REG(VGT_INDX_OFFSET, reg_hex),
		REG(VGT_MAX_VTX_INDX, reg_hex),
		REG(VGT_MIN_VTX_INDX, reg_hex),
		REG(VGT_OUT_DEALLOC_CNTL, reg_hex),
		REG(VGT_CURRENT_BIN_ID_MAX, reg_vgt_current_bin_id_min_max),
		REG(VGT_CURRENT_BIN_ID_MIN, reg_vgt_current_bin_id_min_max),

		REG(TP0_CHICKEN, reg_hex),
		REG(TC_CNTL_STATUS, reg_hex),
		REG(PA_SC_AA_CONFIG, reg_hex),
		REG(VGT_VERTEX_REUSE_BLOCK_CNTL, reg_hex),
		REG(SQ_INTERPOLATOR_CNTL, reg_hex),
		REG(RB_DEPTH_INFO, reg_rb_depth_info),
		REG(COHER_DEST_BASE_0, reg_hex),
		REG(RB_FOG_COLOR, reg_hex),
		REG(RB_STENCILREFMASK_BF, reg_hex),
		REG(RB_STENCILREFMASK, reg_hex),
		REG(PA_SC_LINE_STIPPLE, reg_hex),
		REG(SQ_PS_CONST, reg_hex),
		REG(RB_DEPTH_CLEAR, reg_hex),
		REG(RB_SAMPLE_COUNT_CTL, reg_hex),
		REG(SQ_CONSTANT_0, reg_hex),
		REG(SQ_FETCH_0, reg_hex),

		REG(PA_SU_POINT_MINMAX, reg_hex),
		REG(SQ_VS_CONST, reg_hex),

		REG(COHER_BASE_PM4, reg_hex),
		REG(COHER_STATUS_PM4, reg_hex),
		REG(COHER_SIZE_PM4, reg_hex),

		/*registers added in adreno220*/
		REG(A220_PC_INDX_OFFSET, reg_hex),
		REG(A220_PC_VERTEX_REUSE_BLOCK_CNTL, reg_hex),
		REG(A220_PC_MAX_VTX_INDX, reg_hex),
		REG(A220_RB_LRZ_VSC_CONTROL, reg_hex),
		REG(A220_GRAS_CONTROL, reg_hex),
		REG(A220_VSC_BIN_SIZE, reg_bin_size),
		REG(A220_VSC_PIPE_DATA_LENGTH_7, reg_hex),

		/*registers added in adreno225*/
		REG(A225_RB_COLOR_INFO3, reg_hex),
		REG(A225_PC_MULTI_PRIM_IB_RESET_INDX, reg_hex),
		REG(A225_GRAS_UCP0X, reg_hex),
		REG(A225_GRAS_UCP5W, reg_hex),
		REG(A225_GRAS_UCP_ENABLED, reg_hex),

		/* Debug registers used by snapshot */
		REG(PA_SU_DEBUG_CNTL, reg_hex),
		REG(PA_SU_DEBUG_DATA, reg_hex),
		REG(RB_DEBUG_CNTL, reg_hex),
		REG(RB_DEBUG_DATA, reg_hex),
		REG(PC_DEBUG_CNTL, reg_hex),
		REG(PC_DEBUG_DATA, reg_hex),
		REG(GRAS_DEBUG_CNTL, reg_hex),
		REG(GRAS_DEBUG_DATA, reg_hex),
		REG(SQ_DEBUG_MISC, reg_hex),
		REG(SQ_DEBUG_INPUT_FSM, reg_hex),
		REG(SQ_DEBUG_CONST_MGR_FSM, reg_hex),
		REG(SQ_DEBUG_EXP_ALLOC, reg_hex),
		REG(SQ_DEBUG_FSM_ALU_0, reg_hex),
		REG(SQ_DEBUG_FSM_ALU_1, reg_hex),
		REG(SQ_DEBUG_PTR_BUFF, reg_hex),
		REG(SQ_DEBUG_GPR_VTX, reg_hex),
		REG(SQ_DEBUG_GPR_PIX, reg_hex),
		REG(SQ_DEBUG_TB_STATUS_SEL, reg_hex),
		REG(SQ_DEBUG_VTX_TB_0, reg_hex),
		REG(SQ_DEBUG_VTX_TB_1, reg_hex),
		REG(SQ_DEBUG_VTX_TB_STATE_MEM, reg_hex),
		REG(SQ_DEBUG_TP_FSM, reg_hex),
		REG(SQ_DEBUG_VTX_TB_STATUS_REG, reg_hex),
		REG(SQ_DEBUG_PIX_TB_0, reg_hex),
		REG(SQ_DEBUG_PIX_TB_STATUS_REG_0, reg_hex),
		REG(SQ_DEBUG_PIX_TB_STATUS_REG_1, reg_hex),
		REG(SQ_DEBUG_PIX_TB_STATUS_REG_2, reg_hex),
		REG(SQ_DEBUG_PIX_TB_STATUS_REG_3, reg_hex),
		REG(SQ_DEBUG_PIX_TB_STATE_MEM, reg_hex),
		REG(SQ_DEBUG_MISC_0, reg_hex),
		REG(SQ_DEBUG_MISC_1, reg_hex),
#undef REG
}, reg_a3xx[0x7fff] = {
#define REG(x, fxn) [A3XX_ ## x] = { #x, fxn }
		REG(RBBM_HW_VERSION, reg_hex),
		REG(RBBM_HW_RELEASE, reg_hex),
		REG(RBBM_HW_CONFIGURATION, reg_hex),
		REG(RBBM_SW_RESET_CMD, reg_hex),
		REG(RBBM_AHB_CTL0, reg_hex),
		REG(RBBM_AHB_CTL1, reg_hex),
		REG(RBBM_AHB_CMD, reg_hex),
		REG(RBBM_AHB_ERROR_STATUS, reg_hex),
		REG(RBBM_GPR0_CTL, reg_hex),
/* This the same register as on A2XX, just in a different place */
		REG(RBBM_STATUS, reg_hex),
		REG(RBBM_INTERFACE_HANG_INT_CTL, reg_hex),
		REG(RBBM_INTERFACE_HANG_MASK_CTL0, reg_hex),
		REG(RBBM_INTERFACE_HANG_MASK_CTL1, reg_hex),
		REG(RBBM_INTERFACE_HANG_MASK_CTL2, reg_hex),
		REG(RBBM_INTERFACE_HANG_MASK_CTL3, reg_hex),
		REG(RBBM_INT_CLEAR_CMD, reg_hex),
		REG(RBBM_INT_0_MASK, reg_hex),
		REG(RBBM_INT_0_STATUS, reg_hex),
		REG(RBBM_GPU_BUSY_MASKED, reg_hex),
		REG(RBBM_RBBM_CTL, reg_hex),
		REG(RBBM_RBBM_CTL, reg_hex),
		REG(RBBM_PERFCTR_PWR_1_LO, reg_hex),
		REG(RBBM_PERFCTR_PWR_1_HI, reg_hex),
		REG(RBBM_DEBUG_BUS_CTL, reg_hex),
		REG(RBBM_DEBUG_BUS_DATA_STATUS, reg_hex),
/* Following two are same as on A2XX, just in a different place */
		REG(CP_PFP_UCODE_ADDR, reg_hex),
		REG(CP_PFP_UCODE_DATA, reg_hex),
		REG(CP_ROQ_ADDR, reg_hex),
		REG(CP_ROQ_DATA, reg_hex),
		REG(CP_MEQ_ADDR, reg_hex),
		REG(CP_MEQ_DATA, reg_hex),
		REG(CP_HW_FAULT , reg_hex),
		REG(CP_AHB_FAULT, reg_hex),
		REG(CP_PROTECT_CTRL, reg_hex),
		REG(CP_PROTECT_STATUS, reg_hex),
		REG(CP_PROTECT_REG_0, reg_hex),
		REG(CP_PROTECT_REG_1, reg_hex),
		REG(CP_PROTECT_REG_2, reg_hex),
		REG(CP_PROTECT_REG_3, reg_hex),
		REG(CP_PROTECT_REG_4, reg_hex),
		REG(CP_PROTECT_REG_5, reg_hex),
		REG(CP_PROTECT_REG_6, reg_hex),
		REG(CP_PROTECT_REG_7, reg_hex),
		REG(CP_PROTECT_REG_8, reg_hex),
		REG(CP_PROTECT_REG_9, reg_hex),
		REG(CP_PROTECT_REG_A, reg_hex),
		REG(CP_PROTECT_REG_B, reg_hex),
		REG(CP_PROTECT_REG_C, reg_hex),
		REG(CP_PROTECT_REG_D, reg_hex),
		REG(CP_PROTECT_REG_E, reg_hex),
		REG(CP_PROTECT_REG_F, reg_hex),
		REG(CP_SCRATCH_REG2, reg_hex),
		REG(CP_SCRATCH_REG3, reg_hex),
		REG(VSC_BIN_SIZE, reg_hex),
		REG(VSC_SIZE_ADDRESS, reg_hex),
		REG(VSC_PIPE_CONFIG_0, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_0, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_0, reg_hex),
		REG(VSC_PIPE_CONFIG_1, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_1, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_1, reg_hex),
		REG(VSC_PIPE_CONFIG_2, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_2, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_2, reg_hex),
		REG(VSC_PIPE_CONFIG_3, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_3, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_3, reg_hex),
		REG(VSC_PIPE_CONFIG_4, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_4, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_4, reg_hex),
		REG(VSC_PIPE_CONFIG_5, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_5, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_5, reg_hex),
		REG(VSC_PIPE_CONFIG_6, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_6, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_6, reg_hex),
		REG(VSC_PIPE_CONFIG_7, reg_hex),
		REG(VSC_PIPE_DATA_ADDRESS_7, reg_hex),
		REG(VSC_PIPE_DATA_LENGTH_7, reg_hex),
		REG(GRAS_CL_USER_PLANE_X0, reg_hex),
		REG(GRAS_CL_USER_PLANE_Y0, reg_hex),
		REG(GRAS_CL_USER_PLANE_Z0, reg_hex),
		REG(GRAS_CL_USER_PLANE_W0, reg_hex),
		REG(GRAS_CL_USER_PLANE_X1, reg_hex),
		REG(GRAS_CL_USER_PLANE_Y1, reg_hex),
		REG(GRAS_CL_USER_PLANE_Z1, reg_hex),
		REG(GRAS_CL_USER_PLANE_W1, reg_hex),
		REG(GRAS_CL_USER_PLANE_X2, reg_hex),
		REG(GRAS_CL_USER_PLANE_Y2, reg_hex),
		REG(GRAS_CL_USER_PLANE_Z2, reg_hex),
		REG(GRAS_CL_USER_PLANE_W2, reg_hex),
		REG(GRAS_CL_USER_PLANE_X3, reg_hex),
		REG(GRAS_CL_USER_PLANE_Y3, reg_hex),
		REG(GRAS_CL_USER_PLANE_Z3, reg_hex),
		REG(GRAS_CL_USER_PLANE_W3, reg_hex),
		REG(GRAS_CL_USER_PLANE_X4, reg_hex),
		REG(GRAS_CL_USER_PLANE_Y4, reg_hex),
		REG(GRAS_CL_USER_PLANE_Z4, reg_hex),
		REG(GRAS_CL_USER_PLANE_W4, reg_hex),
		REG(GRAS_CL_USER_PLANE_X5, reg_hex),
		REG(GRAS_CL_USER_PLANE_Y5, reg_hex),
		REG(GRAS_CL_USER_PLANE_Z5, reg_hex),
		REG(GRAS_CL_USER_PLANE_W5, reg_hex),
		REG(VPC_VPC_DEBUG_RAM_SEL, reg_hex),
		REG(VPC_VPC_DEBUG_RAM_READ, reg_hex),
		REG(UCHE_CACHE_INVALIDATE0_REG, reg_hex),
		REG(GRAS_CL_CLIP_CNTL, reg_hex),
		REG(GRAS_CL_GB_CLIP_ADJ, reg_hex),
		REG(GRAS_CL_VPORT_XOFFSET, reg_float),
		REG(GRAS_CL_VPORT_XSCALE, reg_float),
		REG(GRAS_CL_VPORT_YOFFSET, reg_float),
		REG(GRAS_CL_VPORT_YSCALE, reg_float),
		REG(GRAS_CL_VPORT_ZOFFSET, reg_float),
		REG(GRAS_CL_VPORT_ZSCALE, reg_float),
		REG(GRAS_SU_POINT_MINMAX, reg_hex),
		REG(GRAS_SU_POINT_SIZE, reg_hex),
		REG(GRAS_SU_POLY_OFFSET_SCALE, reg_hex),
		REG(GRAS_SU_POLY_OFFSET_OFFSET, reg_hex),
		REG(GRAS_SU_MODE_CONTROL, reg_hex),
		REG(GRAS_SC_CONTROL, reg_hex),
		REG(GRAS_SC_SCREEN_SCISSOR_TL, reg_xy),
		REG(GRAS_SC_SCREEN_SCISSOR_BR, reg_xy),
		REG(GRAS_SC_WINDOW_SCISSOR_TL, reg_xy),
		REG(GRAS_SC_WINDOW_SCISSOR_BR, reg_xy),
		REG(RB_MODE_CONTROL, reg_hex),
		REG(RB_RENDER_CONTROL, reg_hex),
		REG(RB_MSAA_CONTROL, reg_hex),
		REG(RB_MRT_CONTROL0, reg_hex),
		REG(RB_MRT_BUF_INFO0, reg_hex),
		REG(RB_MRT_BLEND_CONTROL0, reg_hex),
		REG(RB_MRT_BLEND_CONTROL1, reg_hex),
		REG(RB_MRT_BLEND_CONTROL2, reg_hex),
		REG(RB_MRT_BLEND_CONTROL3, reg_hex),
		REG(RB_BLEND_RED, reg_hex),
		REG(RB_COPY_CONTROL, reg_hex),
		REG(RB_COPY_DEST_INFO, reg_hex),
		REG(RB_DEPTH_CONTROL, reg_hex),
		REG(RB_STENCIL_CONTROL, reg_hex),
		REG(PC_VSTREAM_CONTROL, reg_hex),
		REG(PC_VERTEX_REUSE_BLOCK_CNTL, reg_hex),
		REG(PC_PRIM_VTX_CNTL, reg_hex),
		REG(PC_RESTART_INDEX, reg_hex),
		REG(HLSQ_CONTROL_0_REG, reg_hex),
		REG(HLSQ_VS_CONTROL_REG, reg_hex),
		REG(HLSQ_CONST_FSPRESV_RANGE_REG, reg_hex),
		REG(HLSQ_CL_NDRANGE_0_REG, reg_hex),
		REG(HLSQ_CL_NDRANGE_2_REG, reg_hex),
		REG(HLSQ_CL_CONTROL_0_REG, reg_hex),
		REG(HLSQ_CL_CONTROL_1_REG, reg_hex),
		REG(HLSQ_CL_KERNEL_CONST_REG, reg_hex),
		REG(HLSQ_CL_KERNEL_GROUP_X_REG, reg_hex),
		REG(HLSQ_CL_KERNEL_GROUP_Z_REG, reg_hex),
		REG(HLSQ_CL_WG_OFFSET_REG, reg_hex),
		REG(VFD_CONTROL_0, reg_hex),
		REG(VFD_INDEX_MIN, reg_hex),
		REG(VFD_FETCH_INSTR_0_0, reg_hex),
		REG(VFD_FETCH_INSTR_0_4, reg_hex),
		REG(VFD_DECODE_INSTR_0, reg_hex),
		REG(VFD_VS_THREADING_THRESHOLD, reg_hex),
		REG(VPC_ATTR, reg_hex),
		REG(VPC_VARY_CYLWRAP_ENABLE_1, reg_hex),
		REG(SP_SP_CTRL_REG, reg_hex),
		REG(SP_VS_CTRL_REG0, reg_hex),
		REG(SP_VS_CTRL_REG1, reg_hex),
		REG(SP_VS_PARAM_REG, reg_hex),
		REG(SP_VS_OUT_REG_7, reg_hex),
		REG(SP_VS_VPC_DST_REG_0, reg_hex),
		REG(SP_VS_OBJ_OFFSET_REG, reg_hex),
		REG(SP_VS_PVT_MEM_SIZE_REG, reg_hex),
		REG(SP_VS_LENGTH_REG, reg_hex),
		REG(SP_FS_CTRL_REG0, reg_hex),
		REG(SP_FS_CTRL_REG1, reg_hex),
		REG(SP_FS_OBJ_OFFSET_REG, reg_hex),
		REG(SP_FS_PVT_MEM_SIZE_REG, reg_hex),
		REG(SP_FS_FLAT_SHAD_MODE_REG_0, reg_hex),
		REG(SP_FS_FLAT_SHAD_MODE_REG_1, reg_hex),
		REG(SP_FS_OUTPUT_REG, reg_hex),
		REG(SP_FS_MRT_REG_0, reg_hex),
		REG(SP_FS_IMAGE_OUTPUT_REG_0, reg_hex),
		REG(SP_FS_IMAGE_OUTPUT_REG_3, reg_hex),
		REG(SP_FS_LENGTH_REG, reg_hex),
		REG(TPL1_TP_VS_TEX_OFFSET, reg_hex),
		REG(TPL1_TP_FS_TEX_OFFSET, reg_hex),
		REG(TPL1_TP_FS_BORDER_COLOR_BASE_ADDR, reg_hex),
		REG(VBIF_FIXED_SORT_EN, reg_hex),
		REG(VBIF_FIXED_SORT_SEL0, reg_hex),
		REG(VBIF_FIXED_SORT_SEL1, reg_hex),
#undef REG
}, *type0_reg = reg_a2xx; /* default to a2xx so we can still parse older rd files prior to RD_GPU_ID */

static void dump_registers(uint32_t regbase,
		uint32_t *dwords, uint32_t sizedwords, int level)
{
	while (sizedwords--) {
		type0_reg_vals[regbase] = *dwords;
		if (type0_reg[regbase].fxn) {
			type0_reg[regbase].fxn(type0_reg[regbase].name, *dwords, level);
		} else {
			printf("%s<%04x>: %08x\n", levels[level], regbase, *dwords);
		}
		regbase++;
		dwords++;
	}
}

static void cp_im_loadi(uint32_t *dwords, uint32_t sizedwords, int level)
{
	uint32_t start = dwords[1] >> 16;
	uint32_t size  = dwords[1] & 0xffff;
	const char *type = NULL, *ext = NULL;
	enum shader_t disasm_type;

	switch (dwords[0]) {
	case 0:
		type = "vertex";
		ext = "vo";
		disasm_type = SHADER_VERTEX;
		break;
	case 1:
		type = "fragment";
		ext = "fo";
		disasm_type = SHADER_FRAGMENT;
		break;
	default:
		type = "<unknown>"; break;
	}

	printf("%s%s shader, start=%04x, size=%04x\n", levels[level], type, start, size);
	disasm_a2xx(dwords + 2, sizedwords - 2, level+1, disasm_type);

	/* dump raw shader: */
	if (ext && dump_shaders) {
		static int n = 0;
		char filename[8];
		int fd;
		sprintf(filename, "%04d.%s", n++, ext);
		fd = open(filename, O_WRONLY| O_TRUNC | O_CREAT, 0644);
		write(fd, dwords + 2, (sizedwords - 2) * 4);
	}
}

static void cp_load_state(uint32_t *dwords, uint32_t sizedwords, int level)
{
	uint32_t dst_off = dwords[0] & 0xffff;
	uint32_t state_src = (dwords[0] >> 16) & 0x7;
	uint32_t state_block_id = (dwords[0] >> 19) & 0x7;
	uint32_t num_unit = (dwords[0] >> 22) & 0x1ff;
	uint32_t state_type = dwords[1] & 0x3;
	uint32_t ext_src_addr = (dwords[1] >> 2) & 0x3fffffff;
	const char *type = NULL, *ext = NULL;
	enum shader_t disasm_type;

	switch (state_block_id) {
	case HLSQ_BLOCK_ID_SP_VS:
		type = "vertex";
		ext = "vo3";
		disasm_type = SHADER_VERTEX;
		break;
	case HLSQ_BLOCK_ID_SP_FS:
		type = "fragment";
		ext = "fo";
		disasm_type = SHADER_FRAGMENT;
		break;
	}

	printf("%s%s shader, dst_off=%04x, state_src=%04x, state_block_id=%d, "
			"num_unit=%d, state_type=%d, ext_src_addr=%08x, size=%04x\n",
			levels[level], type, dst_off, state_src, state_block_id,
			num_unit, state_type, ext_src_addr, sizedwords-2);
	disasm_a3xx(dwords + 2, sizedwords - 2, level+1, disasm_type);

	/* dump raw shader: */
	if (ext && dump_shaders) {
		static int n = 0;
		char filename[8];
		int fd;
		sprintf(filename, "%04d.%s", n++, ext);
		fd = open(filename, O_WRONLY| O_TRUNC | O_CREAT, 0644);
		write(fd, dwords + 2, (sizedwords - 2) * 4);
	}
}

static void dump_tex_const(uint32_t *dwords, uint32_t sizedwords, uint32_t val, int level)
{
	uint32_t w, h, p;
	uint32_t gpuaddr, flags, mip_gpuaddr, mip_flags;
	uint32_t min, mag, swiz, clamp_x, clamp_y, clamp_z;
	static const char *filter[] = {
			"point", "bilinear", "bicubic",
	};
	static const char *clamp[] = {
			"wrap", "mirror", "clamp-last-texel",
	};
	static const char swiznames[] = "xyzw01??";

	/* see sys2gmem_tex_const[] in adreno_a2xxx.c */

	/* Texture, FormatXYZW=Unsigned, ClampXYZ=Wrap/Repeat,
	 * RFMode=ZeroClamp-1, Dim=1:2d, pitch
	 */
	p = (dwords[0] >> 22) << 5;
	clamp_x = (dwords[0] >> 10) & 0x3;
	clamp_y = (dwords[0] >> 13) & 0x3;
	clamp_z = (dwords[0] >> 16) & 0x3;

	/* Format=6:8888_WZYX, EndianSwap=0:None, ReqSize=0:256bit, DimHi=0,
	 * NearestClamp=1:OGL Mode
	 */
	parse_dword_addr(dwords[1], &gpuaddr, &flags, 0xfff);

	/* Width, Height, EndianSwap=0:None */
	w = (dwords[2] & 0x1fff) + 1;
	h = ((dwords[2] >> 13) & 0x1fff) + 1;

	/* NumFormat=0:RF, DstSelXYZW=XYZW, ExpAdj=0, MagFilt=MinFilt=0:Point,
	 * Mip=2:BaseMap
	 */
	mag = (dwords[3] >> 19) & 0x3;
	min = (dwords[3] >> 21) & 0x3;
	swiz = (dwords[3] >> 1) & 0xfff;

	/* VolMag=VolMin=0:Point, MinMipLvl=0, MaxMipLvl=1, LodBiasH=V=0,
	 * Dim3d=0
	 */
	// XXX

	/* BorderColor=0:ABGRBlack, ForceBC=0:diable, TriJuice=0, Aniso=0,
	 * Dim=1:2d, MipPacking=0
	 */
	parse_dword_addr(dwords[5], &mip_gpuaddr, &mip_flags, 0xfff);

	printf("%sset texture const %04x\n", levels[level], val);
	printf("%sclamp x/y/z: %s/%s/%s\n", levels[level+1],
			clamp[clamp_x], clamp[clamp_y], clamp[clamp_z]);
	printf("%sfilter min/mag: %s/%s\n", levels[level+1], filter[min], filter[mag]);
	printf("%sswizzle: %c%c%c%c\n", levels[level+1],
			swiznames[(swiz >> 0) & 0x7], swiznames[(swiz >> 3) & 0x7],
			swiznames[(swiz >> 6) & 0x7], swiznames[(swiz >> 9) & 0x7]);
	printf("%saddr=%08x (flags=%03x), size=%dx%d, pitch=%d, format=%s\n",
			levels[level+1], gpuaddr, flags, w, h, p,
			fmt_name[flags & 0xf]);
	printf("%smipaddr=%08x (flags=%03x)\n", levels[level+1],
			mip_gpuaddr, mip_flags);
}

static void dump_shader_const(uint32_t *dwords, uint32_t sizedwords, uint32_t val, int level)
{
	int i;
	printf("%sset shader const %04x\n", levels[level], val);
	for (i = 0; i < sizedwords; ) {
		uint32_t gpuaddr, flags;
		parse_dword_addr(dwords[i++], &gpuaddr, &flags, 0xf);
		void *addr = hostptr(gpuaddr);
		if (addr) {
			uint32_t size = dwords[i++];
			printf("%saddr=%08x, size=%d, format=%s\n", levels[level+1],
					gpuaddr, size, fmt_name[flags & 0xf]);
			// TODO maybe dump these as bytes instead of dwords?
			size = (size + 3) / 4; // for now convert to dwords
			dump_hex(addr, min(size, 64), level + 1);
			if (size > min(size, 64))
				printf("%s\t\t...\n", levels[level+1]);
			dump_float(addr, min(size, 64), level + 1);
			if (size > min(size, 64))
				printf("%s\t\t...\n", levels[level+1]);
		}
	}
}

static void cp_set_const(uint32_t *dwords, uint32_t sizedwords, int level)
{
	uint32_t val = dwords[0] & 0xffff;
	switch(dwords[0] >> 16) {
	case 0x0:
		dump_float((float *)(dwords+1), sizedwords-1, level+1);
		break;
	case 0x1:
		/* need to figure out how const space is partitioned between
		 * attributes, textures, etc..
		 */
		if (val < 0x78) {
			dump_tex_const(dwords+1, sizedwords-1, val, level);
		} else {
			dump_shader_const(dwords+1, sizedwords-1, val, level);
		}
		break;
	case 0x2:
		printf("%sset bool const %04x\n", levels[level], val);
		break;
	case 0x3:
		printf("%sset loop const %04x\n", levels[level], val);
		break;
	case 0x4:
		val += 0x2000;
		dump_registers(val, dwords+1, sizedwords-1, level+1);
		break;
	}
}

static void cp_event_write(uint32_t *dwords, uint32_t sizedwords, int level)
{
	printf("%sevent %s\n", levels[level], event_name[dwords[0]]);
}

static void cp_draw_indx(uint32_t *dwords, uint32_t sizedwords, int level)
{
	uint32_t i;
	uint32_t prim_type     = dwords[1] & 0x1f;
	uint32_t source_select = (dwords[1] >> 6) & 0x3;
	uint32_t num_indices   = dwords[2];

	printf("%sprim_type:     %s (%d)\n", levels[level],
			vgt_prim_types[prim_type], prim_type);
	printf("%ssource_select: %s (%d)\n", levels[level],
			vgt_source_select[source_select], source_select);
	printf("%snum_indices:   %d\n", levels[level], num_indices);

	if (sizedwords == 5) {
		void *ptr = hostptr(dwords[3]);
		printf("%sgpuaddr:       %08x\n", levels[level], dwords[3]);
		printf("%sidx_size:      %d\n", levels[level], dwords[4]);
		if (ptr) {
			enum pc_di_index_size size =
					((dwords[1] >> 11) & 1) | ((dwords[1] >> 12) & 2);
			int i;
			printf("%sidxs:         ", levels[level]);
			if (size == INDEX_SIZE_8_BIT) {
				uint8_t *idx = ptr;
				for (i = 0; i < dwords[4]; i++)
					printf(" %u", idx[i]);
			} else if (size == INDEX_SIZE_16_BIT) {
				uint16_t *idx = ptr;
				for (i = 0; i < dwords[4]/2; i++)
					printf(" %u", idx[i]);
			} else if (size == INDEX_SIZE_32_BIT) {
				uint32_t *idx = ptr;
				for (i = 0; i < dwords[4]/4; i++)
					printf(" %u", idx[i]);
			}
			printf("\n");
			dump_hex(ptr, dwords[4], level+1);
		}
	}

	/* dump current state of registers: */
	printf("%scurrent register values\n", levels[level]);
	for (i = 0; i < 0x7fff; i++) {
		uint32_t regbase = i;
		uint32_t lastval = type0_reg_vals[regbase];
		/* skip registers we don't know about or have zero: */
		if (!(type0_reg[regbase].name && lastval))
			continue;
		if (type0_reg[regbase].fxn) {
			type0_reg[regbase].fxn(type0_reg[regbase].name, lastval, level+2);
		} else {
			printf("%s<%04x>: %08x\n", levels[level+2], regbase, lastval);
		}
	}
}

static void cp_indirect(uint32_t *dwords, uint32_t sizedwords, int level)
{
	/* traverse indirect buffers */
	int i;
	uint32_t ibaddr = dwords[0];
	uint32_t ibsize = dwords[1];
	uint32_t *ptr = NULL;

	printf("%sibaddr:%08x\n", levels[level], ibaddr);
	printf("%sibsize:%08x\n", levels[level], ibsize);

	/* map gpuaddr back to hostptr: */
	for (i = 0; i < nbuffers; i++) {
		if (buffer_contains_gpuaddr(&buffers[i], ibaddr, ibsize)) {
			ptr = buffers[i].hostptr + (ibaddr - buffers[i].gpuaddr);
			break;
		}
	}

	if (ptr) {
		dump_commands(ptr, ibsize, level);
	} else {
		fprintf(stderr, "could not find: %08x (%d)\n", ibaddr, ibsize);
	}
}

static void cp_mem_write(uint32_t *dwords, uint32_t sizedwords, int level)
{
	uint32_t gpuaddr = dwords[0];
	printf("%sgpuaddr:%08x\n", levels[level], gpuaddr);
	dump_float((float *)&dwords[1], sizedwords-1, level+1);
}

#define CP(x, fxn)   [CP_ ## x] = { #x, fxn }
static const struct {
	const char *name;
	void (*fxn)(uint32_t *dwords, uint32_t sizedwords, int level);
} type3_op[0xff] = {
		CP(ME_INIT, NULL),
		CP(NOP, NULL),
		CP(INDIRECT_BUFFER, cp_indirect),
		CP(INDIRECT_BUFFER_PFD, cp_indirect),
		CP(WAIT_FOR_IDLE, NULL),
		CP(WAIT_REG_MEM, NULL),
		CP(WAIT_REG_EQ, NULL),
		CP(WAT_REG_GTE, NULL),
		CP(WAIT_UNTIL_READ, NULL),
		CP(WAIT_IB_PFD_COMPLETE, NULL),
		CP(REG_RMW, NULL),
		CP(REG_TO_MEM, NULL),
		CP(MEM_WRITE, cp_mem_write),
		CP(MEM_WRITE_CNTR, NULL),
		CP(COND_EXEC, NULL),
		CP(COND_WRITE, NULL),
		CP(EVENT_WRITE, cp_event_write),
		CP(EVENT_WRITE_SHD, NULL),
		CP(EVENT_WRITE_CFL, NULL),
		CP(EVENT_WRITE_ZPD, NULL),
		CP(DRAW_INDX, cp_draw_indx),
		CP(DRAW_INDX_2, NULL),
		CP(DRAW_INDX_BIN, NULL),
		CP(DRAW_INDX_2_BIN, NULL),
		CP(VIZ_QUERY, NULL),
		CP(SET_STATE, NULL),
		CP(SET_CONSTANT, cp_set_const),
		CP(IM_LOAD, NULL),
		CP(IM_LOAD_IMMEDIATE, cp_im_loadi),
		CP(LOAD_CONSTANT_CONTEXT, NULL),
		CP(INVALIDATE_STATE, NULL),
		CP(SET_SHADER_BASES, NULL),
		CP(SET_BIN_MASK, NULL),
		CP(SET_BIN_SELECT, NULL),
		CP(CONTEXT_UPDATE, NULL),
		CP(INTERRUPT, NULL),
		CP(IM_STORE, NULL),
		CP(SET_PROTECTED_MODE, NULL),

		/* for a20x */
		//CP(SET_BIN_BASE_OFFSET, NULL),

		/* for a22x */
		CP(SET_DRAW_INIT_FLAGS, NULL),

		/* for a3xx */
		CP(LOAD_STATE, cp_load_state),
		CP(SET_BIN_DATA, NULL),
};

static void dump_commands(uint32_t *dwords, uint32_t sizedwords, int level)
{
	int dwords_left = sizedwords;
	uint32_t count = 0; /* dword count including packet header */
	uint32_t val;

	while (dwords_left > 0) {
		int type = dwords[0] >> 30;
		printf("t%d", type);
		switch (type) {
		case 0x0: /* type-0 */
			count = (dwords[0] >> 16)+2;
			val = GET_PM4_TYPE0_REGIDX(dwords);
			printf("%swrite %s%s\n", levels[level+1], type0_reg[val].name,
					(dwords[0] & 0x8000) ? " (same register)" : "");
			dump_registers(val, dwords+1, count-1, level+2);
			dump_hex(dwords, count, level+1);
			break;
		case 0x1: /* type-1 */
			count = 3;
			val = dwords[0] & 0xfff;
			printf("%swrite %s\n", levels[level+1], type0_reg[val].name);
			dump_registers(val, dwords+1, 1, level+2);
			val = (dwords[0] >> 12) & 0xfff;
			printf("%swrite %s\n", levels[level+1], type0_reg[val].name);
			dump_registers(val, dwords+2, 1, level+2);
			dump_hex(dwords, count, level+1);
			break;
		case 0x3: /* type-3 */
			count = ((dwords[0] >> 16) & 0x3fff) + 2;
			val = GET_PM4_TYPE3_OPCODE(dwords);
			printf("\t%sopcode: %s (%02x) (%d dwords)%s\n", levels[level],
					type3_op[val].name, val, count,
					(dwords[0] & 0x1) ? " (predicated)" : "");
			if (type3_op[val].fxn)
				type3_op[val].fxn(dwords+1, count-1, level+1);
			dump_hex(dwords, count, level+1);
			break;
		default:
			fprintf(stderr, "bad type!\n");
			return;
		}

		dwords += count;
		dwords_left -= count;

		//printf("*** dwords_left=%d, count=%d\n", dwords_left, count);
	}

	if (dwords_left < 0)
		printf("**** this ain't right!! dwords_left=%d\n", dwords_left);
}

int main(int argc, char **argv)
{
	enum rd_sect_type type = RD_NONE;
	void *buf = NULL;
	int fd, sz, i, n = 1;

	if (!strcmp(argv[n], "--verbose")) {
		disasm_set_debug(PRINT_RAW);
		n++;
	}

	if (!strcmp(argv[n], "--dump-shaders")) {
		dump_shaders = true;
		n++;
	}

	if (argc-n != 1)
		fprintf(stderr, "usage: %s [--dump-shaders] testlog.rd\n", argv[0]);

	fd = open(argv[n], O_RDONLY);
	if (fd < 0)
		fprintf(stderr, "could not open: %s\n", argv[1]);

	while ((read(fd, &type, sizeof(type)) > 0) && (read(fd, &sz, 4) > 0)) {
		free(buf);

		buf = malloc(sz + 1);
		((char *)buf)[sz] = '\0';
		read(fd, buf, sz);

		switch(type) {
		case RD_TEST:
			printf("test: %s\n", (char *)buf);
			break;
		case RD_CMD:
			printf("cmd: %s\n", (char *)buf);
			break;
		case RD_VERT_SHADER:
			printf("vertex shader:\n%s\n", (char *)buf);
			break;
		case RD_FRAG_SHADER:
			printf("fragment shader:\n%s\n", (char *)buf);
			break;
		case RD_GPUADDR:
			buffers[nbuffers].gpuaddr = ((uint32_t *)buf)[0];
			buffers[nbuffers].len = ((uint32_t *)buf)[1];
			break;
		case RD_BUFFER_CONTENTS:
			buffers[nbuffers].hostptr = buf;
			nbuffers++;
			buf = NULL;
			break;
		case RD_CMDSTREAM_ADDR:
			printf("############################################################\n");
			printf("cmdstream: %d dwords\n", ((uint32_t *)buf)[1]);
			dump_commands(hostptr(((uint32_t *)buf)[0]),
					((uint32_t *)buf)[1], 0);
			printf("############################################################\n");
			for (i = 0; i < nbuffers; i++) {
				free(buffers[i].hostptr);
				buffers[i].hostptr = NULL;
			}
			nbuffers = 0;
			break;
		case RD_GPU_ID:
			printf("gpu_id: %d\n", *((unsigned int *)buf));
			if (*((unsigned int *)buf) >= 300) {
				type0_reg = reg_a3xx;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

