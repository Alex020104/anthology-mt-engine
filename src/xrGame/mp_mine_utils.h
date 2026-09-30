#pragma once

class CScriptGameObject;

enum EMPMinePlantMode : u8
{
	mpMineProximity = 0,
	mpMineTimer10 = 1,
	mpMineTimer30 = 2,
};

bool mp_mine_plant(CScriptGameObject* script_item, u8 mode);
