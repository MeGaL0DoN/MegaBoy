#pragma once
#include <filesystem>
#include <span>

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
	SuccessROM,
	SuccessSaveState,
	InvalidROM,
	InvalidBattery,
	CorruptSaveState,
	ROMNotFound,
	FileError
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

	static bool isBootROMValid(std::istream& st, const std::filesystem::path& path);

	static bool isBootROMValid(const std::filesystem::path& path)
	{
		std::ifstream st { path, std::ios::binary | std::ios::in };
		return isBootROMValid(st, path);
	}

	void update(uint32_t cyclesToExecute);
	void stepComponents();

	inline void setDrawCallback(void (*callback)(const uint8_t*, bool))
	{
		this->drawCallback = callback;
	}

	static constexpr std::string_view SAVE_STATE_SIGNATURE = "MegaBoy Emulator Save State";
	static bool isSaveStateFile(std::istream& st);

	FileLoadResult loadFile(std::istream& st, std::filesystem::path filePath, bool loadBatteryOnRomload);

	inline FileLoadResult loadFile(const std::filesystem::path& filePath, bool loadBatteryOnRomload)
	{
		std::ifstream st { filePath, std::ios::in | std::ios::binary };
		if (!st) return FileLoadResult::FileError;
		return loadFile(st, filePath, loadBatteryOnRomload);
	}

	inline void loadCurrentBatterySave() const
	{
		if (!cartridge.hasBattery || !appConfig::batterySaves)
			return;

		if (std::ifstream st { getBatteryFilePath(), std::ios::in | std::ios::binary })
		{
			backupBatteryFile();
			cartridge.getMapper()->loadBattery(st);
		}
	}

	FileLoadResult loadState(const std::filesystem::path& path);
	FileLoadResult loadState(int num);
	bool loadSaveStateThumbnail(const std::filesystem::path& path, uint8_t* framebuffer);

	constexpr bool canSaveStateNow() const { return cartridge.ROMLoaded() && !cpu.isExecutingBootROM(); }

	void saveState(const std::filesystem::path& path) const;
	void saveState(int num);

	inline void saveState(std::ostream& st) const
	{
		if (!canSaveStateNow()) return;
		writeState(st);
	}

	constexpr int getSaveNum() const { return currentSave; }
	constexpr const std::filesystem::path& getSaveStateFolderPath() { return saveStateFolderPath; }
	constexpr const std::filesystem::path& getROMPath() { return romFilePath; }

	inline void setBatterySaveFolder(const std::filesystem::path& path) { customBatterySavePath = path; }

	inline void saveBattery(const std::filesystem::path& path) const
	{
		std::ofstream st { path, std::ios::out | std::ios::binary };
		if (!st) return;
		cartridge.getMapper()->saveBattery(st);
	}
	inline void saveBattery(std::ostream& st) const
	{
		cartridge.getMapper()->saveBattery(st);
	}

	void backupBatteryFile() const;
	void autoSave() const;

	inline void resetRom(bool fullReset)
	{
		if (!cartridge.ROMLoaded()) return;
		if (fullReset) backupBatteryFile();
		reset(fullReset);
		loadBootROM();
	}

	constexpr void enableFastForward(int factor) 
	{ 
		speedFactor = factor; 
		cartridge.timer.fastForwardEnableEvent(factor); 
	}
	constexpr void disableFastForward() 
	{ 
		speedFactor = 1; 
		cartridge.timer.fastForwardDisableEvent();
	}

	bool breakpointHit{ false };
	bool emulationPaused { false };

	std::string gameTitle { };

	MMU mmu { *this };
	CPU cpu { *this };
	std::unique_ptr<PPU> ppu { nullptr };
	APU apu{};
	gbInputManager input { cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
private:
	void (*drawCallback)(const uint8_t* framebuffer, bool firstFrame) { nullptr };
	bool ppuDebugEnable { false };

	uint64_t cycleCounter { 0 };
	int speedFactor { 1 };

	std::filesystem::path saveStateFolderPath;
	int currentSave { 0 };

	std::filesystem::path romFilePath;
	std::filesystem::path customBatterySavePath;;

	std::array<bool, 0x10000> breakpoints {};

	inline void setPPUDebugEnable(bool val)
	{
		ppuDebugEnable = val;
		if (ppu) ppu->setDebugEnable(val);
	}

	inline std::filesystem::path getBatteryFilePath() const
	{
		return customBatterySavePath.empty() ? FileUtils::replaceExtension(romFilePath, ".sav")
											 : customBatterySavePath / "batterySave.sav";
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

	void reset(bool resetBattery, bool clearBuf = true, bool updateSystem = true);

	inline void updatePPUSystem()
	{
		ppu = System::Current() == GBSystem::DMG ? std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::DMG>>(mmu, cpu) } 
												 : std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::GBC>>(mmu, cpu) };

		ppu->setDebugEnable(ppuDebugEnable);
		ppu->drawCallback = this->drawCallback;
	}

	bool loadROM(std::istream& st, const std::filesystem::path& filePath);
	static std::vector<uint8_t> extractZippedROM(std::istream& st);

	static uint64_t calculateHash(std::span<const uint8_t> data);

	void writeState(std::ostream& st) const;
	void writeFrameBuffer(std::ostream& st) const;

	static std::vector<uint8_t> getStateData(std::istream& st);
	FileLoadResult loadState(std::istream& st);
	bool loadFrameBuffer(std::istream& st, uint8_t* framebuffer);
	bool validateAndLoadRom(const std::filesystem::path& romPath, uint8_t checksum);

	void writeGBState(std::ostream& st) const;
	void readGBState(std::istream& st);

	void loadBootROM();
};