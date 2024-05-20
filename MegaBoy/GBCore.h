#pragma once
#include <filesystem>
#include "MMU.h"
#include "CPU.h"
#include "PPU.h"
#include "APU.h"
#include "inputManager.h"
#include "serialPort.h"
#include "Cartridge.h"
#include "stringUtils.h"

enum class FileLoadResult
{
	SuccessROM,
	SuccessState,
	InvalidROM,
	SaveStateROMNotFound
};

class GBCore
{
public:
	static constexpr int CYCLES_PER_FRAME = 17556;
	static constexpr double FRAME_RATE = 1.0 / 59.7;

	GBCore();

	static constexpr int getCycles(double deltaTime) { return static_cast<int>((CYCLES_PER_FRAME * (deltaTime / FRAME_RATE))); }

	void update(int cyclesToExecute = CYCLES_PER_FRAME);
	void stepComponents();

	#ifdef  _WIN32

	inline FileLoadResult loadFile(const wchar_t* _filePath)
	{
		std::ifstream st(_filePath, std::ios::in | std::ios::binary);
		this->filePath = StringUtils::ToUTF8(_filePath);
		return loadFile(st);
	}

	#endif

	inline FileLoadResult loadFile(const char* _filePath)
	{
		std::ifstream st(_filePath, std::ios::in | std::ios::binary);
		this->filePath = _filePath;
		return loadFile(st);
	}

	std::string saveFolderName;

	template <typename T>
	inline void saveState(T filePath)
	{
		std::ofstream st(filePath, std::ios::out | std::ios::binary);
		saveState(st);
	}

	template <typename T>
	inline void saveBattery(T filePath)
	{
		std::ofstream st(filePath, std::ios::out | std::ios::binary);
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
		if (cartridge.ROMLoaded)
		{
			autoSave();
			backupSave();
		}
	}

	void autoSave();
	void backupSave();

	void reset();
	void restartROM(bool resetBattery = true);

	bool paused { false };
	bool runBootROM { false };
	std::string gameTitle { };

	MMU mmu { *this };
	CPU cpu { *this };
	PPU ppu{ mmu, cpu };
	APU apu{};
	inputManager input { cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
private:
	std::string romFilePath;
	std::string filePath;

	static constexpr std::string_view SAVE_STATE_SIGNATURE = "MegaBoy Emulator Save State";

	void saveState(std::ofstream& st);
	bool loadState(std::ifstream& st);
	FileLoadResult loadFile(std::ifstream& st);

	void loadBootROM();
};