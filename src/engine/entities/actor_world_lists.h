#pragma once

#include "entity_types.h"

namespace gbtg
{
void register_actor_shape_probe(const Actor& actor, entity_detail::ShapeProbe& probe); // Xbox 0x0003C770
void clear_actor_shape_probe(Actor& actor, entity_detail::ShapeProbe& probe); // Xbox 0x0003C700

void register_actor_segment_probe(const Actor& actor, entity_detail::SegmentProbe& probe); // Xbox 0x0003E810
}
