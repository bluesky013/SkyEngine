#include <first/pipeline/ToonPass.slangh>
// Keep @SHADER_SOURCE_MARKER@ verbatim in staged shader text.
float4 FSMain() : SV_Target { return gGlobal.color * gPass.color * gBatch.color; }
