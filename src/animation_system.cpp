#include "animation_system.h"
#include <queue>

void AnimationSystem::init(NtshEngn::ECSInterface* ecs) {
	m_ecs = ecs;
}

void AnimationSystem::update(float dt, std::unordered_map<NtshEngn::Entity, InternalObject>& objects, std::vector<InternalMesh>& meshes, HostVisibleVulkanBuffer& jointTransformBuffer) {
	for (auto& it : objects) {
		const NtshEngn::Renderable& objectRenderable = m_ecs->getComponent<NtshEngn::Renderable>(it.first);

		if (objectRenderable.mesh) {
			const NtshEngn::Skin& skin = objectRenderable.mesh->skin;

			if (!skin.joints.empty()) {
				std::vector<NtshEngn::Math::mat4> jointTransformMatrices(skin.joints.size(), NtshEngn::Math::mat4::identity());
				std::vector<NtshEngn::Math::mat4> parentJointTransformMatrices(skin.joints.size(), NtshEngn::Math::mat4::identity());

				if (m_playingAnimations.find(&it.second) != m_playingAnimations.end()) {
					PlayingAnimation& playingAnimation = m_playingAnimations[&it.second];
					const NtshEngn::Animation& animation = objectRenderable.mesh->animations[m_playingAnimations[&it.second].animationIndex];

					std::queue<std::pair<uint32_t, uint32_t>> jointsAndParents;
					jointsAndParents.push({ skin.rootJoint, std::numeric_limits<uint32_t>::max() });
					while (!jointsAndParents.empty()) {
						uint32_t jointIndex = jointsAndParents.front().first;
						NtshEngn::Math::mat4 parentJointTransformMatrix;
						if (jointsAndParents.front().second != std::numeric_limits<uint32_t>::max()) {
							parentJointTransformMatrix = parentJointTransformMatrices[jointsAndParents.front().second];
						}
						else {
							parentJointTransformMatrix = skin.baseMatrix;
						}

						if (animation.jointChannels.find(jointIndex) != animation.jointChannels.end()) {
							const std::vector<NtshEngn::AnimationChannel>& channels = animation.jointChannels.at(jointIndex);

							NtshEngn::Math::vec3 translation;
							NtshEngn::Math::quat rotation;
							NtshEngn::Math::vec3 scale;
							NtshEngn::Math::decomposeTransform(skin.joints[jointIndex].localTransform, translation, rotation, scale);
							for (const NtshEngn::AnimationChannel& channel : channels) {
								// Find previous keyframe
								uint32_t keyframe = findPreviousAnimationKeyframe(playingAnimation.time, channel.keyframes);

								if (keyframe == std::numeric_limits<uint32_t>::max()) {
									continue;
								}

								const NtshEngn::AnimationChannelKeyframe& previousKeyframe = channel.keyframes[keyframe];

								if (channel.interpolationType == NtshEngn::AnimationChannelInterpolationType::Step) {
									const NtshEngn::Math::vec4 channelPrevious = previousKeyframe.value;

									// Step interpolation
									if (channel.transformType == NtshEngn::AnimationChannelTransformType::Translation) {
										translation = NtshEngn::Math::vec3(channelPrevious);
									}
									else if (channel.transformType == NtshEngn::AnimationChannelTransformType::Rotation) {
										rotation = NtshEngn::Math::quat(channelPrevious.x, channelPrevious.y, channelPrevious.z, channelPrevious.w);
									}
									else if (channel.transformType == NtshEngn::AnimationChannelTransformType::Scale) {
										scale = NtshEngn::Math::vec3(channelPrevious);
									}
								}
								else if (channel.interpolationType == NtshEngn::AnimationChannelInterpolationType::Linear) {
									// Linear interpolation
									const NtshEngn::Math::vec4& channelPrevious = previousKeyframe.value;

									if ((keyframe + 1) >= channel.keyframes.size()) {
										// Last keyframe
										if (channel.transformType == NtshEngn::AnimationChannelTransformType::Translation) {
											translation = NtshEngn::Math::vec3(channelPrevious);
										}
										else if (channel.transformType == NtshEngn::AnimationChannelTransformType::Rotation) {
											rotation = NtshEngn::Math::quat(channelPrevious.x, channelPrevious.y, channelPrevious.z, channelPrevious.w);
										}
										else if (channel.transformType == NtshEngn::AnimationChannelTransformType::Scale) {
											scale = NtshEngn::Math::vec3(channelPrevious);
										}
									}
									else {
										const NtshEngn::AnimationChannelKeyframe& nextKeyframe = channel.keyframes[keyframe + 1];
										const NtshEngn::Math::vec4& channelNext = nextKeyframe.value;

										const float timestampPrevious = previousKeyframe.timestamp;
										const float timestampNext = nextKeyframe.timestamp;
										const float interpolationValue = (playingAnimation.time - timestampPrevious) / (timestampNext - timestampPrevious);

										if (channel.transformType == NtshEngn::AnimationChannelTransformType::Translation) {
											translation = NtshEngn::Math::vec3(NtshEngn::Math::lerp(channelPrevious.x, channelNext.x, interpolationValue),
												NtshEngn::Math::lerp(channelPrevious.y, channelNext.y, interpolationValue),
												NtshEngn::Math::lerp(channelPrevious.z, channelNext.z, interpolationValue));
										}
										else if (channel.transformType == NtshEngn::AnimationChannelTransformType::Rotation) {
											rotation = NtshEngn::Math::normalize(NtshEngn::Math::slerp(NtshEngn::Math::quat(channelPrevious.x, channelPrevious.y, channelPrevious.z, channelPrevious.w),
												NtshEngn::Math::quat(channelNext.x, channelNext.y, channelNext.z, channelNext.w),
												interpolationValue));
										}
										else if (channel.transformType == NtshEngn::AnimationChannelTransformType::Scale) {
											scale = NtshEngn::Math::vec3(NtshEngn::Math::lerp(channelPrevious.x, channelNext.x, interpolationValue),
												NtshEngn::Math::lerp(channelPrevious.y, channelNext.y, interpolationValue),
												NtshEngn::Math::lerp(channelPrevious.z, channelNext.z, interpolationValue));
										}
									}
								}
							}

							const NtshEngn::Math::mat4 jointTransformMatrix = NtshEngn::Math::translate(translation) *
								NtshEngn::Math::quatToRotationMatrix(rotation) *
								NtshEngn::Math::scale(scale);

							jointTransformMatrices[jointIndex] = parentJointTransformMatrix * jointTransformMatrix;
							parentJointTransformMatrices[jointIndex] = jointTransformMatrices[jointIndex];
							jointTransformMatrices[jointIndex] *= skin.joints[jointIndex].inverseBindMatrix;
							jointTransformMatrices[jointIndex] = skin.inverseGlobalTransform * jointTransformMatrices[jointIndex];
						}
						else {
							jointTransformMatrices[jointIndex] = parentJointTransformMatrix * skin.joints[jointIndex].localTransform;
							parentJointTransformMatrices[jointIndex] = jointTransformMatrices[jointIndex];
							jointTransformMatrices[jointIndex] *= skin.joints[jointIndex].inverseBindMatrix;
							jointTransformMatrices[jointIndex] = skin.inverseGlobalTransform * jointTransformMatrices[jointIndex];
						}

						for (uint32_t jointChild : skin.joints[jointIndex].children) {
							jointsAndParents.push({ jointChild, jointIndex });
						}

						jointsAndParents.pop();
					}

					if (m_playingAnimations[&it.second].isPlaying) {
						playingAnimation.time += dt;
					}

					// End animation
					if (playingAnimation.time >= animation.duration) {
						m_playingAnimations.erase(&it.second);
					}
				}
				else {
					std::queue<std::pair<uint32_t, uint32_t>> jointsAndParents;
					jointsAndParents.push({ skin.rootJoint, std::numeric_limits<uint32_t>::max() });
					while (!jointsAndParents.empty()) {
						uint32_t jointIndex = jointsAndParents.front().first;
						NtshEngn::Math::mat4 parentJointTransformMatrix;
						if (jointsAndParents.front().second != std::numeric_limits<uint32_t>::max()) {
							parentJointTransformMatrix = parentJointTransformMatrices[jointsAndParents.front().second];
						}
						else {
							parentJointTransformMatrix = skin.baseMatrix;
						}

						jointTransformMatrices[jointIndex] = parentJointTransformMatrix * skin.joints[jointIndex].localTransform;
						parentJointTransformMatrices[jointIndex] = jointTransformMatrices[jointIndex];
						jointTransformMatrices[jointIndex] *= skin.joints[jointIndex].inverseBindMatrix;
						jointTransformMatrices[jointIndex] = skin.inverseGlobalTransform * jointTransformMatrices[jointIndex];

						for (uint32_t jointChild : skin.joints[jointIndex].children) {
							jointsAndParents.push({ jointChild, jointIndex });
						}

						jointsAndParents.pop();
					}
				}

				memcpy(reinterpret_cast<char*>(jointTransformBuffer.address) + (sizeof(NtshEngn::Math::mat4) * it.second.jointTransformOffset), jointTransformMatrices.data(), sizeof(NtshEngn::Math::mat4) * meshes[it.second.meshID].jointCount);
			}
		}
	}
}

void AnimationSystem::playAnimation(InternalObject* object, uint32_t animationIndex) {
	if (m_playingAnimations.find(object) != m_playingAnimations.end()) {
		if (m_playingAnimations[object].animationIndex == animationIndex) {
			m_playingAnimations[object].isPlaying = true;

			return;
		}
	}

	PlayingAnimation playingAnimation;
	playingAnimation.animationIndex = animationIndex;
	m_playingAnimations[object] = playingAnimation;
}

void AnimationSystem::pauseAnimation(InternalObject* object) {
	if (m_playingAnimations.find(object) != m_playingAnimations.end()) {
		if (m_playingAnimations[object].isPlaying) {
			m_playingAnimations[object].isPlaying = false;
		}
	}
}

void AnimationSystem::stopAnimation(InternalObject* object) {
	if (m_playingAnimations.find(object) != m_playingAnimations.end()) {
		m_playingAnimations.erase(object);
	}
}

void AnimationSystem::setAnimationCurrentTime(InternalObject* object, float time) {
	if (m_playingAnimations.find(object) != m_playingAnimations.end()) {
		m_playingAnimations[object].time = time;
	}
}

bool AnimationSystem::isAnimationPlaying(InternalObject* object, uint32_t animationIndex) {
	if (m_playingAnimations.find(object) != m_playingAnimations.end()) {
		if (m_playingAnimations[object].animationIndex == animationIndex) {
			return m_playingAnimations[object].isPlaying;
		}
	}

	return false;
}

uint32_t AnimationSystem::findPreviousAnimationKeyframe(float time, const std::vector<NtshEngn::AnimationChannelKeyframe>& keyframes) {
	const std::vector<NtshEngn::AnimationChannelKeyframe>::const_iterator previousKeyframe = std::lower_bound(keyframes.begin(), keyframes.end(), time, [](const NtshEngn::AnimationChannelKeyframe& keyframe, float time) {
		return keyframe.timestamp < time;
		});

	if (previousKeyframe != keyframes.end()) {
		return static_cast<uint32_t>(std::distance(keyframes.begin(), previousKeyframe));
	}

	return std::numeric_limits<uint32_t>::max();
}
