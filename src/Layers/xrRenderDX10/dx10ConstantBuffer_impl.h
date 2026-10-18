#ifndef	dx10ConstantBuffer_impl_included
#define	dx10ConstantBuffer_impl_included
#pragma once

#include "ConstantBufferValue.h"
#include "ConstantBufferMatrix.h"

IC void dx10ConstantBuffer::Store(u16 offset, const void* value, u32 bytes)
{
	VERIFY(u32(offset) + bytes <= m_uiBufferSize);
	// Diagnostic A/B switch; normal launches always use the value cache.
	static const bool legacy = strstr(Core.Params, "-cb_legacy_uploads") != nullptr;
	if (legacy)
	{
		CopyMemory(static_cast<BYTE*>(m_pBufferData) + offset, value, bytes);
		m_bChanged = true;
		return;
	}
	if (StoreConstantBufferValue(static_cast<BYTE*>(m_pBufferData) + offset, value, bytes))
		m_bChanged = true;
}

IC void dx10ConstantBuffer::StoreMatrix(u16 offset, const Fmatrix& value, u32 rows)
{
    VERIFY(rows >= 2 && rows <= 4 && u32(offset) + rows * lineSize <= m_uiBufferSize);
    float packed[16];
    PackConstantBufferMatrix(packed, &value._11, rows);
    Store(offset, packed, rows * lineSize);
}

IC Fvector4* dx10ConstantBuffer::Access(u16 offset)
{
	//	TODO: DX10: Implement code which will check if set actually changes code.
	m_bChanged = true;

	//	Check buffer size in client code: don't know if actual data will cross
	//	buffer boundaries.
	VERIFY(offset<(int)m_uiBufferSize);
	BYTE* res = ((BYTE*)m_pBufferData) + offset;
	return (Fvector4*)res;
}

IC void dx10ConstantBuffer::set(R_constant* C, R_constant_load& L, const Fmatrix& A)
{
    VERIFY(RC_float == C->type);
    switch (L.cls)
    {
    case RC_2x4: StoreMatrix(L.index, A, 2); break;
    case RC_3x4: StoreMatrix(L.index, A, 3); break;
    case RC_4x4: StoreMatrix(L.index, A, 4); break;
    default: NODEFAULT;
    }
}

IC void dx10ConstantBuffer::set(R_constant* C, R_constant_load& L, const Fvector4& A)
{
	VERIFY(RC_float == C->type);
	VERIFY(RC_1x4 == L.cls || RC_1x3 == L.cls || RC_1x2 == L.cls);
	//Fvector4*	it	= Access(L.index);
	//it->set	(A);

	VERIFY(u32((u32)L.index+lineSize) <= m_uiBufferSize);

	size_t count = 4;
	switch (L.cls)
	{
	case RC_1x2:
		count = 2;
		break;
	case RC_1x3:
		count = 3;
		break;
	case RC_1x4:
		count = 4;
		break;
	default:
		break;
	}

	Store(L.index, &A[0], u32(count * sizeof(float)));

	//c_f.access	(L.index)->set	(A);
	//c_f.dirty	(L.index,L.index+1);
}

IC void dx10ConstantBuffer::set(R_constant* C, R_constant_load& L, float A)
{
	VERIFY(RC_float == C->type);
	VERIFY(RC_1x1 == L.cls);
	VERIFY(u32((u32)L.index+sizeof(float)) <= m_uiBufferSize);
	Store(L.index, &A, sizeof(A));

	//c_f.access	(L.index)->set	(A);
	//c_f.dirty	(L.index,L.index+1);
}

IC void dx10ConstantBuffer::set(R_constant* C, R_constant_load& L, int A)
{
	VERIFY(RC_int == C->type);
	VERIFY(RC_1x1 == L.cls);
	VERIFY(u32((u32)L.index+sizeof(int)) <= m_uiBufferSize);
	Store(L.index, &A, sizeof(A));

	//c_f.access	(L.index)->set	(A);
	//c_f.dirty	(L.index,L.index+1);
}

IC void dx10ConstantBuffer::seta(R_constant* C, R_constant_load& L, u32 e, const Fmatrix& A)
{
    VERIFY(RC_float == C->type);
    u32 rows = 0;
    switch (L.cls)
    {
    case RC_2x4: rows = 2; break;
    case RC_3x4: rows = 3; break;
    case RC_4x4: rows = 4; break;
    default: NODEFAULT;
    }
    const u32 base = u32(L.index) + rows * lineSize * e;
    VERIFY(base + rows * lineSize <= m_uiBufferSize && base <= 0xffff);
    StoreMatrix(u16(base), A, rows);
}

IC void dx10ConstantBuffer::seta(R_constant* C, R_constant_load& L, u32 e, const Fvector4& A)
{
	//	TEST
	//return;
	VERIFY(RC_float == C->type);
	VERIFY(RC_1x4 == L.cls || RC_1x3 == L.cls || RC_1x2 == L.cls);

	static const u16 lineSize = 4 * sizeof(float);
	u32 base = (u32)L.index + lineSize * e;
	VERIFY((base+lineSize) <= m_uiBufferSize);
	Store(u16(base), &A[0], lineSize);

	//u32			base	= L.index + e;
	//c_f.access	(base)->set	(A);
	//c_f.dirty	(base,base+1);
}

IC void* dx10ConstantBuffer::AccessDirect(R_constant_load& L, u32 DataSize)
{
	//	Check buffer size in client code: don't know if actual data will cross
	//	buffer boundaries.
	VERIFY(L.index<(int)m_uiBufferSize);
	BYTE* res = ((BYTE*)m_pBufferData) + L.index;

	if ((u32)L.index + DataSize <= m_uiBufferSize)
	{
		m_bChanged = true;
		return res;
	}
	else return 0;
}

#endif	//	dx10ConstantBuffer_impl_included
