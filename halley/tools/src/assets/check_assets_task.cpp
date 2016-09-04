#include <set>
#include <thread>
#include "halley/tools/assets/check_assets_task.h"
#include "halley/tools/assets/import_assets_task.h"
#include "halley/tools/project/project.h"
#include "halley/tools/assets/import_assets_database.h"
#include "halley/tools/assets/delete_assets_task.h"
#include <boost/filesystem/operations.hpp>
#include "halley/tools/codegen/import_codegen_task.h"

using namespace Halley;
using namespace std::chrono_literals;

CheckAssetsTask::CheckAssetsTask(Project& project, bool headless)
	: EditorTask("Check assets", true, false)
	, project(project)
	, monitorAssets(project.getAssetsSrcPath())
	, monitorSharedAssets(project.getSharedAssetsSrcPath())
	, monitorGen(project.getGenSrcPath())
	, headless(headless)
{}

void CheckAssetsTask::run()
{
	bool first = true;
	while (!isCancelled()) {
		if (first | monitorAssets.poll() | monitorSharedAssets.poll()) { // Don't short-circuit
			checkAllAssets(project.getImportAssetsDatabase(), { project.getAssetsSrcPath(), project.getSharedAssetsSrcPath() }, project.getAssetsPath(), &CheckAssetsTask::importAssets);
		}

		if (first | monitorGen.poll()) {
			checkAllAssets(project.getCodegenDatabase(), { project.getGenSrcPath() }, project.getGenPath(), &CheckAssetsTask::importCodegen);
		}

		first = false;

		do {
			std::this_thread::sleep_for(monitorAssets.hasRealImplementation() ? 25ms : 100ms);
		} while (hasPendingTasks());

		if (headless) {
			return;
		}
	}
}

void CheckAssetsTask::checkAllAssets(ImportAssetsDatabase& db, std::vector<Path> srcPaths, Path dstPath, std::function<EditorTaskAnchor(ImportAssetsDatabase&, const AssetImporter& importer, Path, std::vector<ImportAssetsDatabaseEntry>&&)> importer)
{
	std::map<String, ImportAssetsDatabaseEntry> assets;

	// Enumerate all potential assets
	for (auto srcPath : srcPaths) {
		for (auto filePath : FileSystem::enumerateDirectory(srcPath)) {
			auto& assetImporter = project.getAssetImporter().getImporter(filePath);
			String assetId = assetImporter.getAssetId(filePath);

			auto input = ImportAssetsDatabaseEntry::InputFile(filePath, FileSystem::getLastWriteTime(srcPath / filePath));

			auto iter = assets.find(assetId);
			if (iter == assets.end()) {
				// New; create it
				auto& asset = assets[assetId];
				asset.assetId = assetId;
				asset.assetType = assetImporter.getType();
				asset.srcDir = srcPath;
				asset.inputFiles.push_back(input);
			} else {
				// Already exists
				auto& asset = iter->second;
				if (asset.assetType != assetImporter.getType()) { // Ensure it has the correct type
					throw Exception("AssetId conflict on " + assetId);
				}
				if (asset.srcDir == srcPath) { // Don't mix files from two different source paths
					asset.inputFiles.push_back(input);
				}
			}
		}
	}

	// Check for missing files
	db.markAllAsMissing();
	for (auto& a : assets) {
		db.markAsPresent(a.second);
	}
	auto missing = db.getAllMissing();
	if (!missing.empty()) {
		addPendingTask(EditorTaskAnchor(std::make_unique<DeleteAssetsTask>(db, dstPath, std::move(missing))));
	}

	// Import assets
	auto toImport = filterNeedsImporting(db, assets);
	if (!toImport.empty()) {
		addPendingTask(importer(db, project.getAssetImporter(), dstPath, std::move(toImport)));
	}
}

std::vector<ImportAssetsDatabaseEntry> CheckAssetsTask::filterNeedsImporting(ImportAssetsDatabase& db, const std::map<String, ImportAssetsDatabaseEntry>& assets)
{
	Vector<ImportAssetsDatabaseEntry> toImport;

	for (auto &a : assets) {
		if (db.needsImporting(a.second)) {
			toImport.push_back(a.second);
		}
	}

	return toImport;
}

EditorTaskAnchor CheckAssetsTask::importAssets(ImportAssetsDatabase& db, const AssetImporter& importer, Path dstPath, std::vector<ImportAssetsDatabaseEntry>&& assets)
{
	return EditorTaskAnchor(std::make_unique<ImportAssetsTask>(db, importer, dstPath, std::move(assets)));
}

EditorTaskAnchor CheckAssetsTask::importCodegen(ImportAssetsDatabase& db, const AssetImporter& importer, Path dstPath, std::vector<ImportAssetsDatabaseEntry>&& assets)
{
	return EditorTaskAnchor(std::make_unique<ImportCodegenTask>(db, dstPath, std::move(assets)));
}