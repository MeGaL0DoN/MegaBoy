#pragma once
#include <filesystem>
#include <mini/ini.h>

#include "MMU.h"
#include "CPU/CPU.h"
#include "PPUCore.h"
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
	static constexpr double FRAME_RATE = 1.0 / 59.7;

	static constexpr uint32_t calculateCycles(double deltaTime) { return static_cast<uint32_t>((CYCLES_PER_FRAME * (deltaTime / FRAME_RATE))); }

	void update(uint32_t cyclesToExecute);
	void stepComponents();

	inline void setDrawCallback(void (*callback)(const uint8_t* buffer))
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

	constexpr int getSaveNum() { return currentSave; }
	constexpr const std::filesystem::path& getSaveFolderPath() { return saveFolderPath; }
	constexpr const std::filesystem::path& getROMPath() { return romFilePath; }

	inline void saveState(const std::filesystem::path& _filePath)
	{
		if (!cartridge.ROMLoaded || cpu.isExecutingBootROM())
			return;

		std::ofstream st(_filePath, std::ios::out | std::ios::binary);
		saveState(st);
	}

	inline void saveBattery(const std::filesystem::path& _filePath)
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

	inline void saveCurrentROM()
	{
		autoSave();
		backupSave(currentSave);
		batteryAutoSave();
	}

	void autoSave();
	void backupSave(int num);

	void batteryAutoSave();

	void reset();
	void restartROM(bool resetBattery = true);

	bool emulationPaused { false };
;	std::string gameTitle { };

	MMU mmu { *this };
	CPU cpu { *this };
	std::unique_ptr<PPU> ppu;
	APU apu{};
	inputManager input { cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
private:
	void (*drawCallback)(const uint8_t* framebuffer) { nullptr };

	std::filesystem::path saveFolderPath;
	std::filesystem::path filePath;
	std::filesystem::path romFilePath;

	int currentSave {0};

	inline std::filesystem::path getSaveFilePath(int saveNum)
	{
		return saveFolderPath / ("save" + std::to_string(saveNum) + ".mbs");
	}

	inline void updateSelectedSaveInfo(int saveStateNum)
	{
		currentSave = saveStateNum;
		appConfig::updateConfigFile();
	}

	inline bool loadROM(std::ifstream& st, const std::filesystem::path& filePath)
	{
		if (cartridge.loadROM(st))
		{
			ppu = System::Current() == GBSystem::DMG ? std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::DMG>>(mmu, cpu) } : 
													   std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::GBC>>(mmu, cpu) } ;
			ppu->drawCallback = this->drawCallback;

			reset();
			romFilePath = filePath;
			currentSave = 0;
			return true;
		}

		return false;
	}

	static constexpr std::string_view SAVE_STATE_SIGNATURE = "MegaBoy Emulator Save State";
	bool isSaveStateFile(std::ifstream& st);

	void saveState(std::ofstream& st);
	bool loadState(std::ifstream& st);
	FileLoadResult loadFile(std::ifstream& st);

	void loadBootROM();
};