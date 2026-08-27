#pragma once

class CBlender_upscale final : public IBlender
{
public:
    CBlender_upscale();
    LPCSTR getComment() override { return "INTERNAL: Anthology upscale present"; }
    BOOL canBeDetailed() override { return FALSE; }
    BOOL canBeLMAPped() override { return FALSE; }
    void Compile(CBlender_Compile& C) override;
};
