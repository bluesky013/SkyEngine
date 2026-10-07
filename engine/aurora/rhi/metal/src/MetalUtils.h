//
// Umbrella for the Metal private conversion/limit headers. Split into
// MetalLimits (slot constants), MetalFormat (pixel/vertex/descriptor formats)
// and MetalState (pipeline/encoder state) for cohesion; keeping this umbrella
// so backend sources have a single include to reach all of them.
//

#pragma once

#include "MetalFormat.h"
#include "MetalLimits.h"
#include "MetalState.h"
