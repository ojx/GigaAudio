/**
 * Shuffles and plays all WAV audio files on root of attached USB dive
 */

#include <GigaAudio.h>

GigaAudio audio("USB_VOL");  // replace with name of USB volume

void setup() {
    Serial.begin(9600);

    audio.shuffle();

    if (audio.size() == 0) {  // check number of WAV files on root of USB drive
        Serial.println("No WAV files detected");
        return;
    }

    audio.play();

    Serial.print("Playing: ");
    Serial.println(audio.currentFile());

    if (audio.hasError()) Serial.println(audio.errorMessage());
}

void loop() {
    if (audio.isFinished()) {
        audio.next();  // play next file
    }
}