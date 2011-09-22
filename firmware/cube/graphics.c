/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker cube firmware
 *
 * M. Elizabeth Scott <beth@sifteo.com>
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <string.h>
#include "hardware.h"
#include "lcd.h"
#include "flash.h"
#include "radio.h"


/***********************************************************************
 * Common Macros
 **********************************************************************/

#pragma sdcc_hash +
#define MODE_RETURN() {						\
		__asm	inc	(_ack_data + RF_ACK_FRAME)	__endasm ; \
		__asm	orl	_ack_len, #RF_ACK_LEN_FRAME	__endasm ; \
		return;						\
	}

// Output a nonzero number of of pixels, not known at compile-time
#define PIXEL_BURST(_count) {				\
	register uint8_t _i = (_count);			\
	do {						\
	    ADDR_INC4();				\
	} while (--_i);					\
    }

// Output one pixel with static colors from two registers
#define PIXEL_FROM_REGS(l, h)					__endasm; \
    __asm mov	BUS_PORT, l					__endasm; \
    __asm inc	ADDR_PORT					__endasm; \
    __asm inc	ADDR_PORT					__endasm; \
    __asm mov	BUS_PORT, h					__endasm; \
    __asm inc	ADDR_PORT					__endasm; \
    __asm inc	ADDR_PORT					__endasm; \
    __asm

// Load a 16-bit tile address from DPTR without incrementing
#pragma sdcc_hash +
#define ADDR_FROM_DPTR() {					\
    __asm movx	a, @dptr					__endasm; \
    __asm mov	ADDR_PORT, a					__endasm; \
    __asm inc	dptr						__endasm; \
    __asm mov	CTRL_PORT, #CTRL_FLASH_OUT | CTRL_FLASH_LAT1	__endasm; \
    __asm movx	a, @dptr					__endasm; \
    __asm mov	ADDR_PORT, a					__endasm; \
    __asm dec	dpl						__endasm; \
    __asm mov	CTRL_PORT, #CTRL_FLASH_OUT | CTRL_FLASH_LAT2	__endasm; \
    }

// Load a 16-bit tile address from DPTR, and auto-increment
#pragma sdcc_hash +
#define ADDR_FROM_DPTR_INC() {					\
    __asm movx	a, @dptr					__endasm; \
    __asm mov	ADDR_PORT, a					__endasm; \
    __asm inc	dptr						__endasm; \
    __asm mov	CTRL_PORT, #CTRL_FLASH_OUT | CTRL_FLASH_LAT1	__endasm; \
    __asm movx	a, @dptr					__endasm; \
    __asm mov	ADDR_PORT, a					__endasm; \
    __asm inc	dptr						__endasm; \
    __asm mov	CTRL_PORT, #CTRL_FLASH_OUT | CTRL_FLASH_LAT2	__endasm; \
    }

// Add 2 to DPTR. (Can do this in 2 clocks with inline assembly)
#define DPTR_INC2() {						\
    __asm inc	dptr						__endasm; \
    __asm inc	dptr						__endasm; \
    }


/*
 * XXXX Junk...
 */
#if 0

static void line_bg_spr0(void)
{
    uint8_t x = 15;
    uint8_t bg_wrap = x_bg_wrap;
    register uint8_t spr0_mask = lvram.sprites[0].mask_x;
    register uint8_t spr0_x = lvram.sprites[0].x + x_bg_first_w;
    uint8_t spr0_pixel_addr = (spr0_x - 1) << 2;

    DPTR = y_bg_map;

    // First partial or full tile
    ADDR_FROM_DPTR_INC();
    MAP_WRAP_CHECK();
    ADDR_PORT = y_bg_addr_l + x_bg_first_addr;
    PIXEL_BURST(x_bg_first_w);

    // There are always 15 full tiles on-screen
    do {
	if ((spr0_x & spr0_mask) && ((spr0_x + 7) & spr0_mask)) {
	    // All 8 pixels are non-sprite

	    ADDR_FROM_DPTR_INC();
	    MAP_WRAP_CHECK();
	    ADDR_PORT = y_bg_addr_l;
	    ADDR_INC32();
	    spr0_x += 8;
	    spr0_pixel_addr += 32;

	} else {
	    // A mixture of sprite and tile pixels.

#define SPR0_OPAQUE(i)				\
	test_##i:				\
	    if (BUS_PORT == CHROMA_KEY)		\
		goto transparent_##i;		\
	    ADDR_INC4();			\

#define SPR0_TRANSPARENT_TAIL(i)		\
	transparent_##i:			\
	    ADDR_FROM_DPTR();			\
	    ADDR_PORT = y_bg_addr_l + (i*4);	\
	    ADDR_INC4();			\

#define SPR0_TRANSPARENT(i, j)			\
	    SPR0_TRANSPARENT_TAIL(i);		\
	    ADDR_FROM_SPRITE(0);		\
	    ADDR_PORT = spr0_pixel_addr + (j*4);\
	    goto test_##j;			\

#define SPR0_END()				\
	    spr0_x += 8;			\
	    spr0_pixel_addr += 32;		\
	    DPTR_INC2();			\
	    MAP_WRAP_CHECK();			\
	    continue;				\

	    // Fast path: All opaque pixels in a row.

	    // XXX: The assembly generated by sdcc for this loop is okayish, but
	    //      still rather bad. There are still a lot of gains left to be had
	    //      by using inline assembly here.

	    ADDR_FROM_SPRITE(0);
	    ADDR_PORT = spr0_pixel_addr;
	    SPR0_OPAQUE(0);
	    SPR0_OPAQUE(1);
	    SPR0_OPAQUE(2);
	    SPR0_OPAQUE(3);
	    SPR0_OPAQUE(4);
	    SPR0_OPAQUE(5);
	    SPR0_OPAQUE(6);
	    SPR0_OPAQUE(7);
	    SPR0_END();

	    // Transparent pixel jump targets

	    SPR0_TRANSPARENT(0, 1);
	    SPR0_TRANSPARENT(1, 2);
	    SPR0_TRANSPARENT(2, 3);
	    SPR0_TRANSPARENT(3, 4);
	    SPR0_TRANSPARENT(4, 5);
	    SPR0_TRANSPARENT(5, 6);
	    SPR0_TRANSPARENT(6, 7);
	    SPR0_TRANSPARENT_TAIL(7);
	    SPR0_END();
	}

    } while (--x);

    // Might be one more partial tile
    if (x_bg_last_w) {
	ADDR_FROM_DPTR_INC();
	MAP_WRAP_CHECK();
	ADDR_PORT = y_bg_addr_l;
	PIXEL_BURST(x_bg_last_w);
    }

    do {
	uint8_t active_sprites = 0;

	/*
	 * Per-line sprite accounting. Update all Y coordinates, and
	 * see which sprites are active. (Unrolled loop here, to allow
	 * calculating masks and array addresses at compile-time.)
	 */

#define SPRITE_Y_ACCT(i)						\
	if (!(++lvram.sprites[i].y & lvram.sprites[i].mask_y)) {	\
	    active_sprites |= 1 << i;					\
	    lvram.sprites[i].addr_l += 2;				\
	}							   	\

	SPRITE_Y_ACCT(0);
	SPRITE_Y_ACCT(1);

	/*
	 * Choose a scanline renderer
	 */

	switch (active_sprites) {
	case 0x00:	line_bg(); break;
	case 0x01:	line_bg_spr0(); break;
	}
#endif



static void vm_02(void)
{
    MODE_RETURN();
}

static void vm_06(void)
{
    MODE_RETURN();
}

static void vm_0c(void)
{
    MODE_RETURN();
}

static void vm_0e(void)
{
    MODE_RETURN();
}


/***********************************************************************
 * _SYS_VM_POWERDOWN
 **********************************************************************/

static void vm_powerdown(void)
{
    lcd_sleep();
    MODE_RETURN();
}


/***********************************************************************
 * _SYS_VM_SOLID
 **********************************************************************/

/*
 * Copy vram.color to the LCD bus, and repeat for every pixel.
 */

static void vm_solid(void)
{
    lcd_begin_frame();
    LCD_WRITE_BEGIN();

    __asm
	mov	dptr, #_SYS_VA_COLOR
	movx	a, @dptr
	mov	r0, a
	inc	dptr
	movx	a, @dptr

	mov	r1, #64
1$:	mov	r2, #0
2$:

        PIXEL_FROM_REGS(r0, a)

	djnz	r2, 2$
	djnz	r1, 1$
    __endasm ;

    LCD_WRITE_END();
    lcd_end_frame();
    MODE_RETURN();
}


/***********************************************************************
 * _SYS_VM_FB32
 **********************************************************************/

/*
 * 16-color 32x32 framebuffer mode.
 *
 * To support fast color lookups without copying the whole LUT into
 * internal memory, we use both DPTRs here.
 */

static void vm_fb32_pixel(void) __naked {
    /*
     * Draw four pixels of the same color, from the LUT index at 'a'.
     * Trashes only r0 and a. Assumes DPH1 is already pointed at the colormap.
     */

    __asm
	rl	a
	mov	_DPL1, a

	mov	_DPS, #1
	movx	a, @dptr
	mov	r0, a
	inc	dptr
	movx	a, @dptr
	mov	_DPS, #0

    	PIXEL_FROM_REGS(r0, a)
	PIXEL_FROM_REGS(r0, a)
	PIXEL_FROM_REGS(r0, a)
	PIXEL_FROM_REGS(r0, a)

	ret
    __endasm;
}

void vm_fb32(void)
{
    lcd_begin_frame();
    LCD_WRITE_BEGIN();

    __asm
	mov	_DPH1, #(_SYS_VA_COLORMAP >> 8)
	
	mov	r1, #0		; Loop over 2 banks in DPH
1$:	mov	r2, #0		; Loop over 16 framebuffer lines per bank
2$:	mov	r3, #4		; Loop over 4 display lines per framebuffer line
3$:	mov	DPH, r1
	mov	DPL, r2
	mov	r4, #16		; Loop over 16 horizontal bytes per line
4$:

	movx	a, @dptr
	inc	dptr
	mov	r5, a

	anl	a, #0xF		; Pixel from low nybble
	lcall	_vm_fb32_pixel

	mov	a, r5		; Pixel from high nybble
	anl	a, #0xF0
	swap	a
	lcall	_vm_fb32_pixel

	djnz	r4, 4$		; Next byte
	djnz	r3, 3$		; Next line

	mov	a, r2		; Next framebuffer line
	add	a, #0x10
	mov	r2, a
	jnz	2$

	mov	a, r1		; Next DPH bank
	jnz	5$
	inc	r1
	sjmp	1$
5$:

    __endasm ;

    LCD_WRITE_END();
    lcd_end_frame();
    MODE_RETURN();
}


/***********************************************************************
 * _SYS_VM_FB64
 **********************************************************************/

/*
 * 2-color 64x64 framebuffer mode.
 */

void vm_fb64(void)
{
    lcd_begin_frame();
    LCD_WRITE_BEGIN();

    __asm
	; Copy colormap[0] and colormap[1] to r4-7

	mov	dptr, #_SYS_VA_COLORMAP
	movx	a, @dptr
	mov	r4, a
	inc	dptr
	movx	a, @dptr
	mov	r5, a
	inc	dptr
	movx	a, @dptr
	mov	r6, a
	inc	dptr
	movx	a, @dptr
	mov	r7, a

	mov	_DPH1, #0	; Loop over two banks	
1$:	mov	r0, #0		; Loop over 32 framebuffer lines per bank
2$:	mov	r1, #2		; Loop over 2 screen lines per framebuffer line
3$:	mov	DPH, _DPH1
	mov	DPL, r0
	mov	r2, #8		; Loop over 8 horizontal bytes per line
4$:	movx	a, @dptr
	inc	dptr
	mov	r3, #8		; Loop over 8 pixels per byte
5$:	rrc	a		; Shift out one pixel
	jc	6$

        PIXEL_FROM_REGS(r4, r5)	; colormap[0]
	PIXEL_FROM_REGS(r4, r5)
	djnz	r3, 5$		; Next pixel
	djnz	r2, 4$		; Next byte
	djnz	r1, 3$		; Next line
	sjmp	7$		; Next line

6$:	PIXEL_FROM_REGS(r6, r7)	; colormap[1]
	PIXEL_FROM_REGS(r6, r7)
	djnz	r3, 5$		; Next pixel
	djnz	r2, 4$		; Next byte
	djnz	r1, 3$		; Next line

7$:	mov	a, r0		; Next framebuffer line
	add	a, #8
	mov	r0, a
	jnz	2$

	mov	a, _DPH1	; Next DPH bank
	jnz	8$
	inc	_DPH1
	sjmp	1$
8$:

    __endasm ;

    LCD_WRITE_END();
    lcd_end_frame();
    MODE_RETURN();
}


/***********************************************************************
 * _SYS_VM_BG0
 **********************************************************************/

static uint8_t x_bg0_first_w;		// Width of first displayed background tile, [1, 8]
static uint8_t x_bg0_last_w;		// Width of first displayed background tile, [0, 7]
static uint8_t x_bg0_first_addr;	// Low address offset for first displayed tile
static uint8_t x_bg0_wrap;		// Load value for a dec counter to the next X map wraparound

static uint8_t y_bg0_addr_l;		// Low part of tile addresses, inc by 32 each line
static uint16_t y_bg0_map;		// Map address for the first tile on this line

// Called once per tile, to check for horizontal map wrapping
#define BG0_WRAP_CHECK() {			  	\
	if (!--bg0_wrap)				\
	    DPTR -= RF_VRAM_STRIDE *2;			\
    }

static void vm_bg0_line(void)
{
    /*
     * Scanline renderer, draws a single tiled background layer.
     */

    uint8_t x = 15;
    uint8_t bg0_wrap = x_bg0_wrap;

    DPTR = y_bg0_map;

    // First partial or full tile
    ADDR_FROM_DPTR_INC();
    BG0_WRAP_CHECK();
    ADDR_PORT = y_bg0_addr_l + x_bg0_first_addr;
    PIXEL_BURST(x_bg0_first_w);

    // There are always 15 full tiles on-screen
    do {
	ADDR_FROM_DPTR_INC();
	BG0_WRAP_CHECK();
	ADDR_PORT = y_bg0_addr_l;
	ADDR_INC32();
    } while (--x);

    // Might be one more partial tile
    if (x_bg0_last_w) {
	ADDR_FROM_DPTR_INC();
	BG0_WRAP_CHECK();
	ADDR_PORT = y_bg0_addr_l;
	PIXEL_BURST(x_bg0_last_w);
    }

    // Release the bus
    CTRL_PORT = CTRL_IDLE;
}

static void vm_bg0_setup(void)
{
    /*
     * Once-per-frame setup for BG0
     */
     
    uint8_t pan_x, pan_y;
    uint8_t tile_pan_x, tile_pan_y;

    cli();
    pan_x = vram.bg0_x;
    pan_y = vram.bg0_y;
    sti();

    tile_pan_x = pan_x >> 3;
    tile_pan_y = pan_y >> 3;

    y_bg0_addr_l = pan_y << 5;
    y_bg0_map = tile_pan_y << 2;		// Y tile * 2
    y_bg0_map += tile_pan_y << 5;		// Y tile * 16
    y_bg0_map += tile_pan_x << 1;		// X tile * 2;

    x_bg0_last_w = pan_x & 7;
    x_bg0_first_w = 8 - x_bg0_last_w;
    x_bg0_first_addr = (pan_x << 2) & 0x1C;
    x_bg0_wrap = _SYS_VRAM_BG0_WIDTH - tile_pan_x;
}

static void vm_bg0_next(void)
{
    /*
     * Advance BG0 state to the next line
     */

    y_bg0_addr_l += 32;
    if (!y_bg0_addr_l) {
	// Next tile, with vertical wrap
	y_bg0_map += _SYS_VRAM_BG0_WIDTH * 2;
	if (y_bg0_map >= _SYS_VRAM_BG0_WIDTH * _SYS_VRAM_BG0_WIDTH * 2)
	    y_bg0_map -= _SYS_VRAM_BG0_WIDTH * _SYS_VRAM_BG0_WIDTH * 2;
    }
}

static void vm_bg0(void)
{
    uint8_t y = 128;

    lcd_begin_frame();
    vm_bg0_setup();

    do {
	vm_bg0_line();
	vm_bg0_next();
	flash_handle_fifo();
    } while (--y);    

    lcd_end_frame();
    MODE_RETURN();
}


/***********************************************************************
 * Graphics mode dispatch
 **********************************************************************/

void graphics_render(void) __naked
{
    /*
     * Check the toggle bit (rendering trigger), in bit 1 of
     * vram.flags. If it matches the LSB of frame_count, we have
     * nothing to do.
     */

    __asm
	mov	dptr, #_SYS_VA_FLAGS
	movx	a, @dptr
	jb	acc.3, 1$			; Handle _SYS_VF_CONTINUOUS
	rr	a
	xrl	a, (_ack_data + RF_ACK_FRAME)	; Compare _SYS_VF_TOGGLE with frame_count LSB
	rrc	a
	jnc	3$				; Return if no toggle
1$:
    __endasm ;

    /*
     * Video mode jump table.
     *
     * This acts as a tail-call. The mode-specific function returns
     * on our behalf, after acknowledging the frame.
     */

    __asm
	mov	dptr, #_SYS_VA_MODE
	movx	a, @dptr
	anl	a, #_SYS_VM_MASK
	mov	dptr, #2$
	jmp	@a+dptr
2$:
	ljmp	_vm_powerdown
	nop
        ljmp	_vm_02
	nop
	ljmp	_vm_fb32
	nop
	ljmp	_vm_fb64
	nop
	ljmp	_vm_solid
	nop
	ljmp	_vm_bg0
	nop
	ljmp	_vm_0c
	nop
	ljmp	_vm_0e

3$:	ret

    __endasm ;
}
