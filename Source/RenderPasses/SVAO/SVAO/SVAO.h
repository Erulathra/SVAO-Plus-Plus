#pragma once

#include "Core/Plugin.h"

#include "../VAOBase.h"

using namespace Falcor;

class SVAO : public VAOBase {
public:
    FALCOR_PLUGIN_CLASS(SVAO, "SVAO", "Stochastic Volumetric Ambient Occasion");
};
