#include "MyForm.h"
#include <string>
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <bitset>
#include <random>

using namespace System;
using namespace System::Windows::Forms;

int portX_1 = 1;
int portX_2 = 2;
int portY_1 = 3;
int portY_2 = 4;
int n = 1; // номер по списку группы

BYTE PARITY = NOPARITY;

struct HammingDecodeResult {
    BYTE decodedData;
    bool hasSingleError;
    bool hasDoubleError;
    int errorPosition;
};

std::vector<std::string> g_errorLog;

// Глобальные массивы для мониторинга
std::vector<std::vector<BYTE>> g_originalPackets;
std::vector<std::vector<BYTE>> g_stuffedPackets;
std::vector<std::vector<bool>> g_jamPositions;

// Генератор случайных чисел
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dis(0.0, 1.0);

// Функция для проверки занятости канала (70% вероятность)
bool isChannelBusy() {
    return dis(gen) < 0.7;
}

// Функция для проверки коллизии (30% вероятность)
bool shouldCollide() {
    return dis(gen) < 0.5;
}

// Функция для искажения байта (инвертирование случайного бита)
BYTE distortByte(BYTE original) {
    std::uniform_int_distribution<> bitDist(0, 7);
    int bitToFlip = bitDist(gen);
    return original ^ (1 << bitToFlip);
}

// Функция для открытия COM-порта
HANDLE openCOMPort(int portNumber, DWORD baudRate = CBR_115200) {
    std::string portName = "\\\\.\\COM" + std::to_string(portNumber);

    HANDLE hPort = CreateFileA(
        portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPort == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hPort, &dcbSerialParams)) {
        CloseHandle(hPort);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = PARITY;

    if (!SetCommState(hPort, &dcbSerialParams)) {
        CloseHandle(hPort);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 1;
    timeouts.WriteTotalTimeoutMultiplier = 1;

    if (!SetCommTimeouts(hPort, &timeouts)) {
        CloseHandle(hPort);
        return INVALID_HANDLE_VALUE;
    }

    return hPort;
}

// Вычисление контрольных битов для 4 бит данных
BYTE calculateHammingBits(BYTE data4bits) {
    // data4bits содержит 4 бита данных в младших битах
    BYTE d1 = (data4bits >> 0) & 1;
    BYTE d2 = (data4bits >> 1) & 1;
    BYTE d3 = (data4bits >> 2) & 1;
    BYTE d4 = (data4bits >> 3) & 1;

    // Вычисление контрольных битов
    BYTE p1 = d1 ^ d2 ^ d4;
    BYTE p2 = d1 ^ d3 ^ d4;
    BYTE p3 = d2 ^ d3 ^ d4;

    // Формирование 7-битного кода Хэмминга: p1 p2 d1 p3 d2 d3 d4
    BYTE hamming = 0;
    hamming |= (p1 << 6);
    hamming |= (p2 << 5);
    hamming |= (d1 << 4);
    hamming |= (p3 << 3);
    hamming |= (d2 << 2);
    hamming |= (d3 << 1);
    hamming |= (d4 << 0);

    return hamming;
}

// Упрощенная функция декодирования Хэмминга без исправления ошибок
HammingDecodeResult decodeHammingWithDetection(BYTE hamming7bits) {
    HammingDecodeResult result;
    result.hasSingleError = false;
    result.hasDoubleError = false;
    result.errorPosition = -1;

    // Извлечение битов из кода Хэмминга
    BYTE p1 = (hamming7bits >> 6) & 1;
    BYTE p2 = (hamming7bits >> 5) & 1;
    BYTE d1 = (hamming7bits >> 4) & 1;
    BYTE p3 = (hamming7bits >> 3) & 1;
    BYTE d2 = (hamming7bits >> 2) & 1;
    BYTE d3 = (hamming7bits >> 1) & 1;
    BYTE d4 = (hamming7bits >> 0) & 1;

    // Вычисление синдрома ошибки
    BYTE s1 = p1 ^ d1 ^ d2 ^ d4;
    BYTE s2 = p2 ^ d1 ^ d3 ^ d4;
    BYTE s3 = p3 ^ d2 ^ d3 ^ d4;

    BYTE syndrome = (s1 << 2) | (s2 << 1) | s3;

    // Только обнаружение ошибок без исправления
    if (syndrome != 0) {
        result.hasSingleError = true;
        // Не исправляем ошибки, просто отмечаем их наличие
    }

    // Восстановление исходных 4 бит данных
    result.decodedData = 0;
    result.decodedData |= (d1 << 0);
    result.decodedData |= (d2 << 1);
    result.decodedData |= (d3 << 2);
    result.decodedData |= (d4 << 3);

    return result;
}

// Функция для вывода информации о принятых пакетах
void printReceivedPacketsInfo(const std::vector<std::vector<BYTE>>& packets) {
    std::cout << "\nИнформация о принятых пакетах: " << packets.size() << " пакетов" << std::endl;
}

// Упрощенная функция кодирования байта
std::vector<BYTE> hammingEncodeByte(BYTE data, int byteIndex) {
    std::vector<BYTE> result;

    // Разбиваем байт на два 4-битных блока
    BYTE high4bits = (data >> 4) & 0x0F;  // Старшие 4 бита
    BYTE low4bits = data & 0x0F;          // Младшие 4 бита

    // Кодируем каждый 4-битный блок в 7 бит Хэмминга
    BYTE hamming1 = calculateHammingBits(high4bits);
    BYTE hamming2 = calculateHammingBits(low4bits);

    result.push_back(hamming1);
    result.push_back(hamming2);

    return result;
}

// Упрощенная функция декодирования байта
BYTE hammingDecodeByteWithLog(BYTE hamming1, BYTE hamming2, int byteIndex) {
    HammingDecodeResult result1 = decodeHammingWithDetection(hamming1);
    HammingDecodeResult result2 = decodeHammingWithDetection(hamming2);

    std::string logEntry = "[ПРИЕМ] Байт " + std::to_string(byteIndex) + ": ";

    // Анализ блоков
    if (result1.hasSingleError) {
        logEntry += "Ошибка в блоке 1; ";
    }
    if (result2.hasSingleError) {
        logEntry += "Ошибка в блоке 2; ";
    }

    // Восстановление данных
    BYTE resultByte = (result1.decodedData << 4) | result2.decodedData;

    if (result1.hasSingleError || result2.hasSingleError) {
        g_errorLog.push_back(logEntry);
    }

    return resultByte;
}

// Упрощенная функция кодирования данных
std::vector<BYTE> hammingEncode(const std::vector<BYTE>& data) {
    std::vector<BYTE> encoded;

    for (size_t i = 0; i < data.size(); i++) {
        std::vector<BYTE> encodedByte = hammingEncodeByte(data[i], i);
        encoded.insert(encoded.end(), encodedByte.begin(), encodedByte.end());
    }

    return encoded;
}

// Упрощенная функция декодирования данных
std::vector<BYTE> hammingDecode(const std::vector<BYTE>& encoded) {
    std::vector<BYTE> decoded;

    for (size_t i = 0; i < encoded.size(); i += 2) {
        if (i + 1 < encoded.size()) {
            int byteIndex = i / 2;
            BYTE original = hammingDecodeByteWithLog(encoded[i], encoded[i + 1], byteIndex);
            decoded.push_back(original);
        }
    }

    return decoded;
}

// Функция для вывода статистики ошибок
void printErrorStatistics() {
    if (!g_errorLog.empty()) {
        std::cout << "Обнаружено ошибок: " << g_errorLog.size() << std::endl;
    }
}

// Подсчет количества единиц в последовательности байтов
BYTE countOnes(const std::vector<BYTE>& data) {
    int count = 0;
    for (BYTE byte : data) {
        // Используем bitset для подсчета единиц в каждом байте
        std::bitset<8> bits(byte);
        count += bits.count();
    }
    // Ограничиваем значение одним байтом
    return static_cast<BYTE>(count & 0xFF);
}

// Байт-стаффинг с ESC-символами
std::vector<BYTE> byteStuffing(const std::vector<BYTE>& data) {
    std::vector<BYTE> stuffedData;
    const BYTE FLAG1 = '@';       // Первый байт флага
    const BYTE FLAG2 = 'a' + n - 1; // Второй байт флага
    const BYTE ESC = 0x1B;        // ESC-символ

    int i = 0;
    for (BYTE byte : data) {
        // Если байт совпадает с флагом или ESC, заменяем его на ESC-последовательность
        if (i > 1 && (byte == FLAG1 || byte == FLAG2 || byte == ESC)) {
            stuffedData.push_back(ESC);  // ESC-символ
            stuffedData.push_back(byte + 0x80);  // Модифицированный байт (устанавливаем старший бит)
        }
        else {
            stuffedData.push_back(byte);
        }
        i++;
    }

    return stuffedData;
}

// Удаление байт-стаффинга
std::vector<BYTE> byteUnstuffing(const std::vector<BYTE>& stuffedData) {
    std::vector<BYTE> originalData;
    const BYTE ESC = 0x1B; // ESC-символ
    bool escapeNext = false;

    for (BYTE byte : stuffedData) {
        if (escapeNext) {
            // Восстанавливаем оригинальный байт (сбрасываем старший бит)
            originalData.push_back(byte - 0x80);
            escapeNext = false;
        }
        else if (byte == ESC) {
            // Следующий байт - заэскейпленный
            escapeNext = true;
        }
        else {
            // Обычный байт
            originalData.push_back(byte);
        }
    }

    return originalData;
}

// Упрощенная функция создания пакета
std::vector<BYTE> createPacket(int sourcePort, const std::vector<BYTE>& data, int packetIndex, bool isLastPacket) {
    std::vector<BYTE> packet;

    // Flag
    packet.push_back('@');
    packet.push_back('a' + n - 1);

    // Destination Address
    packet.push_back(0x00);

    // Source Address
    packet.push_back(static_cast<BYTE>(sourcePort));

    // Packet Info
    BYTE info = static_cast<BYTE>(packetIndex & 0x7F);
    if (isLastPacket) info |= 0x80;
    packet.push_back(info);

    // Data с кодированием Хэмминга
    size_t dataSize = n + 1;
    std::vector<BYTE> packetData = data;
    packetData.resize(dataSize, 0x00);

    // Кодируем данные кодом Хэмминга
    std::vector<BYTE> encodedData = hammingEncode(packetData);

    // Передаем данные без ошибок
    std::vector<BYTE> cleanData = encodedData;

    packet.insert(packet.end(), cleanData.begin(), cleanData.end());

    // Расчет FCS
    BYTE fcs = countOnes(packet);
    packet.push_back(fcs);

    return packet;
}

// Проверка контрольной суммы пакета
bool verifyFCS(const std::vector<BYTE>& packet) {
    if (packet.size() < 1) return false;

    // Создаем копию пакета без FCS для проверки
    std::vector<BYTE> packetWithoutFCS(packet.begin(), packet.end() - 1);

    // Рассчитываем ожидаемое количество единиц
    BYTE expectedOnes = countOnes(packetWithoutFCS);

    // Извлекаем FCS из пакета
    BYTE receivedOnes = packet[packet.size() - 1];

    return expectedOnes == receivedOnes;
}

// Разбиение данных на пакеты
std::vector<std::vector<BYTE>> splitDataIntoPackets(int sourcePort, const std::string& input) {
    std::vector<std::vector<BYTE>> packets;
    std::vector<BYTE> data(input.begin(), input.end());

    size_t chunkSize = n + 1;
    size_t totalPackets = (data.size() + chunkSize - 1) / chunkSize;

    for (size_t i = 0; i < totalPackets; i++) {
        size_t start = i * chunkSize;
        using std::min;
        size_t end = min(start + chunkSize, data.size());
        std::vector<BYTE> chunk(data.begin() + start, data.begin() + end);
        packets.push_back(createPacket(sourcePort, chunk, i, i == totalPackets - 1));
    }

    if (packets.empty()) {
        packets.push_back(createPacket(sourcePort, std::vector<BYTE>(), 0, true));
    }

    return packets;
}

// Разбор пакета
bool parsePacket(const std::vector<BYTE>& packet, int& sourcePort, std::vector<BYTE>& data, int& packetIndex, bool& isLastPacket) {
    if (packet.size() < 6) return false; // Минимальный размер уменьшен на 1 байт

    // Проверяем flag
    if (packet[0] != '@' || packet[1] != ('a' + n - 1)) return false;

    // Проверяем контрольную сумму
    if (!verifyFCS(packet)) return false;

    sourcePort = packet[3];
    packetIndex = packet[4] & 0x7F;
    isLastPacket = (packet[4] & 0x80) != 0;

    size_t encodedDataSize = (n + 1) * 2;
    if (packet.size() < 5 + encodedDataSize + 1) return false;

    // Извлекаем закодированные данные
    std::vector<BYTE> encodedData(packet.begin() + 5, packet.begin() + 5 + encodedDataSize);

    // Декодируем данные кодом Хэмминга
    data = hammingDecode(encodedData);

    return true;
}

// Модифицированная функция записи байта с обработкой только коллизий
// Модифицированная функция записи байта с обработкой только коллизий
bool writeByteWithCollisionHandling(HANDLE hPort, BYTE byte, int& attemptCount, bool& hadJamBefore) {
    const int MAX_ATTEMPTS = 10;
    const BYTE JAM_SIGNAL = 0xFF;

    attemptCount = 0;
    hadJamBefore = false; // Инициализируем флаг

    // Сначала проверяем занятость канала (без счетчика попыток)
    while (isChannelBusy()) {
        std::cout << "[КАНАЛ] Канал занят, ожидание..." << std::endl;
        Sleep(10); // Ждем перед следующей проверкой
    }

    // Теперь пытаемся отправить байт с обработкой коллизий
    while (attemptCount < MAX_ATTEMPTS) {
        attemptCount++;

        // Проверка коллизии (30% вероятность)
        if (shouldCollide()) {
            std::cout << "[КОЛЛИЗИЯ] Обнаружена коллизия при отправке байта 0x"
                << std::hex << std::setw(2) << std::setfill('0') << (int)byte
                << ", попытка " << attemptCount << "/" << MAX_ATTEMPTS << std::endl;

            // Отправляем искаженный байт
            BYTE distorted = distortByte(byte);
            DWORD written;
            WriteFile(hPort, &distorted, 1, &written, NULL);

            // Отправляем JAM-сигнал
            WriteFile(hPort, &JAM_SIGNAL, 1, &written, NULL);

            std::cout << "[JAM] Отправлен JAM-сигнал 0xFF" << std::endl;

            // Устанавливаем флаг, что перед этим байтом был JAM
            hadJamBefore = true;

            // Ждем перед повторной попыткой
            Sleep(20);
            continue;
        }

        // Успешная отправка без коллизий
        DWORD written;
        if (WriteFile(hPort, &byte, 1, &written, NULL) && written == 1) {
            std::cout << "[УСПЕХ] Байт 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)byte
                << " успешно отправлен с попытки " << attemptCount << std::endl;
            return true;
        }

        // Если произошла ошибка записи (не коллизия), ждем и пробуем снова
        Sleep(10);
    }

    std::cout << "[ОШИБКА] Превышено максимальное количество попыток (" << MAX_ATTEMPTS << ") для байта 0x"
        << std::hex << std::setw(2) << std::setfill('0') << (int)byte << std::endl;
    return false;
}

// Модифицированная функция записи пакетов в порт
// Модифицированная функция записи пакетов в порт
bool writePacketsToPort(HANDLE hPort, const std::vector<std::vector<BYTE>>& originalPackets) {
    g_originalPackets.clear();
    g_stuffedPackets.clear();
    g_jamPositions.clear(); // Очищаем массив позиций JAM
    g_originalPackets = originalPackets;

    for (const auto& packet : originalPackets) {
        // Применяем байт-стаффинг ко всему пакету (включая флаг)
        std::vector<BYTE> stuffed = byteStuffing(packet);
        g_stuffedPackets.push_back(stuffed);

        // Создаем массив для отслеживания позиций JAM в этом пакете
        std::vector<bool> jamFlags(stuffed.size(), false);

        // Отправляем каждый байт с обработкой коллизий
        bool success = true;
        for (size_t i = 0; i < stuffed.size(); i++) {
            int attemptCount;
            bool hadJamBefore = false;
            if (!writeByteWithCollisionHandling(hPort, stuffed[i], attemptCount, hadJamBefore)) {
                success = false;
                break;
            }
            // Сохраняем информацию о JAM перед этим байтом
            jamFlags[i] = hadJamBefore;
        }

        if (!success) {
            return false;
        }

        // Сохраняем информацию о JAM для этого пакета
        g_jamPositions.push_back(jamFlags);

        Sleep(10);
    }

    return true;
}

std::vector<std::vector<BYTE>> readPacketsFromPort(HANDLE hPort, int timeoutMs = 3000) {
    std::vector<std::vector<BYTE>> receivedPackets;
    std::vector<BYTE> buffer;

    DWORD startTime = GetTickCount();
    bool lastPacketReceived = false;

    while ((GetTickCount() - startTime) < timeoutMs && !lastPacketReceived) {
        char chunk[512];
        DWORD bytesRead;

        if (ReadFile(hPort, chunk, sizeof(chunk), &bytesRead, NULL) && bytesRead > 0) {
            // Обрабатываем полученные данные, удаляя JAM-сигналы
            for (DWORD i = 0; i < bytesRead; i++) {
                BYTE currentByte = chunk[i];

                // Пропускаем JAM-сигналы
                if (currentByte == 0xFF) {
                    std::cout << "[ПРИЕМ] Обнаружен JAM-сигнал, пропускаем" << std::endl;
                    buffer.pop_back();
                    continue;
                }

                buffer.push_back(currentByte);
            }

            // Обрабатываем буфер в поиске пакетов
            while (buffer.size() >= 2) {
                // Сначала ищем начало пакета (флаг) в буфере с стаффингом
                bool packetFound = false;
                size_t packetStart = 0;

                // Ищем начало пакета '@' и следующий байт флага
                for (size_t i = 0; i <= buffer.size() - 2; i++) {
                    if (buffer[i] == '@' && buffer[i + 1] == ('a' + n - 1)) {
                        packetStart = i;
                        packetFound = true;
                        break;
                    }
                }

                if (!packetFound) {
                    // Не нашли флаг, удаляем все кроме последнего байта
                    if (buffer.size() > 1) {
                        buffer.erase(buffer.begin(), buffer.end() - 1);
                    }
                    break;
                }

                // Удаляем мусорные данные перед пакетом
                if (packetStart > 0) {
                    buffer.erase(buffer.begin(), buffer.begin() + packetStart);
                }

                // Теперь применяем байт-дестаффинг к потенциальному пакету
                std::vector<BYTE> unstuffedBuffer = byteUnstuffing(buffer);

                // Проверяем, что после дестаффинга у нас все еще есть флаг
                if (unstuffedBuffer.size() >= 2 &&
                    unstuffedBuffer[0] == '@' &&
                    unstuffedBuffer[1] == ('a' + n - 1)) {

                    // Определяем ожидаемый размер пакета
                    size_t expectedSize = 5 + ((n + 1) * 2) + 1;

                    if (unstuffedBuffer.size() >= expectedSize) {
                        // Извлекаем полный пакет
                        std::vector<BYTE> originalPacket(
                            unstuffedBuffer.begin(),
                            unstuffedBuffer.begin() + expectedSize
                        );

                        // Проверяем контрольную сумму
                        if (verifyFCS(originalPacket)) {
                            receivedPackets.push_back(originalPacket);
                            std::cout << "[ПРИЕМ] Успешно принят пакет " << receivedPackets.size()
                                << ", размер: " << originalPacket.size() << " байт" << std::endl;

                            // Проверяем, последний ли это пакет
                            bool isLastPacket = (originalPacket[4] & 0x80) != 0;
                            if (isLastPacket) {
                                lastPacketReceived = true;
                                std::cout << "[ПРИЕМ] Принят последний пакет" << std::endl;
                            }
                        }
                        else {
                            std::cout << "[ПРИЕМ] Ошибка контрольной суммы пакета" << std::endl;
                        }

                        // Вычисляем, сколько байт удалить из оригинального буфера
                        // Нужно найти длину стаффинга для обработанных данных
                        size_t processedStuffedLength = 0;
                        size_t processedUnstuffedLength = 0;
                        const BYTE ESC = 0x1B;

                        while (processedUnstuffedLength < expectedSize && processedStuffedLength < buffer.size()) {
                            if (buffer[processedStuffedLength] == ESC) {
                                processedStuffedLength += 2; // ESC + модифицированный байт
                                processedUnstuffedLength += 1;
                            }
                            else {
                                processedStuffedLength += 1;
                                processedUnstuffedLength += 1;
                            }
                        }

                        // Удаляем обработанные данные из буфера
                        if (processedStuffedLength <= buffer.size()) {
                            buffer.erase(buffer.begin(), buffer.begin() + processedStuffedLength);
                            std::cout << "[ПРИЕМ] Удалено " << processedStuffedLength
                                << " байт из буфера, осталось: " << buffer.size() << std::endl;
                        }
                        else {
                            buffer.clear();
                        }
                    }
                    else {
                        // Не хватает данных для полного пакета
                        break;
                    }
                }
                else {
                    // После дестаффинга флаг исчез - удаляем первый байт и продолжаем поиск
                    buffer.erase(buffer.begin());
                }
            }
        }
        else {
            // Если данных нет, небольшая пауза
            Sleep(10);
        }
    }

    std::cout << "[ПРИЕМ] Завершено. Принято пакетов: " << receivedPackets.size()
        << ", осталось в буфере: " << buffer.size() << " байт" << std::endl;

    return receivedPackets;
}

// Упрощенная функция assembleDataFromPackets
std::string assembleDataFromPackets(const std::vector<std::vector<BYTE>>& packets) {
    std::vector<BYTE> result;

    // Выводим информацию о принятых пакетах
    printReceivedPacketsInfo(packets);

    // Сортируем пакеты по индексу
    std::vector<std::vector<BYTE>> sorted = packets;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return (a[4] & 0x7F) < (b[4] & 0x7F);
        });

    for (const auto& packet : sorted) {
        int source, index;
        bool last;
        std::vector<BYTE> data;

        if (parsePacket(packet, source, data, index, last)) {
            if (last) {
                while (!data.empty() && data.back() == 0) {
                    data.pop_back();
                }
            }
            result.insert(result.end(), data.begin(), data.end());
        }
    }

    // Выводим статистику после сборки всех пакетов
    printErrorStatistics();

    return std::string(result.begin(), result.end());
}

// Модифицированная функция workFirst для очистки лога ошибок
std::string workFirst(std::string input) {
    // Очищаем лог ошибок перед новой передачей
    g_errorLog.clear();

    HANDLE hPortX = INVALID_HANDLE_VALUE;
    HANDLE hPortY = INVALID_HANDLE_VALUE;
    std::string result;

    try {
        hPortX = openCOMPort(portX_1);
        if (hPortX == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open COM" + std::to_string(portX_1));

        hPortY = openCOMPort(portY_2);
        if (hPortY == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open COM" + std::to_string(portY_2));

        // Очистка буфера
        char buf[256];
        DWORD read;
        do {
            ReadFile(hPortY, buf, sizeof(buf), &read, NULL);
        } while (read > 0);

        auto packets = splitDataIntoPackets(portX_1, input);
        if (!writePacketsToPort(hPortX, packets)) {
            throw std::runtime_error("Write failed - превышено количество попыток отправки");
        }

        auto received = readPacketsFromPort(hPortY, 5000);

        if (!received.empty()) {
            result = assembleDataFromPackets(received);
        }
        else {
            result = "Error: No packets received";
        }
    }
    catch (const std::exception& e) {
        result = "Error: " + std::string(e.what());
    }

    if (hPortX != INVALID_HANDLE_VALUE) CloseHandle(hPortX);
    if (hPortY != INVALID_HANDLE_VALUE) CloseHandle(hPortY);

    for (std::vector<BYTE> arr : g_originalPackets) {
        for (BYTE el : arr) {
            std::cout << std::uppercase << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(el) << " ";
        }
        std::cout << "\n";
    }

    for (std::vector<BYTE> arr : g_stuffedPackets) {
        for (BYTE el : arr) {
            std::cout << std::uppercase << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(el) << " ";
        }
        std::cout << "\n";
    }

    return result;
}

// Модифицированная функция workSecond для очистки лога ошибок
std::string workSecond(std::string input) {
    // Очищаем лог ошибок перед новой передачей
    g_errorLog.clear();

    HANDLE hPortX = INVALID_HANDLE_VALUE;
    HANDLE hPortY = INVALID_HANDLE_VALUE;
    std::string result;

    try {
        hPortX = openCOMPort(portY_1);
        if (hPortX == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open COM" + std::to_string(portY_1));

        hPortY = openCOMPort(portX_2);
        if (hPortY == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open COM" + std::to_string(portX_2));

        // Очистка буфера
        char buf[256];
        DWORD read;
        do {
            ReadFile(hPortY, buf, sizeof(buf), &read, NULL);
        } while (read > 0);

        auto packets = splitDataIntoPackets(portY_1, input);
        if (!writePacketsToPort(hPortX, packets)) {
            throw std::runtime_error("Write failed - превышено количество попыток отправки");
        }

        auto received = readPacketsFromPort(hPortY, 5000);

        if (!received.empty()) {
            result = assembleDataFromPackets(received);
        }
        else {
            result = "Error: No packets received";
        }
    }
    catch (const std::exception& e) {
        result = "Error: " + std::string(e.what());
    }

    if (hPortX != INVALID_HANDLE_VALUE) CloseHandle(hPortX);
    if (hPortY != INVALID_HANDLE_VALUE) CloseHandle(hPortY);

    return result;
}

[STAThread]
int main(array<String^>^ args)
{
    // Настройка кодировки консоли для русского языка
    setlocale(LC_ALL, "Russian");
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew Project6::MyForm());
    return 0;
}