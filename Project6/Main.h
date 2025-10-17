#pragma once
#include <string>
#include <windows.h>
#include <vector>
#include <iomanip>
#include <iostream>

extern int portX_1;
extern int portX_2;
extern int portY_1;
extern int portY_2;

extern BYTE PARITY;

extern std::vector<std::vector<BYTE>> g_originalPackets;
extern std::vector<std::vector<BYTE>> g_stuffedPackets;
extern std::vector<std::vector<bool>> g_jamPositions;

std::string workFirst(std::string input);

std::string workSecond(std::string input);