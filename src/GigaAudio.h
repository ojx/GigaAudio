/*
  GigaAudio.h - Library for playing WAV files on the Arduino GIGA WiFi R1.
  Created by Owain Jones, December 27, 2023.
  Released into the public domain.
*/
#ifndef GigaAudio_h
#define GigaAudio_h

#include "Arduino.h"
#include <Arduino_AdvancedAnalog.h>
#include <Arduino_USBHostMbed5.h>
#include <FATFileSystem.h>

class GigaAudio
{
  public:
    GigaAudio(char* volumeName, uint8_t dacPin = A12);
    bool load(char* fileName);
    void play();
    void shuffle();
    void pause();
    void stop();
    void next();
    void prev();
    void delay(int ms);
    void update();
    bool isFinished();
    bool isPlaying();
    char* currentFile();
    char* getFile(int index);
    bool hasError();
    char* errorMessage();
    int size();
  private:
    bool _scan();  
    uint8_t _pin;
    char* _fileLoc;
    char* _current;
    char* _volume;
    bool _playing;
    bool _error;
    bool _scanned;
    bool _finish_flag;
    long _data_start;
    int _frequency;
    int _count;
    char* _message;
    FILE* _file;
    int _sample_size;
    int _samples_count;
    std::string _files[100];
    std::shared_ptr<USBHostMSD> _msd;
    std::shared_ptr<mbed::FATFileSystem> _usb;
    std::shared_ptr<AdvancedDAC> _dac0;
    bool _mount();
    void _tick();
    bool _begin();
    bool _isWAV(char* fn);  
};

#endif