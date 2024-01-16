#include "Arduino.h"
#include "GigaAudio.h"
#include <Arduino_AdvancedAnalog.h>
#include <Arduino_USBHostMbed5.h>
#include <FATFileSystem.h>

GigaAudio::GigaAudio(char* volumeName, uint8_t dacPin) {
  _volume = volumeName;
  _pin = dacPin;
  _fileLoc = nullptr;
  _current = nullptr;
  _playing = false;
  _error = false;
  _finish_flag = false;
  _message = nullptr;
  _scanned = false;
  _file = nullptr;
  _sample_size = 0;
  _samples_count = 0;
  _data_start = 0;
  _frequency = 0;
  _count = 0;
  _msd = nullptr;
  _usb = nullptr;
  _dac0 = nullptr;

  randomSeed(analogRead(A0) * analogRead(A1) * 119 % 1024);
  randomSeed(rand() % 1024);
}

bool GigaAudio::load(char* fileName) {
  char* fileLoc = (char*)malloc(3 + strlen(_volume) + strlen(fileName));
  strcpy(fileLoc, "/");
  strcat(fileLoc, _volume);
  strcat(fileLoc, "/");
  strcat(fileLoc, fileName);

  if (_fileLoc != nullptr && _fileLoc == fileLoc) {
    return true;
  }

  _fileLoc = fileLoc;

  _playing = false;
  _error = false;

  if (!_mount()) return false;

  _file = fopen(_fileLoc, "rb");
  if (_file == nullptr) {
    _error = true;
    char* err = strerror(errno);
    char* msg = "Can't open audio file: ";
    _message = (char*)malloc(strlen(msg) + strlen(err) + strlen(_fileLoc) + 3);
    strcpy(_message, msg);
    strcat(_message, _fileLoc);
    strcat(_message, " (");
    strcat(_message, err);
    strcat(_message, ")");

    return false;
  }

  struct wav_header_t {
    char chunkID[4];              //"RIFF" = 0x46464952
    unsigned long chunkSize;      //28 [+ sizeof(wExtraFormatBytes) + wExtraFormatBytes] + sum(sizeof(chunk.id) + sizeof(chunk.size) + chunk.size)
    char format[4];               //"WAVE" = 0x45564157
    char subchunk1ID[4];          //"fmt " = 0x20746D66
    unsigned long subchunk1Size;  //16 [+ sizeof(wExtraFormatBytes) + wExtraFormatBytes]
    unsigned short audioFormat;
    unsigned short numChannels;
    unsigned long sampleRate;
    unsigned long byteRate;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
  };

  wav_header_t header;
  fread(&header, sizeof(header), 1, _file);

  /* Find the data section of the WAV file. */
  struct chunk_t {
    char ID[4];
    unsigned long size;
  };

  chunk_t chunk;

  /* Find data chunk. */
  while (true) {
    fread(&chunk, sizeof(chunk), 1, _file);
    if (*(unsigned int*)&chunk.ID == 0x61746164)
      break;
    /* Skip chunk data bytes. */
    fseek(_file, chunk.size, SEEK_CUR);
  }

  _data_start = ftell(_file);

  /* Determine number of samples. */
  _sample_size = header.bitsPerSample / 8;
  _samples_count = chunk.size * 8 / header.bitsPerSample;

  /* Configure the advanced DAC. */
  _frequency = header.sampleRate * 2;

  if (!_begin()) return false;

  _error = false;
  _message = nullptr;
  _current = fileName;

  return true;
}

char* GigaAudio::currentFile() {
  return _current;
}


void GigaAudio::play() {
  if (!_scanned) {
    if (!_scan()) return;
  }

  if (_file == nullptr) {
    if (_count > 0) {
      if (!load(getFile(0))) return;
    } else return;
  }

  if (!_playing) {
    _begin();
  }
  _tick();
}

void GigaAudio::pause() {
  if (_playing) {
    _playing = false;
    _dac0->stop();
  }
}

void GigaAudio::stop() {
  if (_playing) {
    _dac0->stop();
    _playing = false;
  }
  fseek(_file, _data_start, SEEK_SET);
}

bool GigaAudio::isFinished() {
  if (_finish_flag && !_playing) {
    _finish_flag = false;
    return true;
  }
  _tick();
  return false;
}


void GigaAudio::update() {
  if (_playing) {
    _tick();
  }
}

void GigaAudio::delay(int ms) {
    int end = ms + millis();

    while (millis() < end) {
        if (_playing) {
            _tick();
        }
    }
}

bool GigaAudio::isPlaying() {
  if (_playing) {
    _tick();
  }
  return _playing;
}

bool GigaAudio::hasError() {
  return _error;
}

char* GigaAudio::errorMessage() {
  return _message;
}

int GigaAudio::size() {
  return _count;
}

char* GigaAudio::getFile(int index) {
  if (index >= 0 && index < _count) {
    std::string str = _files[index];
    char* cstr = new char[str.length() + 1];
    strcpy(cstr, str.c_str());
    return cstr;
  }
  return nullptr;
}

void GigaAudio::next() {
  if (!_scanned) {
    if (!_scan()) return;
  }

  if (_file == nullptr || _count == 1) {
    char* cstr = new char[_files[0].length() + 1];
    strcpy(cstr, _files[0].c_str());
    load(cstr);
    play();
  } else if (_count > 1) {
    for (int i = 0; i < _count; i++) {
      if (_files[i] == _current) {
        char* cstr = new char[_files[(i + 1) % _count].length() + 1];
        strcpy(cstr, _files[(i + 1) % _count].c_str());
        load(cstr);
        play();
        break;
      }
    }
  }
}

void GigaAudio::prev() {
  if (!_scanned) {
    if (!_scan()) return;
  }

  if (_file == nullptr || _count == 1) {
    char* cstr = new char[_files[0].length() + 1];
    strcpy(cstr, _files[0].c_str());
    load(cstr);
    play();
  } else if (_count > 1) {
    for (int i = 0; i < _count; i++) {
      if (_files[i] == _current) {
        if (i == 0) {
          char* cstr = new char[_files[_count - 1].length() + 1];
          strcpy(cstr, _files[_count - 1].c_str());
          load(cstr);
        } else {
          char* cstr = new char[_files[i - 1].length() + 1];
          strcpy(cstr, _files[i - 1].c_str());
          load(cstr);
        }
        play();
        break;
      }
    }
  }
}

void GigaAudio::shuffle() {
  if (!_scanned) {
    if (!_scan()) return;
  }

  if (_count > 1) {
    for (int i = 0; i < _count; i++) {
      int r = rand() % _count;
      std::string tmp = _files[i];
      _files[i] = _files[r];
      _files[r] = tmp;
    }
  }
}


/*********************************************************************/
/************************** PRIVATE: *********************************/
/*********************************************************************/

bool GigaAudio::_scan() {
  if (_scanned) return true;

  _scanned = true;

  char* dirLoc = (char*)malloc(3 + strlen(_volume));
  strcpy(dirLoc, "/");
  strcat(dirLoc, _volume);
  strcat(dirLoc, "/");

  if (!_mount()) return false;

  DIR* d = opendir(dirLoc);

  if (!d) {
    _error = true;
    char* err = strerror(errno);
    char* msg = "Can't scan volume: ";
    _message = (char*)malloc(strlen(msg) + strlen(err) + strlen(dirLoc) + 3);
    strcpy(_message, msg);
    strcat(_message, dirLoc);
    strcat(_message, " (");
    strcat(_message, err);
    strcat(_message, ")");

    return false;
  }

  _count = 0;

  while (true) {
    struct dirent* e = readdir(d);
    if (!e) {
      break;
    }

    if (_count < 100 && _isWAV(e->d_name)) {
      _files[_count] = std::string(e->d_name);
      _count++;
    }
  }

  if (_count == 0) {
    _error = true;
    _message = "No WAV files found on volume";
    return false;
  }

  int errnum = closedir(d);

  if (errno < 0) {
    _error = true;
    char* err = strerror(errnum);
    char* msg = "Error closing volume: ";
    _message = (char*)malloc(strlen(msg) + strlen(err) + strlen(dirLoc) + 3);
    strcpy(_message, msg);
    strcat(_message, dirLoc);
    strcat(_message, " (");
    strcat(_message, err);
    strcat(_message, ")");

    return false;
  }

  return true;
}


bool GigaAudio::_mount() {
  if (_usb == nullptr) {
    pinMode(PA_15, OUTPUT);
    digitalWrite(PA_15, HIGH);

    _dac0 = std::make_shared<AdvancedDAC>(_pin);

    _msd = std::make_shared<USBHostMSD>();
    _msd->init();

    while (!_msd->connect()) {
      delay(50);
    }

    _usb = std::make_shared<mbed::FATFileSystem>(_volume);
    int res = _usb->mount(_msd.get());

    if (res != 0) {
      _error = true;
      _message = "Unable to mount USB Drive";
      return false;
    }
  }

  return true;
}

bool GigaAudio::_begin() {
  if (_fileLoc == nullptr) return false;

  if (!_dac0->begin(AN_RESOLUTION_12, _frequency, 256, 16)) {
    _error = true;
    char* err = strerror(errno);
    char* msg = "Failed to start DAC for audio output: ";
    _message = (char*)malloc(strlen(msg) + strlen(err) + strlen(_fileLoc) + 3);
    strcpy(_message, msg);
    strcat(_message, _fileLoc);
    strcat(_message, " (");
    strcat(_message, err);
    strcat(_message, ")");
    return false;
  }
  _playing = true;
  _finish_flag = false;

  return true;
}

void GigaAudio::_tick() {
  if (_playing && _dac0->available() && !feof(_file)) {
    /* Read data from file. */
    uint16_t sample_data[256] = { 0 };
    fread(sample_data, _sample_size, 256, _file);

    /* Get a free buffer for writing. */
    SampleBuffer buf = _dac0->dequeue();

    /* Write data to buffer. */
    for (size_t i = 0; i < buf.size(); i++) {
      /* Scale down to 12 bit. */
      uint16_t const dac_val = ((static_cast<unsigned int>(sample_data[i]) + 32768) >> 4) & 0x0fff;
      buf[i] = dac_val;
    }

    /* Write the buffer to DAC. */
    _dac0->write(buf);
  } else if (feof(_file)) {
    _finish_flag = true;
    stop();
  }
}

bool GigaAudio::_isWAV(char* fn) {
  std::string filename(fn);
  int idx = filename.rfind('.');

  if (idx != std::string::npos && filename.substr(0, 1) != ".") {
    std::string ext = filename.substr(idx + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext == "wav";
  } else {
    return false;
  }
}
