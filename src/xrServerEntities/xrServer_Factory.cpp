////////////////////////////////////////////////////////////////////////////
//	Module 		: xrServer_Factory.cpp
//	Created 	: 19.09.2002
//  Modified 	: 04.06.2003
//	Author		: Oles Shyshkovtsov, Alexander Maksimchuk, Victor Reutskiy and Dmitriy Iassenev
//	Description : Server objects factory
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "object_factory.h"

CSE_Abstract* F_entity_Create(LPCSTR section)
{
	if (!pSettings->section_exist(section)) return nullptr;
	return F_entity_Create(section, pSettings->r_clsid(section, "class"));
}

CSE_Abstract* F_entity_Create(LPCSTR section, const CLASS_ID& clsid)
{
	return object_factory().server_object(clsid, section);
}
