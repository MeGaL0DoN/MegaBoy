#pragma once
#include <filesystem>

#include "MMU.h"
#include "CPU/CPU.h"
#include "PPU/PPUCore.h"
#include "APU/APU.h"
#include "gbInputManager.h"
#include "serialPort.h"
#include "Cartridge.h"
#include "appConfig.h"
#include "Utils/fileUtils.h"

enum class FileLoadResult
{
	Success,
	InvalidROM,
	SaveStateROMNotFound
};

class GBCore
{
public:
	friend class debugUI;

	static constexpr const char* DMG_BOOTROM_NAME = "dmg_boot.bin";
	static constexpr const char* CGB_BOOTROM_NAME = "cgb_boot.bin";

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
	constexpr const std::filesystem::path& getSaveStateFolderPath() { return saveStateFolderPath; }
	constexpr const std::filesystem::path& getROMPath() { return romFilePath; }

	inline void setBatterySaveFolder(const std::filesystem::path& path) { customBatterySavePath = path; }
	inline std::filesystem::path getBatterySaveFolder() const { return customBatterySavePath; }

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

	void backupBatteryFile() const;
	void autoSave() const;

	inline void resetRom(bool fullReset)
	{
		if (!cartridge.ROMLoaded) return;;
		if (fullReset) backupBatteryFile();
		reset(fullReset);
		loadBootROM();
	}

	constexpr void enableFastForward(int factor) { speedFactor = factor; cartridge.timer.slowDownFactor = factor; }
	constexpr void disableFastForward() { speedFactor = 1; cartridge.timer.slowDownFactor = 1; }

	bool emulationPaused { false };
;	std::string gameTitle { };

	MMU mmu { *this };
	CPU cpu { *this };
	std::unique_ptr<PPU> ppu { nullptr };
	APU apu{};
	gbInputManager input { cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
private:
	void (*drawCallback)(const uint8_t* framebuffer) { nullptr };
	bool ppuDebugEnable { false };

	uint64_t cycleCounter { 0 };
	int speedFactor { 1 };

	std::filesystem::path saveStateFolderPath;
	std::filesystem::path filePath;
	std::filesystem::path romFilePath;

	std::filesystem::path customBatterySavePath;

	int currentSave{ 0 };

	std::array<bool, 0x10000> breakpoints{};
	bool breakpointHit{ false };

	inline void setPPUDebugEnable(bool val)
	{
		ppuDebugEnable = val;
		if (ppu) ppu->setDebugEnable(val);
	}

	inline std::filesystem::path getBatteryFilePath(const std::filesystem::path& romPath) const
	{
		return customBatterySavePath.empty() ? FileUtils::replaceExtension(romPath, ".sav") 
											 : customBatterySavePath / std::to_string(cartridge.checksum);
	}

	inline std::filesystem::path getSaveStateFilePath(int saveNum) const
	{
		return saveStateFolderPath / ("save" + std::to_string(saveNum) + ".mbs");
	}

	inline void updateSelectedSaveInfo(int saveStateNum)
	{
		currentSave = saveStateNum;
		appConfig::updateConfigFile();
	}

	inline void updatePPUSystem()
	{
		ppu = System::Current() == GBSystem::DMG ? std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::DMG>>(mmu, cpu) } :
												   std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::GBC>>(mmu, cpu) };

		ppu->setDebugEnable(ppuDebugEnable);
		ppu->drawCallback = this->drawCallback;
	}

	void reset(bool resetBattery);

	inline bool loadROM(std::ifstream& st, const std::filesystem::path& filePath)
	{
		if (cartridge.loadROM(st))
		{
			updatePPUSystem();
			reset(true);
			romFilePath = filePath;
			currentSave = 0;
			return true;
		}

		return false;
	}

	inline void loadBattery(std::ifstream& st)
	{
		if (cartridge.hasBattery)
		{
			backupBatteryFile();
			cartridge.getMapper()->loadBattery(st);
		}
	}

	static constexpr std::string_view SAVE_STATE_SIGNATURE = "MegaBoy Emulator Save State";
	static bool isSaveStateFile(std::ifstream& st);

	void saveState(std::ofstream& st) const;
	bool loadState(std::ifstream& st);
	FileLoadResult loadFile(std::ifstream& st);

	void loadBootROM();
};