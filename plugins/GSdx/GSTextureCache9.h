/* 
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSTextureCache.h"
#include "GSDevice9.h"

class GSTextureCache9 : public GSTextureCache
{
	class Source9 : public Source
	{
	protected:
		int Get8bitFormat() {return D3DFMT_A8;}

	public:
		explicit Source9(GSRenderer* r) : Source(r) {}
	};

	class Target9 : public Target
	{
	public:
		explicit Target9(GSRenderer* r) : Target(r) {}

		void Read(const GSVector4i& r);
	};

protected:
	Source* CreateSource() {return new Source9(m_renderer);}
	Target* CreateTarget() {return new Target9(m_renderer);}

public:
	GSTextureCache9(GSRenderer* r);
};