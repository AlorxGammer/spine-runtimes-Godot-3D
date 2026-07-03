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

#include "SpineTransformConstraint.h"
#include "SpineTransformConstraintPose.h"
#include "SpineCommon.h"
#include "SpineSkeleton.h"
#include "SpineSprite.h"

void SpineTransformConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update", "skeleton"), &SpineTransformConstraint::update);
	ClassDB::bind_method(D_METHOD("get_data"), &SpineTransformConstraint::get_data);
	ClassDB::bind_method(D_METHOD("get_bones"), &SpineTransformConstraint::get_bones);
	ClassDB::bind_method(D_METHOD("get_source"), &SpineTransformConstraint::get_source);
	ClassDB::bind_method(D_METHOD("set_source", "v"), &SpineTransformConstraint::set_source);
	ClassDB::bind_method(D_METHOD("get_pose"), &SpineTransformConstraint::get_pose);
	ClassDB::bind_method(D_METHOD("get_applied_pose"), &SpineTransformConstraint::get_applied_pose);
	ClassDB::bind_method(D_METHOD("is_active"), &SpineTransformConstraint::is_active);
	ClassDB::bind_method(D_METHOD("set_active", "v"), &SpineTransformConstraint::set_active);
}

void SpineTransformConstraint::update(Ref<SpineSkeleton> skeleton) {
	SPINE_CHECK(get_spine_object(), )
	get_spine_object()->update(*skeleton->get_spine_object(), spine::Physics_Update);
}

Ref<SpineTransformConstraintData> SpineTransformConstraint::get_data() {
	SPINE_CHECK(get_spine_object(), nullptr)
	auto &data = get_spine_object()->getData();
	Ref<SpineTransformConstraintData> data_ref(memnew(SpineTransformConstraintData));
	data_ref->set_spine_object(*get_spine_owner()->get_skeleton_data_res(), &data);
	return data_ref;
}

Array SpineTransformConstraint::get_bones() {
	Array result;
	SPINE_CHECK(get_spine_object(), result)
	auto &bones = get_spine_object()->getBones();
	result.resize((int) bones.size());
	for (int i = 0; i < bones.size(); ++i) {
		auto bone = bones[i];
		Ref<SpineBonePose> bone_ref(memnew(SpineBonePose));
		bone_ref->set_spine_object(get_spine_owner(), bone);
		result[i] = bone_ref;
	}
	return result;
}

Ref<SpineBone> SpineTransformConstraint::get_source() {
	SPINE_CHECK(get_spine_object(), nullptr)
	auto source = &get_spine_object()->getSource();
	if (!source) return nullptr;
	Ref<SpineBone> target_ref(memnew(SpineBone));
	target_ref->set_spine_object(get_spine_owner(), source);
	return target_ref;
}

void SpineTransformConstraint::set_source(Ref<SpineBone> v) {
	SPINE_CHECK(get_spine_object(), )
	if (v.is_valid() && v->get_spine_object()) {
		get_spine_object()->setSource(*v->get_spine_object());
	}
}

Ref<SpineTransformConstraintPose> SpineTransformConstraint::get_pose() {
	SPINE_CHECK(get_spine_object(), nullptr)
	auto &pose = get_spine_object()->getPose();
	Ref<SpineTransformConstraintPose> pose_ref(memnew(SpineTransformConstraintPose));
	pose_ref->set_spine_object(get_spine_owner(), &pose);
	return pose_ref;
}

Ref<SpineTransformConstraintPose> SpineTransformConstraint::get_applied_pose() {
	SPINE_CHECK(get_spine_object(), nullptr)
	auto &pose = get_spine_object()->getAppliedPose();
	Ref<SpineTransformConstraintPose> pose_ref(memnew(SpineTransformConstraintPose));
	pose_ref->set_spine_object(get_spine_owner(), &pose);
	return pose_ref;
}

bool SpineTransformConstraint::is_active() {
	SPINE_CHECK(get_spine_object(), false)
	return get_spine_object()->isActive();
}

void SpineTransformConstraint::set_active(bool v) {
	SPINE_CHECK(get_spine_object(), )
	get_spine_object()->setActive(v);
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

#include "SpineTransformConstraint.h"
#include "SpineCommon.h"
#include "SpineSprite.h"

void SpineTransformConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &SpineTransformConstraint::update);
	ClassDB::bind_method(D_METHOD("get_data"), &SpineTransformConstraint::get_data);
	ClassDB::bind_method(D_METHOD("get_bones"), &SpineTransformConstraint::get_bones);
	ClassDB::bind_method(D_METHOD("get_target"), &SpineTransformConstraint::get_target);
	ClassDB::bind_method(D_METHOD("set_target", "v"), &SpineTransformConstraint::set_target);
	ClassDB::bind_method(D_METHOD("get_mix_rotate"), &SpineTransformConstraint::get_mix_rotate);
	ClassDB::bind_method(D_METHOD("set_mix_rotate", "v"), &SpineTransformConstraint::set_mix_rotate);
	ClassDB::bind_method(D_METHOD("get_mix_x"), &SpineTransformConstraint::get_mix_x);
	ClassDB::bind_method(D_METHOD("set_mix_x", "v"), &SpineTransformConstraint::set_mix_x);
	ClassDB::bind_method(D_METHOD("get_mix_y"), &SpineTransformConstraint::get_mix_y);
	ClassDB::bind_method(D_METHOD("set_mix_y", "v"), &SpineTransformConstraint::set_mix_y);
	ClassDB::bind_method(D_METHOD("get_mix_scale_x"), &SpineTransformConstraint::get_mix_scale_x);
	ClassDB::bind_method(D_METHOD("set_mix_scale_x", "v"), &SpineTransformConstraint::set_mix_scale_x);
	ClassDB::bind_method(D_METHOD("get_mix_scale_y"), &SpineTransformConstraint::get_mix_scale_y);
	ClassDB::bind_method(D_METHOD("set_mix_scale_y", "v"), &SpineTransformConstraint::set_mix_scale_y);
	ClassDB::bind_method(D_METHOD("get_mix_shear_y"), &SpineTransformConstraint::get_mix_shear_y);
	ClassDB::bind_method(D_METHOD("set_mix_shear_y", "v"), &SpineTransformConstraint::set_mix_shear_y);
	ClassDB::bind_method(D_METHOD("is_active"), &SpineTransformConstraint::is_active);
	ClassDB::bind_method(D_METHOD("set_active", "v"), &SpineTransformConstraint::set_active);
}

void SpineTransformConstraint::update() {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_42_PLUS
	get_spine_object()->update(spine::Physics_Update);
#else
	get_spine_object()->update();
#endif
}

int SpineTransformConstraint::get_order() {
	SPINE_CHECK(get_spine_object(), 0)
	return get_spine_object()->getOrder();
}

Ref<SpineTransformConstraintData> SpineTransformConstraint::get_data() {
	SPINE_CHECK(get_spine_object(), nullptr)
	auto &data = get_spine_object()->getData();
	Ref<SpineTransformConstraintData> data_ref(memnew(SpineTransformConstraintData));
	data_ref->set_spine_object(*get_spine_owner()->get_skeleton_data_res(), &data);
	return data_ref;
}

Array SpineTransformConstraint::get_bones() {
	Array result;
	SPINE_CHECK(get_spine_object(), result)
	auto &bones = get_spine_object()->getBones();
	result.resize((int) bones.size());
	for (int i = 0; i < bones.size(); ++i) {
		auto bone = bones[i];
		Ref<SpineBone> bone_ref(memnew(SpineBone));
		bone_ref->set_spine_object(get_spine_owner(), bone);
		result[i] = bone_ref;
	}
	return result;
}

Ref<SpineBone> SpineTransformConstraint::get_target() {
	SPINE_CHECK(get_spine_object(), nullptr)
	auto target = get_spine_object()->getTarget();
	if (!target) return nullptr;
	Ref<SpineBone> target_ref(memnew(SpineBone));
	target_ref->set_spine_object(get_spine_owner(), target);
	return target_ref;
}

void SpineTransformConstraint::set_target(Ref<SpineBone> v) {
	SPINE_CHECK(get_spine_object(), )
	get_spine_object()->setTarget(v.is_valid() && v->get_spine_object() ? v->get_spine_object() : nullptr);
}

float SpineTransformConstraint::get_mix_rotate() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getMixRotate();
#else
	return get_spine_object()->getRotateMix();
#endif
}

void SpineTransformConstraint::set_mix_rotate(float v) {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_4_PLUS
	get_spine_object()->setMixRotate(v);
#else
	get_spine_object()->setRotateMix(v);
#endif
}

float SpineTransformConstraint::get_mix_x() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getMixX();
#else
	return get_spine_object()->getTranslateMix();
#endif
}

void SpineTransformConstraint::set_mix_x(float v) {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_4_PLUS
	get_spine_object()->setMixX(v);
#else
	get_spine_object()->setTranslateMix(v);
#endif
}

float SpineTransformConstraint::get_mix_y() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getMixY();
#else
	return get_spine_object()->getTranslateMix();
#endif
}

void SpineTransformConstraint::set_mix_y(float v) {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_4_PLUS
	get_spine_object()->setMixY(v);
#else
	get_spine_object()->setTranslateMix(v);
#endif
}

float SpineTransformConstraint::get_mix_scale_x() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getMixScaleX();
#else
	return get_spine_object()->getScaleMix();
#endif
}

void SpineTransformConstraint::set_mix_scale_x(float v) {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_4_PLUS
	get_spine_object()->setMixScaleX(v);
#else
	get_spine_object()->setScaleMix(v);
#endif
}

float SpineTransformConstraint::get_mix_scale_y() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getMixScaleY();
#else
	return get_spine_object()->getScaleMix();
#endif
}

void SpineTransformConstraint::set_mix_scale_y(float v) {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_4_PLUS
	get_spine_object()->setMixScaleY(v);
#else
	get_spine_object()->setScaleMix(v);
#endif
}

float SpineTransformConstraint::get_mix_shear_y() {
	SPINE_CHECK(get_spine_object(), 0)
#ifdef SPINE_RUNTIME_4_PLUS
	return get_spine_object()->getMixShearY();
#else
	return get_spine_object()->getShearMix();
#endif
}

void SpineTransformConstraint::set_mix_shear_y(float v) {
	SPINE_CHECK(get_spine_object(), )
#ifdef SPINE_RUNTIME_4_PLUS
	get_spine_object()->setMixShearY(v);
#else
	get_spine_object()->setShearMix(v);
#endif
}

bool SpineTransformConstraint::is_active() {
	SPINE_CHECK(get_spine_object(), false)
	return get_spine_object()->isActive();
}

void SpineTransformConstraint::set_active(bool v) {
	SPINE_CHECK(get_spine_object(), )
	get_spine_object()->setActive(v);
}
#endif

