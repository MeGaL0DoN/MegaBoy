#pragma once
#include <filesystem>
#include <mini/ini.h>

#include "MMU.h"
#include "CPU/CPU.h"
#include "PPU/PPUCore.h"
#include "APU/APU.h"
#include "inputManager.h"
#include "serialPort.h"
#include "Cartridge.h"
#include "appConfig.h"

enum class FileLoadResult
{
	Success,
	InvalidROM,
	SaveStateROMNotFound
};

class GBCore
{
public:
	static constexpr uint32_t CYCLES_PER_FRAME = 17556 * 4;
	static constexpr uint32_t CYCLES_PER_SECOND = 1048576 * 4;
	static constexpr double FRAME_RATE = static_cast<double>(CYCLES_PER_FRAME) / CYCLES_PER_SECOND;

	constexpr uint64_t totalCycles() const { return cycleCounter; }

	static constexpr uint32_t calculateCycles(double deltaTime) { return static_cast<uint32_t>((CYCLES_PER_FRAME * (deltaTime / FRAME_RATE))); }

	static bool isBootROMValid(const std::filesystem::path& path);

	void update(uint32_t cyclesToExecute);
	void stepComponents();

	inline void setDrawCallback(void (*callback)(const uint8_t*))
	{
		this->drawCallback = callback;
	}

	inline FileLoadResult loadFile(const std::filesystem::path& _filePath)
	{
		std::ifstream st(_filePath, std::ios::in | std::ios::binary);
		this->filePath = _filePath;
		return loadFile(st);
	}

	void loadState(int num);
	void saveState(int num);

	constexpr int getSaveNum() const { return currentSave; }
	constexpr const std::filesystem::path& getSaveFolderPath() { return saveFolderPath; }
	constexpr const std::filesystem::path& getROMPath() { return romFilePath; }

	inline void saveState(const std::filesystem::path& _filePath) const
	{
		if (!cartridge.ROMLoaded || cpu.isExecutingBootROM()) 
			return;

		std::ofstream st(_filePath, std::ios::out | std::ios::binary);
		saveState(st);
	}

	inline void saveBattery(const std::filesystem::path& _filePath) const
	{
		std::ofstream st(_filePath, std::ios::out | std::ios::binary);
		cartridge.getMapper()->saveBattery(st);
	}
	inline void loadBattery()
	{
		if (!cartridge.ROMLoaded || !cartridge.hasBattery) return;
		saveCurrentROM();
		restartROM(false);
	}

	inline void saveCurrentROM() const
	{
		autoSave();
		backupSave(currentSave);
		batteryAutoSave();
	}

	void autoSave() const;
	void batteryAutoSave() const;
	void backupSave(int num) const;

	void reset();
	void restartROM(bool resetBattery = true);

	bool emulationPaused { false };
;	std::string gameTitle { };

	MMU mmu { *this };
	CPU cpu { *this };
	std::unique_ptr<PPU> ppu { nullptr };
	APU apu{};
	inputManager input { cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
private:
	void (*drawCallback)(const uint8_t* framebuffer) { nullptr };

	uint64_t cycleCounter { 0 };

	std::filesystem::path saveFolderPath;
	std::filesystem::path filePath;
	std::filesystem::path romFilePath;

	int currentSave { 0 };

	inline std::filesystem::path getSaveFilePath(int saveNum) const
	{
		return saveFolderPath / ("save" + std::to_string(saveNum) + ".mbs");
	}

	inline void updateSelectedSaveInfo(int saveStateNum)
	{
		currentSave = saveStateNum;
		appConfig::updateConfigFile();
	}

	inline void upddatePPUSystem()
	{
		ppu = System::Current() == GBSystem::DMG ? std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::DMG>>(mmu, cpu) } :
												   std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::GBC>>(mmu, cpu) };
		ppu->drawCallback = this->drawCallback;
	}

	inline bool loadROM(std::ifstream& st, const std::filesystem::path& filePath)
	{
		if (cartridge.loadROM(st))
		{
			upddatePPUSystem();
			reset();
			romFilePath = filePath;
			currentSave = 0;
			return true;
		}

		return false;
	}

	static constexpr std::string_view SAVE_STATE_SIGNATURE = "MegaBoy Emulator Save State";
	static bool isSaveStateFile(std::ifstream& st);

	void saveState(std::ofstream& st) const;
	bool loadState(std::ifstream& st);
	FileLoadResult loadFile(std::ifstream& st);

	void loadBootROM();
};