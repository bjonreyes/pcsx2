/*	ZeroGS KOSMOS
 *	Copyright (C) 2005-2006 zerofrog@gmail.com
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	02111-1307	USA
 */
 
#ifndef GIFTRANSFER_H_INCLUDED
#define GIFTRANSFER_H_INCLUDED

#include "Regs.h"
#include "Util.h"

// This is fairly broken right now, and shouldn't be enabled unless you feel like fixing it.
//#define NEW_GIF_TRANSFER
enum GIF_FLG
{
	GIF_FLG_PACKED	= 0,
	GIF_FLG_REGLIST	= 1,
	GIF_FLG_IMAGE	= 2,
	GIF_FLG_IMAGE2	= 3
};

//
// GIFTag
union GIFTag
{
	u64 ai64[2];
	u32 ai32[4];
	struct
	{
		u32 NLOOP:15;
		u32 EOP:1;
		u32 _PAD1:16;
		u32 _PAD2:14;
		u32 PRE:1;
		u32 PRIM:11;
		u32 FLG:2; // enum GIF_FLG
		u32 NREG:4;
		u64 REGS:64;
	};
	void set(u32 *data)
	{
		for(int i = 0; i <= 3; i++)
		{
			ai32[i] = data[i];
		}
	}
	GIFTag(u32 *data)
	{
		set(data);
	}
	GIFTag(){ ai64[0] = 0; ai64[1] = 0; }
};

// EE part. Data transfer packet description

typedef struct 
{
	int mode;
	int regn;
	u64 regs;
	int nloop;
	int eop;
	int nreg;
	u32 adonly;
	GIFTag tag;

#ifdef NEW_GIF_TRANSFER	
	void setTag(u32 *data) 
	{
		tag.set(data);

		nloop	= tag.NLOOP;
		eop		= tag.EOP;
		mode	= tag.FLG;
		nreg	= tag.NREG;
		//regs = tag.REGS;
		//regn = 0;
		
		ERROR_LOG("GIFtag: %8.8lx_%8.8lx_%8.8lx_%8.8lx: EOP=%d, NLOOP=%x, FLG=%x, NREG=%d, PRE=%d\n",
				data[3], data[2], data[1], data[0],
				eop, nloop, mode, nreg, tag.PRE);
		

		switch (mode) 
		{
			case GIF_FLG_PACKED:
				regs = *(u64 *)(data+2);
				regn = 0;
				break;

			case GIF_FLG_REGLIST:
				regs = *(u64 *)(data+2);
				regn = 0;
				break;
		}

		adonly = (nreg == 1) && ((u8)regs == 0xe);
	}
	
	u32 GetReg() 
	{
		return (regs >> regn) & 0xf;
	}

	bool StepReg()
	{
		regn += 1;
							
		if ((regn & 0xf) == nreg) 
		{
			regn = 0;
								
			if (--nloop <= 0) 
			{
				return false;
			}
		}
		return true;
	}
#else
       
        void setTag(u32 *data) 
        {
                tag.set(data);

                nloop   = tag.NLOOP;
                eop     = tag.EOP;
                u32 tagpre              = tag.PRE;
                u32 tagprim             = tag.PRIM;
                u32 tagflg              = tag.FLG;
                
                // Hmm....
                nreg    = tag.NREG << 2;
                if (nreg == 0) nreg = 64;

        //      GS_LOG("GIFtag: %8.8lx_%8.8lx_%8.8lx_%8.8lx: EOP=%d, NLOOP=%x, FLG=%x, NREG=%d, PRE=%d\n",
        //                      data[3], data[2], data[1], data[0],
        //                      path->eop, path->nloop, tagflg, path->nreg, tagpre);

                mode = tagflg;

                switch (mode) 
                {
					case GIF_FLG_PACKED:
						regs = *(u64 *)(data+2);
						regn = 0;
						if (tagpre) GIFRegHandlerPRIM((u32*)&tagprim);

						break;

					case GIF_FLG_REGLIST:
						regs = *(u64 *)(data+2);
						regn = 0;
						break;
                }
        }
#endif
} pathInfo;

void _GSgifPacket(pathInfo *path, u32 *pMem);
void _GSgifRegList(pathInfo *path, u32 *pMem);
void _GSgifTransfer(pathInfo *path, u32 *pMem, u32 size);

extern GIFRegHandler g_GIFPackedRegHandlers[];
extern GIFRegHandler g_GIFRegHandlers[];

#endif // GIFTRANSFER_H_INCLUDED
