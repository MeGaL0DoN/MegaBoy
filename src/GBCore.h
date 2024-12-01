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
	SuccessROM,
	SuccessSaveState,
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

	static constexpr uint32_t calculateCycles(double deltaTime) { return static_cast<uint32_t>((CYCLES_PER_FRAME * (deltaTime / FRAME_RATE))); }

	constexpr uint64_t totalCycles() const { return cycleCounter; }

	static bool isBootROMValid(const std::filesystem::path& path);

	void update(uint32_t cyclesToExecute);
	void stepComponents();

	inline void setDrawCallback(void (*callback)(const uint8_t*, bool))
	{
		this->drawCallback = callback;
	}

	inline FileLoadResult loadFile(const std::filesystem::path& _filePath)
	{
		std::ifstream st(_filePath, std::ios::in | std::ios::binary);
		this->filePath = _filePath;
		return loadFile(st);
	}

	bool loadSaveStateThumbnail(const std::filesystem::path& path, uint8_t* framebuffer);

	void loadState(int num);
	void saveState(int num);

	constexpr int getSaveNum() const { return currentSave; }
	constexpr const std::filesystem::path& getSaveStateFolderPath() { return saveStateFolderPath; }
	constexpr const std::filesystem::path& getROMPath() { return romFilePath; }

	inline void setBatterySaveFolder(const std::filesystem::path& path) { customBatterySavePath = path; }
	inline std::filesystem::path getBatterySaveFolder() const { return customBatterySavePath; }

	constexpr bool canSaveStateNow() const { return cartridge.ROMLoaded() && !cpu.isExecutingBootROM(); }

	inline void saveState(const std::filesystem::path& path) const
	{
		if (!canSaveStateNow()) return;
		std::ofstream st(path, std::ios::out | std::ios::binary);
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
		if (!cartridge.ROMLoaded()) return;
		if (fullReset) backupBatteryFile();
		reset(fullReset);
		loadBootROM();
	}

	constexpr void enableFastForward(int factor) { speedFactor = factor; cartridge.timer.slowDownFactor = factor; }
	constexpr void disableFastForward() { speedFactor = 1; cartridge.timer.slowDownFactor = 1; }

	bool breakpointHit{ false };
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
	void (*drawCallback)(const uint8_t* framebuffer, bool firstFrame) { nullptr };
	bool ppuDebugEnable { false };

	uint64_t cycleCounter { 0 };
	int speedFactor { 1 };

	std::filesystem::path saveStateFolderPath;
	std::filesystem::path filePath;
	std::filesystem::path romFilePath;

	std::filesystem::path customBatterySavePath;

	int currentSave{ 0 };

	std::array<bool, 0x10000> breakpoints {};

	inline void setPPUDebugEnable(bool val)
	{
		ppuDebugEnable = val;
		if (ppu) ppu->setDebugEnable(val);
	}

	inline std::filesystem::path getBatteryFilePath() const
	{
		return customBatterySavePath.empty() ? FileUtils::replaceExtension(romFilePath, ".sav") 
											 : customBatterySavePath / std::to_string(cartridge.getChecksum());
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

	FileLoadResult loadFile(std::istream& st);

	bool loadROMFromStream(std::istream& st);
	bool loadROMFromZipStream(std::istream& st);

	inline bool loadROM(std::istream& is, const std::filesystem::path& filePath)
	{
		auto loadRomFunc = filePath.extension() == ".zip" ? &GBCore::loadROMFromZipStream : &GBCore::loadROMFromStream;

		if ((this->*loadRomFunc)(is))
		{
			romFilePath = filePath;
			loadBootROM();
			return true;
		}

		return false;
	}
	inline void loadBattery(std::istream& st) const
	{
		if (cartridge.hasBattery)
		{
			backupBatteryFile();
			cartridge.getMapper()->loadBattery(st);
		}
	}

	static constexpr std::string_view SAVE_STATE_SIGNATURE = "MegaBoy Emulator Save State";
	static bool isSaveStateFile(std::istream& st);

	void saveState(std::ostream& st) const;
	void saveFrameBuffer(std::ostream& st) const;

	bool loadState(std::istream& st);
	void loadFrameBuffer(std::istream& st, uint8_t* framebuffer);

	void saveGBState(std::ostream& st) const;
	void loadGBState(std::istream& st);

	void loadBootROM();
};