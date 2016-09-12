#include "animation_importer.h"
#include <yaml-cpp/yaml.h>
#include "halley/core/graphics/sprite/animation.h"
#include "halley/support/exception.h"
#include "halley/file/byte_serializer.h"

using namespace Halley;

std::vector<Path> AnimationImporter::import(const ImportingAsset& asset, Path dstDir, ProgressReporter reporter, AssetCollector collector)
{
	Animation animation;
	parseAnimation(animation, gsl::as_bytes(gsl::span<const Byte>(asset.inputFiles.at(0).data)));

	Path dst = asset.inputFiles[0].name;
	dst.replace_extension("");
	FileSystem::writeFile(dstDir / dst, Serializer::toBytes(animation));

	return { dst };
}

void AnimationImporter::parseAnimation(Animation& animation, gsl::span<const gsl::byte> data)
{
	String strData(reinterpret_cast<const char*>(data.data()), data.size());
	YAML::Node root = YAML::Load(strData.cppStr());

	animation.name = root["name"].as<std::string>();

	if (root["spriteSheet"].IsDefined()) {
		animation.spriteSheetName = root["spriteSheet"].as<std::string>();
	}

	if (root["material"].IsDefined()) {
		animation.materialName = root["material"].as<std::string>();
	}

	auto& directions = animation.directions;
	if (root["directions"].IsDefined()) {
		for (auto directionNode : root["directions"]) {
			String name = directionNode["name"].as<std::string>("default");
			String fileName = directionNode["fileName"].as<std::string>(name);
			bool flip = directionNode["flip"].as<bool>(false);
			size_t idx = directions.size();
			directions.emplace_back(AnimationDirection(name, fileName, flip, int(idx)));
		}
	} else {
		directions.emplace_back(AnimationDirection("default", "default", false, 0));
	}

	for (auto sequenceNode : root["sequences"]) {
		AnimationSequence sequence;
		sequence.name = sequenceNode["name"].as<std::string>("default");
		sequence.fps = sequenceNode["fps"].as<float>(0.0f);
		sequence.loop = sequenceNode["loop"].as<bool>(true);
		sequence.noFlip = sequenceNode["noFlip"].as<bool>(false);
		String fileName = sequenceNode["fileName"].as<std::string>();

		// Load frames
		if (sequenceNode["frames"].IsDefined()) {
			for (auto frameNode : sequenceNode["frames"]) {
				String value = frameNode.as<std::string>();
				Vector<int> values;
				if (value.isInteger()) {
					values.push_back(value.toInteger());
				} else if (value.contains("-")) {
					auto split = value.split('-');
					if (split.size() == 2 && split[0].isInteger() && split[1].isInteger()) {
						int a = split[0].toInteger();
						int b = split[1].toInteger();
						int dir = a < b ? 1 : -1;
						for (int i = a; i != b + dir; i += dir) {
							values.push_back(i);
						}
					} else {
						throw Exception("Invalid frame token: " + value);
					}
				}

				for (int number : values) {
					sequence.frameDefinitions.emplace_back(AnimationFrameDefinition(number, fileName));
				}
			}
		}

		// No frames listed, 
		if (sequence.frameDefinitions.size() == 0) {
			sequence.frameDefinitions.emplace_back(AnimationFrameDefinition(0, fileName));
		}

		animation.sequences.emplace_back(sequence);
	}
}