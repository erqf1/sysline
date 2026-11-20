#pragma once
#include <QLibrary>
#include <QString>
#include <memory>

// Werte direkt vom Grafiktreiber (zur Laufzeit geladen, ohne SDK und ohne eigenen Treiber)
struct GpuReading {
    bool ok = false;
    QString name;
    double temp = -1, hotspot = -1, load = -1;  // °C, %
    double vramUsed = -1, vramTotal = -1;       // GiB
};

// NVIDIA: NVML (nvml.dll unter Windows, libnvidia-ml.so.1 unter Linux) - wird mit jedem Treiber installiert
class Nvml {
public:
    ~Nvml();
    bool open();
    GpuReading read();

private:
    QLibrary lib_;
    void* dev_ = nullptr;
    bool tried_ = false, ok_ = false;
    void* fnTemp_ = nullptr;
    void* fnUtil_ = nullptr;
    void* fnMem_ = nullptr;
    void* fnName_ = nullptr;
    void* fnShutdown_ = nullptr;
};

// AMD unter Windows: ADL (atiadlxx.dll, kommt mit dem Adrenalin-Treiber). Liest Temperatur (Rand + Hotspot) und Last.
class Adl {
public:
    Adl();
    ~Adl();
    bool open();
    GpuReading read();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    bool tried_ = false, ok_ = false;
};
