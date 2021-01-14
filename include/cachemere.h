#include "cachemere/policy/eviction_lru.h"
#include "cachemere/policy/eviction_segmented_lru.h"

#include "cachemere/policy/insertion_always.h"
#include "cachemere/policy/insertion_tinylfu.h"

#include "cachemere/cache.h"
#include "cachemere/measurement.h"
#include "cachemere/presets.h"
