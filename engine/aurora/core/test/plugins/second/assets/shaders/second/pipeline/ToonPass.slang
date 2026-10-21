#include <second/pipeline/ToonPass.slangh>
#ifndef TONE
#define TONE 0
#endif
float4 FSMain() : SV_Target { return (TONE != 0 ? 0.5 : 1.0) * mul(gGlobal.transform, mul(gPass.viewProjection, gPass.color)) * (gBatch.color + gBatch.surface); }
