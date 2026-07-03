#ifdef SPINE_RUNTIME_43
/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated April 5, 2025. Replaces all prior versions.
 *
 * Copyright (c) 2013-2025, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software
 * or otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THE SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include "SpineTimeline.h"
#include "SpineSkeleton.h"
#include "SpineEvent.h"
#if VERSION_MAJOR == 3
#include "core/method_bind_ext.gen.inc"
#endif

void SpineTimeline::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply", "skeleton", "last_time", "time", "events", "alpha", "from_setup", "add", "out", "applied_pose"),
						 &SpineTimeline::apply);
	ClassDB::bind_method(D_METHOD("get_frame_entries"), &SpineTimeline::get_frame_entries);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &SpineTimeline::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_frames"), &SpineTimeline::get_frames);
	ClassDB::bind_method(D_METHOD("get_duration"), &SpineTimeline::get_duration);
	ClassDB::bind_method(D_METHOD("get_property_ids"), &SpineTimeline::get_property_ids);
	ClassDB::bind_method(D_METHOD("get_type"), &SpineTimeline::get_type);
}

void SpineTimeline::apply(Ref<SpineSkeleton> skeleton, float last_time, float time, Array events, float alpha, bool from_setup, bool add, bool out,
						  bool applied_pose) {
	SPINE_CHECK(get_spine_object(), )
	if (!skeleton->get_spine_object()) return;
	spine::Array<spine::Event *> spine_events;
	spine_events.setSize((int) events.size(), nullptr);
	for (int i = 0; i < events.size(); ++i) {
		events[i] = ((Ref<SpineEvent>) spine_events[i])->get_spine_object();
	}
	get_spine_object()->apply(*(skeleton->get_spine_object()), last_time, time, &spine_events, alpha,
							  from_setup ? spine::MixFrom_Setup : spine::MixFrom_Current, add, out, applied_pose);
}

int SpineTimeline::get_frame_entries() {
	SPINE_CHECK(get_spine_object(), 0)
	return (int) get_spine_object()->getFrameEntries();
}

int SpineTimeline::get_frame_count() {
	SPINE_CHECK(get_spine_object(), 0)
	return (int) get_spine_object()->getFrameCount();
}

Array SpineTimeline::get_frames() {
	Array result;
	SPINE_CHECK(get_spine_object(), result)
	auto &frames = get_spine_object()->getFrames();
	result.resize((int) frames.size());
	for (int i = 0; i < result.size(); ++i) {
		result[i] = frames[i];
	}
	return result;
}

float SpineTimeline::get_duration() {
	SPINE_CHECK(get_spine_object(), 0)
	return get_spine_object()->getDuration();
}

Array SpineTimeline::get_property_ids() {
	Array result;
	SPINE_CHECK(get_spine_object(), result)
	auto &ids = get_spine_object()->getPropertyIds();
	result.resize((int) ids.size());
	for (int i = 0; i < result.size(); ++i) {
		result[i] = (int64_t) ids[i];
	}
	return result;
}

String SpineTimeline::get_type() {
	SPINE_CHECK(get_spine_object(), "")
	return get_spine_object()->getRTTI().getClassName();
}
#else
/******************************************************************************
 * Spine Runtimes License Agreement
 * Last updated April 5, 2025. Replaces all prior versions.
 *
 * Copyright (c) 2013-2025, Esoteric Software LLC
 *
 * Integration of the Spine Runtimes into software or otherwise creating
 * derivative works of the Spine Runtimes is permitted under the terms and
 * conditions of Section 2 of the Spine Editor License Agreement:
 * http://esotericsoftware.com/spine-editor-license
 *
 * Otherwise, it is permitted to integrate the Spine Runtimes into software
 * or otherwise create derivative works of the Spine Runtimes (collectively,
 * "Products"), provided that each user of the Products must obtain their own
 * Spine Editor license and redistribution of the Products in any form must
 * include this license and copyright notice.
 *
 * THE SPINE RUNTIMES ARE PROVIDED BY ESOTERIC SOFTWARE LLC "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ESOTERIC SOFTWARE LLC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES,
 * BUSINESS INTERRUPTION, OR LOSS OF USE, DATA, OR PROFITS) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THE SPINE RUNTIMES, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include "SpineTimeline.h"
#include "SpineSkeleton.h"
#include "SpineEvent.h"
#ifndef SPINE_RUNTIME_4_PLUS
#include <spine/AttachmentTimeline.h>
#include <spine/ColorTimeline.h>
#include <spine/CurveTimeline.h>
#include <spine/DeformTimeline.h>
#include <spine/DrawOrderTimeline.h>
#include <spine/EventTimeline.h>
#include <spine/IkConstraintTimeline.h>
#include <spine/PathConstraintMixTimeline.h>
#include <spine/PathConstraintPositionTimeline.h>
#include <spine/RotateTimeline.h>
#include <spine/TransformConstraintTimeline.h>
#include <spine/TranslateTimeline.h>
#include <spine/TwoColorTimeline.h>
#endif
#if VERSION_MAJOR == 3
#include "core/method_bind_ext.gen.inc"
#endif

#ifndef SPINE_RUNTIME_4_PLUS
namespace {
spine::Vector<float> *get_timeline_frames(spine::Timeline *timeline) {
	if (timeline->getRTTI().isExactly(spine::AttachmentTimeline::rtti)) return &((spine::AttachmentTimeline *) timeline)->getFrames();
	if (timeline->getRTTI().isExactly(spine::ColorTimeline::rtti)) return &((spine::ColorTimeline *) timeline)->getFrames();
	if (timeline->getRTTI().isExactly(spine::DeformTimeline::rtti)) return &((spine::DeformTimeline *) timeline)->getFrames();
	if (timeline->getRTTI().isExactly(spine::DrawOrderTimeline::rtti)) return &((spine::DrawOrderTimeline *) timeline)->getFrames();
	if (timeline->getRTTI().isExactly(spine::EventTimeline::rtti)) return const_cast<spine::Vector<float> *>(&((spine::EventTimeline *) timeline)->getFrames());
	if (timeline->getRTTI().isExactly(spine::RotateTimeline::rtti)) return &((spine::RotateTimeline *) timeline)->getFrames();
	return nullptr;
}

int get_timeline_frame_count(spine::Timeline *timeline) {
	if (timeline->getRTTI().instanceOf(spine::CurveTimeline::rtti)) return (int) ((spine::CurveTimeline *) timeline)->getFrameCount();
	if (timeline->getRTTI().isExactly(spine::AttachmentTimeline::rtti)) return (int) ((spine::AttachmentTimeline *) timeline)->getFrameCount();
	if (timeline->getRTTI().isExactly(spine::DrawOrderTimeline::rtti)) return (int) ((spine::DrawOrderTimeline *) timeline)->getFrameCount();
	if (timeline->getRTTI().isExactly(spine::EventTimeline::rtti)) return (int) ((spine::EventTimeline *) timeline)->getFrameCount();
	return 0;
}

int get_timeline_frame_entries(spine::Timeline *timeline) {
	spine::Vector<float> *frames = get_timeline_frames(timeline);
	int frame_count = get_timeline_frame_count(timeline);
	if (frames && frame_count > 0) return (int) frames->size() / frame_count;

	if (timeline->getRTTI().isExactly(spine::IkConstraintTimeline::rtti)) return spine::IkConstraintTimeline::ENTRIES;
	if (timeline->getRTTI().isExactly(spine::PathConstraintMixTimeline::rtti)) return spine::PathConstraintMixTimeline::ENTRIES;
	if (timeline->getRTTI().isExactly(spine::PathConstraintPositionTimeline::rtti)) return spine::PathConstraintPositionTimeline::ENTRIES;
	if (timeline->getRTTI().isExactly(spine::TransformConstraintTimeline::rtti)) return spine::TransformConstraintTimeline::ENTRIES;
	if (timeline->getRTTI().isExactly(spine::TranslateTimeline::rtti)) return spine::TranslateTimeline::ENTRIES;
	if (timeline->getRTTI().isExactly(spine::TwoColorTimeline::rtti)) return spine::TwoColorTimeline::ENTRIES;
	return 0;
}

float get_timeline_duration(spine::Timeline *timeline) {
	spine::Vector<float> *frames = get_timeline_frames(timeline);
	int frame_count = get_timeline_frame_count(timeline);
	int frame_entries = get_timeline_frame_entries(timeline);
	if (!frames || frame_count <= 0 || frame_entries <= 0) return 0;

	int last_time_index = (frame_count - 1) * frame_entries;
	if (last_time_index < 0 || last_time_index >= (int) frames->size()) return 0;
	return (*frames)[last_time_index];
}
}
#endif

void SpineTimeline::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply", "skeleton", "last_time", "time", "events", "alpha", "blend", "direction"), &SpineTimeline::apply);
	ClassDB::bind_method(D_METHOD("get_frame_entries"), &SpineTimeline::get_frame_entries);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &SpineTimeline::get_frame_count);
	ClassDB::bind_method(D_METHOD("get_frames"), &SpineTimeline::get_frames);
	ClassDB::bind_method(D_METHOD("get_duration"), &SpineTimeline::get_duration);
	ClassDB::bind_method(D_METHOD("get_property_ids"), &SpineTimeline::get_property_ids);
	ClassDB::bind_method(D_METHOD("get_type"), &SpineTimeline::get_type);
}

void SpineTimeline::apply(Ref<SpineSkeleton> skeleton, float last_time, float time, Array events, float alpha,
						  SpineConstant::MixBlend blend, SpineConstant::MixDirection direction) {
	SPINE_CHECK(get_spine_object(), )
	if (!skeleton->get_spine_object()) return;
	spine::Vector<spine::Event *> spine_events;
	spine_events.setSize((int) events.size(), nullptr);
	for (int i = 0; i < events.size(); ++i) {
		Ref<SpineEvent> event_ref = events[i];
		spine_events[i] = event_ref.is_valid() ? event_ref->get_spine_object() : nullptr;
	}
	get_spine_object()->apply(*(skeleton->get_spine_object()), last_time, time, &spine_events, alpha, (spine::MixBlend) blend, (spine::MixDirection) direction);
}

int SpineTimeline::get_frame_entries() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return (int) get_spine_object()->getFrameEntries();
#else
	return get_timeline_frame_entries(get_spine_object());
#endif
}

int SpineTimeline::get_frame_count() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return (int) get_spine_object()->getFrameCount();
#else
	return get_timeline_frame_count(get_spine_object());
#endif
}

Array SpineTimeline::get_frames() {
	Array result;
	SPINE_CHECK(get_spine_object(), result)
#ifdef SPINE_RUNTIME_4_PLUS
	auto &frames = get_spine_object()->getFrames();
	result.resize((int) frames.size());
	for (int i = 0; i < result.size(); ++i) {
		result[i] = frames[i];
	}
#else
	spine::Vector<float> *frames = get_timeline_frames(get_spine_object());
	if (!frames) return result;
	result.resize((int) frames->size());
	for (int i = 0; i < result.size(); ++i) {
		result[i] = (*frames)[i];
	}
#endif
	return result;
}

float SpineTimeline::get_duration() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getDuration();
#else
	return get_timeline_duration(get_spine_object());
#endif
}

Array SpineTimeline::get_property_ids() {
	Array result;
	SPINE_CHECK(get_spine_object(), result)
#ifdef SPINE_RUNTIME_4_PLUS
	auto &ids = get_spine_object()->getPropertyIds();
	result.resize((int) ids.size());
	for (int i = 0; i < result.size(); ++i) {
		result[i] = (int64_t) ids[i];
	}
#else
	result.resize(1);
	result[0] = (int64_t) get_spine_object()->getPropertyId();
#endif
	return result;
}

String SpineTimeline::get_type() {
	SPINE_CHECK(get_spine_object(), "")
	return get_spine_object()->getRTTI().getClassName();
}
#endif

